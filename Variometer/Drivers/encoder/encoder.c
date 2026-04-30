#include "encoder.h"

/* --- Private State --- */
static QueueHandle_t encoderQueue;
static TIM_HandleTypeDef *encoder_htim;
static volatile uint16_t lastTimerCount = 0;

static volatile uint32_t buttonPressTime = 0;
static volatile bool buttonIsPressed = false;

/* --- Private Helper --- */
static void sendEventFromISR(encoderEventType_t type) {
    if (encoderQueue == NULL) return;

    encoderEvent_t newEvent;
    newEvent.type = type;
    newEvent.delta = 0; // Button events have 0 delta
    newEvent.timestamp = HAL_GetTick();

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(encoderQueue, &newEvent, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* --- Public Functions --- */

void RE_Init(TIM_HandleTypeDef *htim) {
    // 1. Store timer handle and start the hardware encoder interface
    encoder_htim = htim;
    HAL_TIM_Encoder_Start(encoder_htim, TIM_CHANNEL_ALL);
    lastTimerCount = __HAL_TIM_GET_COUNTER(encoder_htim);

    // 2. Create the FreeRTOS Queue for button events
    encoderQueue = xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoderEvent_t));

    // 3. Configure the SW Pin (EXTI - Rising/Falling Edge)
    // (Assumes you enabled the GPIO Clock in main.c or CubeMX already)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = ENCODER_SW_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL; // HW-040 has built-in pullup
    HAL_GPIO_Init(ENCODER_SW_PORT, &GPIO_InitStruct);

    // 4. Configure NVIC for EXTI
    // CRITICAL: Priority must be >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (usually 5)
    HAL_NVIC_SetPriority(ENCODER_SW_EXTI_IRQ, 5, 0);
    HAL_NVIC_EnableIRQ(ENCODER_SW_EXTI_IRQ);
}

bool RE_GetEvent(encoderEvent_t *event, TickType_t xTicksToWait) {
    if (encoderQueue == NULL) return false;

    TickType_t startTick = xTaskGetTickCount();
    TickType_t waitChunk = pdMS_TO_TICKS(RE_POLL_INTERVAL_MS);

    while (1) {
        // --- 1. Check for Hardware Timer Rotations ---
        uint16_t currentCount = __HAL_TIM_GET_COUNTER(encoder_htim);
        
        // Casting to int16_t safely handles timer wrap-around (0 to 65535 or vice-versa)
        int16_t delta = (int16_t)currentCount - (int16_t)lastTimerCount;

        // Standard STM32 Encoder Mode counts both edges of both channels (4 counts per step)
        if (delta >= 4 || delta <= -4) {
            event->type = ENCODER_EVENT_ROTATION;
            event->delta = (delta > 0) ? 1 : -1;
            event->timestamp = HAL_GetTick();
            
            // Advance the tracker by 4, retaining any remainder for fast spins
            lastTimerCount += (event->delta * 4); 
            return true;
        }

        // --- 2. Check for Button Events (Queue) ---
        TickType_t elapsed = xTaskGetTickCount() - startTick;
        TickType_t remaining = (xTicksToWait == portMAX_DELAY) ? portMAX_DELAY : (xTicksToWait - elapsed);
        
        // Block on the queue for either the remaining time OR the 10ms chunk, whichever is shorter
        TickType_t blockTime = (remaining < waitChunk && remaining != portMAX_DELAY) ? remaining : waitChunk;

        if (xQueueReceive(encoderQueue, event, blockTime) == pdPASS) {
            return true; // Click or Long Press received
        }

        // --- 3. Handle Timeouts ---
        if (xTicksToWait != portMAX_DELAY) {
            if ((xTaskGetTickCount() - startTick) >= xTicksToWait) {
                break; // User's requested timeout expired
            }
        }
    }
    
    return false;
}

void RE_EXTI_Callback(uint16_t GPIO_Pin) {
    // --- BUTTON HANDLING EXTI ---
    if (GPIO_Pin == ENCODER_SW_PIN) {
        uint32_t now = HAL_GetTick();
        GPIO_PinState swState = HAL_GPIO_ReadPin(ENCODER_SW_PORT, ENCODER_SW_PIN);

        if (swState == GPIO_PIN_RESET) { // Pressed (Active Low)
            if (!buttonIsPressed) {
                buttonPressTime = now;
                buttonIsPressed = true;
            }
        } 
        else { // Released (Active High)
            if (buttonIsPressed) {
                uint32_t pressDuration = now - buttonPressTime;
                buttonIsPressed = false;

                if (pressDuration >= RE_LONG_PRESS_MS) {
                    sendEventFromISR(ENCODER_EVENT_LONG_PRESS);
                } 
                else if (pressDuration >= RE_CLICK_MS) {
                    sendEventFromISR(ENCODER_EVENT_CLICK);
                }
            }
        }
    }
}
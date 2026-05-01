#include "encoder.h"
#include "FreeRTOS.h"
#include "timers.h" 

// Handle para el software timer que revisará al clk del encoder
static TimerHandle_t encoderTimerHandle = NULL;

// Variables de estado privadas
static QueueHandle_t encoderQueue;
static TIM_HandleTypeDef *encoder_htim;
static volatile uint16_t lastTimerCount = 0;

static volatile uint32_t buttonPressTime = 0;
static volatile bool buttonIsPressed = false;


// Helper
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


// Callback del software timer
static void RE_TimerCallback(TimerHandle_t xTimer) {
    uint16_t currentCount = __HAL_TIM_GET_COUNTER(encoder_htim);
    int16_t delta = (int16_t)currentCount - (int16_t)lastTimerCount;

    // El clk se mueve de a 2 pasos
    if (delta >= 2 || delta <= -2) {
        encoderEvent_t newEvent;
        newEvent.type = ENCODER_EVENT_ROTATION;
        newEvent.delta = (delta > 0) ? 1 : -1;
        newEvent.timestamp = HAL_GetTick();
        
        lastTimerCount += (newEvent.delta * 2); 
        
        // Encolar la rotación
        xQueueSend(encoderQueue, &newEvent, 0); 
    }
}

void RE_Init(TIM_HandleTypeDef *htim) {
    encoder_htim = htim;
    HAL_TIM_Encoder_Start(encoder_htim, TIM_CHANNEL_ALL);
    lastTimerCount = __HAL_TIM_GET_COUNTER(encoder_htim);

    encoderQueue = xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoderEvent_t));

    // Crear e iniciar un software timer periódico
    encoderTimerHandle = xTimerCreate(
        "EncTimer", 
        pdMS_TO_TICKS(RE_POLL_INTERVAL_MS), 
        pdTRUE, // autoReload
        (void *)0, 
        RE_TimerCallback
    );

    RE_Disable_Rotations(); // Rotation readings disabled by default
}

void RE_Enable_Rotations(){
    if (encoderTimerHandle != NULL) {
        xTimerStart(encoderTimerHandle, 0);
        lastTimerCount = __HAL_TIM_GET_COUNTER(encoder_htim);
    }
}

void RE_Disable_Rotations(){
    if (encoderTimerHandle != NULL) {
        xTimerStop(encoderTimerHandle, 0);
    }
}

bool RE_GetEvent(encoderEvent_t *event, TickType_t xTicksToWait) {
    if (encoderQueue == NULL) return false;

    // Solo despertar cuando haya un elemento en la queue (boton o rotacion) o timeout
    if (xQueueReceive(encoderQueue, event, xTicksToWait) == pdPASS) {
        return true;
    }
    
    return false; // Timeout
}

void RE_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    // falling = boton presionado
    if (GPIO_Pin == ENCODER_SW_PIN) {
        if (!buttonIsPressed) {
            buttonPressTime = HAL_GetTick();
            buttonIsPressed = true;
        }
    }
}

void RE_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
    // rising = botón liberado
    if (GPIO_Pin == ENCODER_SW_PIN) {
        if (buttonIsPressed) {
            uint32_t pressDuration = HAL_GetTick() - buttonPressTime;
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
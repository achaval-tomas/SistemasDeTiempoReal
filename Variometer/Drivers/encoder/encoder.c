#include "encoder.h"
#include "cmsis_os.h" // O FreeRTOS.h / task.h dependiendo de tu setup

/* --- Private State --- */
static QueueHandle_t encoderQueue;
static TIM_HandleTypeDef *encoder_htim;
static volatile uint16_t lastTimerCount = 0;

static volatile uint32_t buttonPressTime = 0;
static volatile bool buttonIsPressed = false;

// Nuevo: Manejador para la tarea en background
static TaskHandle_t encoderTaskHandle = NULL; 

/* --- Private Helpers --- */
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

// NUEVO: Tarea en background que revisa el hardware timer
static void RE_PollerTask(void *pvParameters) {
    while (1) {
        // Dormir esta tarea por el intervalo (ej. 10ms)
        vTaskDelay(pdMS_TO_TICKS(RE_POLL_INTERVAL_MS)); 

        uint16_t currentCount = __HAL_TIM_GET_COUNTER(encoder_htim);
        int16_t delta = (int16_t)currentCount - (int16_t)lastTimerCount;

        if (delta >= 4 || delta <= -4) {
            encoderEvent_t newEvent;
            newEvent.type = ENCODER_EVENT_ROTATION;
            newEvent.delta = (delta > 0) ? 1 : -1;
            newEvent.timestamp = HAL_GetTick();
            
            lastTimerCount += (newEvent.delta * 4); 
            
            // Enviar a la cola (usando la función normal, no FromISR)
            // Usamos tiempo de bloqueo 0 para no atascar el poller si la cola se llena
            xQueueSend(encoderQueue, &newEvent, 0); 
        }
    }
}

/* --- Public Functions --- */

void RE_Init(TIM_HandleTypeDef *htim) {
    // 1. Store timer handle and start the hardware encoder interface
    encoder_htim = htim;
    HAL_TIM_Encoder_Start(encoder_htim, TIM_CHANNEL_ALL);
    lastTimerCount = __HAL_TIM_GET_COUNTER(encoder_htim);

    // 2. Create the FreeRTOS Queue for button events
    encoderQueue = xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoderEvent_t));

    // 3. Crear la tarea en background para leer el Timer
    // Asegúrate de darle una prioridad adecuada (ej. Normal o una menos que tu UI)
    xTaskCreate(RE_PollerTask, "EncPoller", 128, NULL, tskIDLE_PRIORITY + 1, &encoderTaskHandle);
}

bool RE_GetEvent(encoderEvent_t *event, TickType_t xTicksToWait) {
    if (encoderQueue == NULL) return false;

    // MAGIA: Ahora esta función simplemente le dice a tu UI Task que se 
    // vaya a dormir. Solo despertará cuando el botón (ISR) o la rotación (PollerTask)
    // envíen algo a esta cola.
    if (xQueueReceive(encoderQueue, event, xTicksToWait) == pdPASS) {
        return true;
    }
    
    return false; // Timeout expirado
}

void RE_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    // --- BUTTON PRESSED (Active Low / Falling Edge) ---
    if (GPIO_Pin == ENCODER_SW_PIN) {
        if (!buttonIsPressed) {
            buttonPressTime = HAL_GetTick();
            buttonIsPressed = true;
        }
    }
}

void RE_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
    // --- BUTTON RELEASED (Active High / Rising Edge) ---
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
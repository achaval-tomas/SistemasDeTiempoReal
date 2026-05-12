#include "encoder.h"
#include "FreeRTOS.h"
#include "stm32h5xx_hal.h"
#include "timers.h" 

// Handle para el software timer que revisará al clk del encoder
static TimerHandle_t encoderTimerHandle = NULL;

// Variables de estado privadas
static QueueHandle_t encoderQueue;
static TIM_HandleTypeDef *encoder_htim;
static volatile uint16_t lastTimerCount = 0;

static volatile uint32_t buttonPressTime = 0;
static volatile bool buttonIsPressed = false;

// Callback del software timer
static void RE_TimerCallback(TimerHandle_t xTimer) {
    uint16_t currentCount = __HAL_TIM_GET_COUNTER(encoder_htim);
    int16_t delta = (int16_t)(currentCount - lastTimerCount);

    // El timer se mueve 2 pasos por cada paso de rotacion fisico
    int16_t steps = delta / 2;

    if (steps != 0) {
        encoderEvent_td newEvent;
        newEvent.type = ENCODER_EVENT_ROTATION;
        newEvent.delta = steps;
        
        lastTimerCount += (steps * 2); 
        
        // Encolar la rotación
        xQueueSend(encoderQueue, &newEvent, 0); 
    }
}

void RE_Init(TIM_HandleTypeDef *htim) {
    encoder_htim = htim;
    HAL_TIM_Encoder_Start(encoder_htim, TIM_CHANNEL_ALL);
    lastTimerCount = __HAL_TIM_GET_COUNTER(encoder_htim);

    encoderQueue = xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoderEvent_td));

    // Crear e iniciar un software timer periódico
    encoderTimerHandle = xTimerCreate(
        "Encoder Timer", 
        pdMS_TO_TICKS(RE_POLL_INTERVAL_MS), 
        pdTRUE, // autoReload
        (void *)0, 
        RE_TimerCallback
    );

    RE_Disable_Rotations(); // Rotation readings DISABLED by default
}

void RE_Enable_Rotations(){
    if (encoderTimerHandle == NULL) return;

    // Start the software timer
    xTimerReset(encoderTimerHandle, 0);
    lastTimerCount = __HAL_TIM_GET_COUNTER(encoder_htim);
}

void RE_Disable_Rotations(){
    if (encoderTimerHandle == NULL) return;

    xTimerStop(encoderTimerHandle, 0);
}

bool RE_GetEvent(encoderEvent_td *event, TickType_t xTicksToWait) {
    if (encoderQueue == NULL) return false;

    // Solo despertar cuando haya un elemento en la queue (boton o rotacion) o timeout
    if (xQueueReceive(encoderQueue, event, xTicksToWait) == pdPASS) {
        return true;
    }
    
    return false; // Timeout
}

// FALLING = PRESIONADO
void RE_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin != ENCODER_SW_PIN) return; 

    if (!buttonIsPressed) {
        buttonPressTime = HAL_GetTick();
        buttonIsPressed = true;
    }

}

// RISING = LIBERADO
void RE_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
    // Return if invalid pin, uninitialized queue or no press detected before release.
    if (GPIO_Pin != ENCODER_SW_PIN) return;
    if (encoderQueue == NULL) return;
    if (!buttonIsPressed) return;
    
    uint32_t pressDuration = HAL_GetTick() - buttonPressTime;
    buttonIsPressed = false;

    // Skip clicks that are too short
    if (pressDuration < RE_CLICK_MS) return;

    encoderEvent_td newEvent;
    newEvent.type = (pressDuration >= RE_LONG_PRESS_MS) ? ENCODER_EVENT_LONG_PRESS : ENCODER_EVENT_CLICK;
    newEvent.delta = 0;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(encoderQueue, &newEvent, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
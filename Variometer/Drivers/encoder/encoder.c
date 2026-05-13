#include "encoder.h"
#include "portmacrocommon.h"
#include "projdefs.h"
#include "stm32h5xx_hal.h"

// Handle para el software timer que revisará al clk del encoder
static TimerHandle_t encoder_periodic_timer = NULL;

// Handle para el one-shot timer de los long-press
static TimerHandle_t encoder_long_press_timer = NULL;

// Variables de estado privadas
static TIM_HandleTypeDef *encoder_htim;
static QueueHandle_t encoder_queue;
static volatile uint16_t lastTimerCount = 0;

static volatile uint32_t buttonPressTime = 0;
static volatile bool buttonIsPressed = false;

// Trigger periodicamente cada RE_POLL_INTERVAL_MS para revisar el contador del timer
static void RE_PeriodicTimerCallback(TimerHandle_t xTimer) {
    uint16_t currentCount = __HAL_TIM_GET_COUNTER(encoder_htim);
    int16_t delta = (int16_t)(currentCount - lastTimerCount);

    // El timer se mueve 2 pasos por cada paso de rotacion fisico
    int16_t steps = delta / 2;

    if (steps != 0) {
        encoderEvent_td newEvent;
        newEvent.type = ENCODER_EVENT_ROTATION;
        newEvent.delta = steps;
        
        lastTimerCount += (steps * 2); 
        
        // Encolar la rotación, non-blocking
        xQueueSend(encoder_queue, &newEvent, 0); 
    }
}

// Trigger cuando el botón se mantiene apretado más de RE_LONG_PRESS_MS
static void RE_LongPressTimerCallback(TimerHandle_t xTimer){
    if (encoder_queue == NULL) return;
    if (!buttonIsPressed) return;
    buttonIsPressed = false;

    encoderEvent_td newEvent = {ENCODER_EVENT_LONG_PRESS, 0};

    // Encolar el long press, non-blocking
    xQueueSend(encoder_queue, &newEvent, 0); 
}

void RE_Init(TIM_HandleTypeDef *htim, QueueHandle_t eventsQueue) {
    encoder_htim = htim;
    HAL_TIM_Encoder_Start(encoder_htim, TIM_CHANNEL_ALL);
    lastTimerCount = __HAL_TIM_GET_COUNTER(encoder_htim);

    encoder_queue = eventsQueue;

    // Software timer periodico para pollear el contador del HW timer
    encoder_periodic_timer = xTimerCreate(
        "rotations Timer", 
        pdMS_TO_TICKS(RE_POLL_INTERVAL_MS), 
        pdTRUE, // autoReload
        (void *)0, 
        RE_PeriodicTimerCallback
    );

    // One-shot timer para detectar long-press del pulsador
    encoder_long_press_timer = xTimerCreate(
        "long press Timer", 
        pdMS_TO_TICKS(RE_LONG_PRESS_MS), 
        pdFALSE, // one-shot
        (void *)0, 
        RE_LongPressTimerCallback
    );

    // Usar RE_Enable_Rotations para habilitar eventos de rotación
}

void RE_Enable_Rotations(){
    if (encoder_periodic_timer == NULL) return;

    // Start the software timer
    xTimerReset(encoder_periodic_timer, 0);
    lastTimerCount = __HAL_TIM_GET_COUNTER(encoder_htim);
}

void RE_Disable_Rotations(){
    if (encoder_periodic_timer == NULL) return;

    xTimerStop(encoder_periodic_timer, 0);
}

// FALLING = PRESIONADO
void RE_EXTI_Falling_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin != ENCODER_SW_PIN) return; 

    if (!buttonIsPressed) {
        buttonPressTime = HAL_GetTick();
        buttonIsPressed = true;

        // Iniciar one-shot timer para detectar long-press
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTimerStartFromISR(encoder_long_press_timer, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }

}

// RISING = LIBERADO
void RE_EXTI_Rising_Callback(uint16_t GPIO_Pin) {
    if (GPIO_Pin != ENCODER_SW_PIN) return;
    if (encoder_queue == NULL) return;
    
    // Si es falso, ya lo manejó el one-shot timer por long-press
    if (!buttonIsPressed) return;
    
    // Matar el one-shot timer que detectaría un long-press
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTimerStopFromISR(encoder_long_press_timer, &xHigherPriorityTaskWoken);
    
    buttonIsPressed = false;
    
    // Solo encolar eventos de clicks >= RE_CLICK_MS
    if ((HAL_GetTick() - buttonPressTime) >= RE_CLICK_MS){
        encoderEvent_td newEvent = {ENCODER_EVENT_CLICK, 0};
        xQueueSendFromISR(encoder_queue, &newEvent, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
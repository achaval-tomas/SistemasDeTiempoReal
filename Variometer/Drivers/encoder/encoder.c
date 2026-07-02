#include "encoder.h"
#include "portmacrocommon.h"
#include "projdefs.h"
#include "stm32h5xx_hal.h"

// Handle para el software timer que revisará al clk del encoder
static TimerHandle_t encoder_PeriodicTimer = NULL;

// Handle para el one-shot timer de los long-press
static TimerHandle_t encoder_LongPressTimer = NULL;
static SemaphoreHandle_t encoder_LongPressToken = NULL;

// Variables de estado privadas
static TIM_HandleTypeDef *encoder_TIM;
static QueueHandle_t encoder_Queue;
static volatile uint16_t encoder_LastTIMCount = 0;
static volatile uint32_t encoder_LastEdgeTick = 0;

// Trigger periodicamente cada RE_POLL_INTERVAL_MS para revisar el contador del timer
static void RE_PeriodicTimerCallback(TimerHandle_t xTimer) {
    uint16_t currentCount = __HAL_TIM_GET_COUNTER(encoder_TIM);
    int16_t delta = (int16_t)(currentCount - encoder_LastTIMCount);
    
    // El timer se mueve 2 pasos por cada paso de rotacion fisico
    int16_t steps = delta / 2;
    
    if (steps != 0) {
        // Encolar el evento de rotación, non-blocking
        encoderEvent_td newEvent = {.type = ENCODER_EVENT_ROTATION, .delta = steps};
        xQueueSend(encoder_Queue, &newEvent, 0);

        // Actualizar LastTIMCount sumando pasos ya procesados
        encoder_LastTIMCount += steps*2;
    } 
}

// Trigger cuando el botón se mantiene apretado más de RE_LONG_PRESS_MS
static void RE_LongPressTimerCallback(TimerHandle_t xTimer){
    if (
        (xSemaphoreTake(encoder_LongPressToken, 0) == pdTRUE)
        &&
        (HAL_GPIO_ReadPin(ENCODER_SW_PORT, ENCODER_SW_PIN) == GPIO_PIN_RESET)
    ) {
        encoderEvent_td newEvent = {.type = ENCODER_EVENT_LONG_PRESS, .delta = 0};
        xQueueSend(encoder_Queue, &newEvent, 0);
    }
}

void RE_Init(TIM_HandleTypeDef *htim, QueueHandle_t eventsQueue) {
    encoder_TIM = htim;
    if (HAL_TIM_Encoder_Start(encoder_TIM, TIM_CHANNEL_ALL) != HAL_OK) {
        Error_Handler();
    }
    encoder_LastTIMCount = __HAL_TIM_GET_COUNTER(encoder_TIM);

    encoder_Queue = eventsQueue;

    // Inicializar mutex para sincronizar EXTI y Timer call-back en situación de long-press
    encoder_LongPressToken = xSemaphoreCreateBinary();

    // Software timer periodico para pollear el contador del HW timer
    encoder_PeriodicTimer = xTimerCreate(
        "rotations Timer", 
        pdMS_TO_TICKS(RE_POLL_INTERVAL_MS), 
        pdTRUE, // autoReload
        (void *)0, 
        RE_PeriodicTimerCallback
    );

    // One-shot timer para detectar long-press del pulsador
    encoder_LongPressTimer = xTimerCreate(
        "long press Timer", 
        pdMS_TO_TICKS(RE_LONG_PRESS_MS), 
        pdFALSE, // one-shot
        (void *)0, 
        RE_LongPressTimerCallback
    );

    // Usar RE_Enable_Rotations para habilitar eventos de rotación
}

void RE_Enable_Rotations(){
    if (encoder_PeriodicTimer == NULL) return;

    // Start the software timer
    xTimerReset(encoder_PeriodicTimer, 0);
    encoder_LastTIMCount = __HAL_TIM_GET_COUNTER(encoder_TIM);
}

void RE_Disable_Rotations(){
    if (encoder_PeriodicTimer == NULL) return;

    xTimerStop(encoder_PeriodicTimer, 0);
}

// FALLING = PRESIONADO
void RE_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != ENCODER_SW_PIN)
        return;

    // Ignorar "bounce"
    uint32_t now = HAL_GetTick();
    if ((now - encoder_LastEdgeTick) < RE_DEBOUNCE_MS)
        return;
    encoder_LastEdgeTick = now;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Habilitar token de un uso para procesar la pulsación
    xSemaphoreGiveFromISR(encoder_LongPressToken, &xHigherPriorityTaskWoken);

    // Disparar one-shot timer para detectar long-press
    xTimerResetFromISR(encoder_LongPressTimer, &xHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// RISING = LIBERADO
void RE_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin != ENCODER_SW_PIN || encoder_Queue == NULL)
        return;

    // Ignorar bounce
    uint32_t now = HAL_GetTick();
    if ((now - encoder_LastEdgeTick) < RE_DEBOUNCE_MS)
        return;
    encoder_LastEdgeTick = now;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (xSemaphoreTakeFromISR(encoder_LongPressToken, &xHigherPriorityTaskWoken) == pdTRUE) {
        // Fue liberado antes que el timer de long-press expirara, por lo que es un click normal
        xTimerStopFromISR(encoder_LongPressTimer, &xHigherPriorityTaskWoken);
        encoderEvent_td newEvent = {.type = ENCODER_EVENT_CLICK, .delta = 0};
        xQueueSendFromISR(encoder_Queue, &newEvent, &xHigherPriorityTaskWoken);
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
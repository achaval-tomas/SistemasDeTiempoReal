#ifndef RE_H // Rotary Encoder (RE)
#define RE_H

#include "FreeRTOS.h"
#include "timers.h" 
#include "queue.h"
#include "stm32h5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

// HW Pin configuration (assuming TIMER pre-configured)
#define ENCODER_SW_PORT      GPIOC
#define ENCODER_SW_PIN       GPIO_PIN_9
#define ENCODER_SW_EXTI_IRQ  EXTI9_IRQn

// Timing configurations
#define RE_CLICK_MS          50     // Debounce / Minimum click time
#define RE_LONG_PRESS_MS     800    // Minimum time for a long press
#define RE_POLL_INTERVAL_MS  50     // How often to check the timer for rotations

// Encoder event types
typedef enum {
    ENCODER_EVENT_NONE = 0,
    ENCODER_EVENT_ROTATION,
    ENCODER_EVENT_CLICK,
    ENCODER_EVENT_LONG_PRESS
} encoderEventType_td;

typedef struct {
    encoderEventType_td type;
    int16_t delta;           // for rotation type, +1 (CW) or -1 (CCW)
} encoderEvent_td;

// Initializes library, needs htim preconfigured in encoder mode and queue handle where
// it will send all encoder events.
void RE_Init(TIM_HandleTypeDef *htim, QueueHandle_t eventsQueue);

// Start/stop periodic updates of encoder position
void RE_Enable_Rotations();

// After rotations are DISABLED, only PRESS events are detected
void RE_Disable_Rotations();

void RE_EXTI_Falling_Callback(uint16_t GPIO_Pin);
void RE_EXTI_Rising_Callback(uint16_t GPIO_Pin);

// Simple helper functions to check event types
static inline bool is_click(encoderEvent_td event){
    return event.type == ENCODER_EVENT_CLICK;
}

static inline bool is_long_press(encoderEvent_td event){
    return event.type == ENCODER_EVENT_LONG_PRESS;
}

static inline bool is_rotation(encoderEvent_td event){
    return event.type == ENCODER_EVENT_ROTATION;
}

#endif /* RE_H */
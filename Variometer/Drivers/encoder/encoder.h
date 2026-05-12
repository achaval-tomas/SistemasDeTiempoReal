#ifndef RE_H // Rotary Encoder (RE)
#define RE_H

#include "stm32h5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* FreeRTOS Includes */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

// HW Pin configuration (assuming TIMER pre-configured)
#define ENCODER_SW_PORT      GPIOC
#define ENCODER_SW_PIN       GPIO_PIN_9
#define ENCODER_SW_EXTI_IRQ  EXTI9_IRQn

// Timing configurations
#define RE_CLICK_MS          50     // Debounce / Minimum click time
#define RE_LONG_PRESS_MS     800    // Minimum time for a long press
#define RE_POLL_INTERVAL_MS  50     // How often to check the timer for rotations

#define ENCODER_QUEUE_SIZE 8

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


/**
 * @brief Initializes the Encoder Library and starts the hardware timer
 * @param htim Pointer to the configured Timer Handle (e.g., &htim2)
 */
void RE_Init(TIM_HandleTypeDef *htim);

/**
 * @brief Fetches the next event (Rotation or Button). 
 * @param event Pointer to structure to populate
 * @param xTicksToWait FreeRTOS timeout
 * @return true if an event occurred, false on timeout
 */
bool RE_GetEvent(encoderEvent_td *event, TickType_t xTicksToWait);

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
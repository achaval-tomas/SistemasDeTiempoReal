#ifndef RE_H // Rotary Encoder (RE)
#define RE_H

#include "stm32h5xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* FreeRTOS Includes */
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/* --- Hardware Configuration --- */
// Note: CLK and DT pins are now managed by your STM32 Timer Alternate Functions!
// We only need to configure the SW (Button) pin here.
#define ENCODER_SW_PORT      GPIOC
#define ENCODER_SW_PIN       GPIO_PIN_9
#define ENCODER_SW_EXTI_IRQ  EXTI9_IRQn

/* --- Timing Thresholds --- */
#define RE_CLICK_MS          50     // Debounce / Minimum click time
#define RE_LONG_PRESS_MS     800    // Minimum time for a long press
#define RE_POLL_INTERVAL_MS  50     // How often to check the timer in background

/* --- Event Definitions --- */
typedef enum {
    ENCODER_EVENT_NONE = 0,
    ENCODER_EVENT_ROTATION,
    ENCODER_EVENT_CLICK,
    ENCODER_EVENT_LONG_PRESS
} encoderEventType_t;

typedef struct {
    encoderEventType_t type;
    int16_t delta;           // +1 (CW) or -1 (CCW)
    uint32_t timestamp;      
} encoderEvent_t;

/* --- Queue Definitions --- */
#define ENCODER_QUEUE_SIZE 8 // Only stores button events now, so it can be smaller

/* --- Public Function Prototypes --- */

/**
 * @brief Initializes the Encoder Library and starts the hardware timer
 * @param htim Pointer to the configured Timer Handle (e.g., &htim2)
 */
void RE_Init(TIM_HandleTypeDef *htim);

/**
 * @brief Fetches the next event (Rotation or Button). 
 * @param event Pointer to structure to populate
 * @param xTicksToWait FreeRTOS timeout (portMAX_DELAY is safe and recommended!)
 * @return true if an event occurred, false on timeout
 */
bool RE_GetEvent(encoderEvent_t *event, TickType_t xTicksToWait);

// Start/stop periodic updates of encoder position
void RE_Enable_Rotations();

// After rotations are DISABLED, only PRESS events are detected
void RE_Disable_Rotations();

void RE_EXTI_Falling_Callback(uint16_t GPIO_Pin);
void RE_EXTI_Rising_Callback(uint16_t GPIO_Pin);

#endif /* RE_H */
#include "my_tasks.h"

typedef struct {
    float pnew;
    float p_prev;
    float climb_rate_filt;
    float dt;
} varioState_td;

static varioState_td state = {0};

/*
 * Processes raw pressure readings into a filtered climb rate.
 */
static void update_climb_rate(varioState_td *state, float raw_pressure) {
    // Low-pass filter pressure
    state->pnew = state->pnew * (1.0f - ALPHA) + raw_pressure * ALPHA;

    // Pressure rate (Pa/s) and conversion to m/s using 12Pa ~ 1m approximation
    // SAFE: state->dt will never be 0 due to main loop logic
    float dp_dt = (state->pnew - state->p_prev) / state->dt;
    state->p_prev = state->pnew;

    float climb_rate = -dp_dt * 0.0833f;

    // Low-pass filter climb rate
    state->climb_rate_filt = state->climb_rate_filt * (1.0f - BETA) + climb_rate * BETA;
}

/*
 * Determines buzzer parameters and loop timing based on climb rate.
 */
static uint32_t process_climb_rate(float climb_rate, buzzerParams_td *buzz) {
    uint32_t next_delay;

    if (climb_rate >= CLIMB_RATE_THRESHOLD) {
        buzz->frequencyHZ = CLIMB_FREQ_BASE + (int)(climb_rate * CLIMB_FREQ_SCALE);
        buzz->durationMS = 80;
        
        uint32_t cadence = 200 - (uint32_t)(climb_rate * 80);
        next_delay = (cadence < 60) ? 60 : cadence;
    } 
    else if (climb_rate <= DESCENT_RATE_THRESHOLD) {
        int freq = DESCENT_FREQ_BASE + (int)(climb_rate * DESCENT_FREQ_SCALE);
        buzz->frequencyHZ = (freq < DESCENT_FREQ_MIN) ? DESCENT_FREQ_MIN : freq;
        buzz->durationMS = 200;
        next_delay = 300;
    } 
    else {
        next_delay = 100;
    }

    return next_delay;
}

void VariometerTask(void *pvParameters) {
    varioState_td state = {0};
    bmp280_td bmp280;
    buzzerQueueData_td buzzMsg = {0};
    displayQueueData_td dispMsg = {0};
    TickType_t lastTick, now;
    uint64_t delayMS = 100;

switched_off:
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Play start up tune and show initializing message on display
    buzzMsg.type = BUZZ_STARTUP;
    xQueueSend(buzzerQueue, &buzzMsg, 0);
    
    dispMsg.type = DISPLAY_ON;
    xQueueSend(displayQueue, &dispMsg, 0);

    // Stabilize initial pressure reading
    float p_init = 0.0f;
    for (int i = 0; i < 30; i++) {
        bmp280_read_data(&bmp280);
        p_init += bmp280.pressure_Pa;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    state.pnew = state.p_prev = p_init / 30.0f;
    state.dt = 0.1f;
    lastTick = xTaskGetTickCount();

    while (1) {
        bmp280_read_data(&bmp280);

        // Update climb rate based on latest pressure reading
        update_climb_rate(&state, bmp280.pressure_Pa);

        // Set buzzer parameters and determine next loop delay based on climb rate
        delayMS = process_climb_rate(state.climb_rate_filt, &buzzMsg.vario);

        // Enqueue buzzer command if thresholds are exceeded
        if (state.climb_rate_filt >= CLIMB_RATE_THRESHOLD || state.climb_rate_filt <= DESCENT_RATE_THRESHOLD) {
            buzzMsg.type = BUZZ_VARIO;
            xQueueSend(buzzerQueue, &buzzMsg, 0);
        }

        // Enqueue display update with the latest data
        dispMsg.type = DISPLAY_UPDATE;
        dispMsg.updateData.sensorData = (bmp280_td){bmp280.temperature_C, state.pnew};
        dispMsg.updateData.climb_rate = state.climb_rate_filt;
        xQueueSend(displayQueue, &dispMsg, 0);

        // Check if user button was pressed to switch off
        if (ulTaskNotifyTake(pdTRUE, 0) != 0) {
            // Send shutdown commands to buzzer and display tasks
            buzzMsg.type = BUZZ_SHUTDOWN;
            xQueueSend(buzzerQueue, &buzzMsg, 0);
            dispMsg.type = DISPLAY_OFF;
            xQueueSend(displayQueue, &dispMsg, 0);

            goto switched_off;
        }

        // Delay according to processed climb_rate
        vTaskDelay(pdMS_TO_TICKS(delayMS));


        // Timing update
        now = xTaskGetTickCount();
        state.dt = (float)(now - lastTick) / (float)configTICK_RATE_HZ;
        lastTick = now;
    }
}
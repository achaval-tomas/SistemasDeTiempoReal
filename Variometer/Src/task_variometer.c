#include "my_tasks.h"

typedef struct {
    float pnew;
    float p_prev;
    float climb_rate_filt;
    float dt;
} varioState_td;

static varioState_td vState = {0};
static bmp280_td bmp280 = {0};

/*
 * Calculates a 30-sample average for a stable initial pressure value
 */
void set_initial_pressure() {
    float p_init = 0.0f;
    for (int i = 0; i < 30; i++) {
        bmp280_read_data(&bmp280);
        p_init += bmp280.pressure_Pa;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vState.pnew = vState.p_prev = p_init / 30.0f;
}


/*
 * Processes raw pressure readings into a filtered climb rate.
 */
static void update_climb_rate(void) {
    // Filter pressure (bigger step if more time has passed)
    float alpha = vState.dt / (varioConfig.tau_p + vState.dt);
    vState.pnew += alpha * (bmp280.pressure_Pa - vState.pnew);

    // Calculate rate of pressure change
    float dp_dt = (vState.pnew - vState.p_prev) / vState.dt;
    vState.p_prev = vState.pnew;

    // Convert to climb rate
    float climb_rate = -dp_dt * 0.083333f;

    // Filter climb rate (bigger step if more time has passed)
    float beta = vState.dt / (varioConfig.tau_c + vState.dt);
    vState.climb_rate_filt += beta * (climb_rate - vState.climb_rate_filt);
}

/*
 * Determines buzzer parameters and loop timing based on climb rate.
 */
static uint32_t get_next_delay(float climb_rate) {
    uint32_t next_delay = 200;

    if (climb_rate >= varioConfig.lift_threshold) {
        uint32_t cadence = 200 - (uint32_t)(climb_rate * 80);
        next_delay = (cadence < 80) ? 80 : cadence;
    } else if (climb_rate <= varioConfig.sink_threshold) {
        next_delay = 300;
    }

    return next_delay;
}

void VariometerTask(void *pvParameters) {
    buzzerQueueData_td buzzMsg = {0};
    displayQueueData_td dispMsg = {0};
    TickType_t lastTick, now;
    uint64_t delayMS = 200;

switched_off:
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Play start up tune and show initializing message on display
    buzzMsg.type = BUZZ_STARTUP;
    xQueueOverwrite(buzzerQueue, &buzzMsg);
    
    dispMsg.type = DISPLAY_ON;
    xQueueOverwrite(displayQueue, &dispMsg);

    // Stabilize initial pressure reading (3 seconds)
    set_initial_pressure();

    // Initial timing setup
    vState.dt = 0.1f;
    lastTick = xTaskGetTickCount();

    while (1) {
        bmp280_read_data(&bmp280);

        // Update climb rate based on latest pressure reading
        update_climb_rate();

        // Set buzzer parameters and determine next loop delay based on climb rate
        delayMS = get_next_delay(vState.climb_rate_filt);

        // Enqueue buzzer command if thresholds are exceeded
        if (vState.climb_rate_filt >= varioConfig.lift_threshold || vState.climb_rate_filt <= varioConfig.sink_threshold) {
            buzzMsg.type = BUZZ_VARIO;
            buzzMsg.vario_climb_rate = vState.climb_rate_filt;
            xQueueOverwrite(buzzerQueue, &buzzMsg);
        }

        // Enqueue display update with the latest data
        dispMsg.type = DISPLAY_UPDATE;
        dispMsg.updateData.sensorData = (bmp280_td){bmp280.temperature_C, vState.pnew};
        dispMsg.updateData.climb_rate = vState.climb_rate_filt;
        xQueueOverwrite(displayQueue, &dispMsg);

        // Check if user button was pressed to switch off
        if (ulTaskNotifyTake(pdTRUE, 0) != 0) {
            // Send shutdown commands to buzzer and display tasks
            buzzMsg.type = BUZZ_SHUTDOWN;
            xQueueOverwrite(buzzerQueue, &buzzMsg);
            dispMsg.type = DISPLAY_OFF;
            xQueueOverwrite(displayQueue, &dispMsg);

            goto switched_off;
        }

        // Delay according to processed climb_rate
        vTaskDelay(pdMS_TO_TICKS(delayMS));


        // Timing update
        now = xTaskGetTickCount();
        vState.dt = (float)(now - lastTick) / (float)configTICK_RATE_HZ;
        lastTick = now;
    }
}
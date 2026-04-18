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
static void update_climb_rate() {
    // Low-pass filter pressure
    vState.pnew = vState.pnew * (1.0f - ALPHA) + bmp280.pressure_Pa * ALPHA;

    // Pressure rate (Pa/s) and conversion to m/s using 12Pa ~ 1m approximation
    // SAFE: vState.dt will never be 0 due to main loop logic
    float dp_dt = (vState.pnew - vState.p_prev) / vState.dt;
    vState.p_prev = vState.pnew;

    float climb_rate = -dp_dt * 0.0833f;

    // Low-pass filter climb rate
    vState.climb_rate_filt = vState.climb_rate_filt * (1.0f - BETA) + climb_rate * BETA;
}

/*
 * Determines buzzer parameters and loop timing based on climb rate.
 */
static uint32_t get_next_delay(float climb_rate) {
    uint32_t next_delay = 100;

    if (climb_rate >= CLIMB_RATE_THRESHOLD) {
        uint32_t cadence = 200 - (uint32_t)(climb_rate * 80);
        next_delay = (cadence < 80) ? 80 : cadence;
    } else if (climb_rate <= DESCENT_RATE_THRESHOLD) {
        next_delay = 300;
    }

    return next_delay;
}

void VariometerTask(void *pvParameters) {
    buzzerQueueData_td buzzMsg = {0};
    displayQueueData_td dispMsg = {0};
    TickType_t lastTick, now;
    uint64_t delayMS = 100;

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
        if (vState.climb_rate_filt >= CLIMB_RATE_THRESHOLD || vState.climb_rate_filt <= DESCENT_RATE_THRESHOLD) {
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
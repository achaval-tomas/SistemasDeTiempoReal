#include "my_tasks.h"
#include "bmp280.h"

typedef struct {
    float pnew;
    float p_prev;
    float climb_rate_filt;
} varioState_td;

varioState_td vState = {0};
bmp280_td bmp280 = {0};

/*
 * Calculates a 30-sample average for a stable initial pressure value
 */
void set_initial_pressure() {
    float p_init = 0.0f;
    for (int i = 0; i < 30; i++) {
        xQueueReceive(varioQueue, &bmp280, portMAX_DELAY);
        p_init += bmp280.pressure_Pa;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vState.pnew = vState.p_prev = p_init / 30.0f;
}


/*
 * Processes raw pressure readings into a filtered climb rate.
 */
void update_vState(void) {
    // Filter pressure
    vState.pnew = (1.0f - varioConfig.alpha) * vState.pnew + varioConfig.alpha * bmp280.pressure_Pa;

    // Calculate rate of pressure change in Pa/s
    float dp_dt = 1000.0f * ((vState.pnew - vState.p_prev) / (float)DT_ms);
    vState.p_prev = vState.pnew;

    // Convert to climb rate in m/s approximating 12Pa ~ 1m
    float climb_rate = -dp_dt * 0.083333f;

    // Filter climb rate
    vState.climb_rate_filt = (1.0f - varioConfig.beta) * vState.climb_rate_filt + varioConfig.beta * climb_rate;
}

void VariometerTask(void *pvParameters) {
    buzzerQueueData_td buzzMsg = {0};
    displayQueueData_td dispMsg = {0};

switched_off:
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    // Play start up tune and show initializing message on display
    buzzMsg.type = BUZZ_STARTUP;
    xQueueOverwrite(buzzerQueue, &buzzMsg);
    
    dispMsg.type = DISPLAY_ON;
    xQueueOverwrite(displayQueue, &dispMsg);

    // Stabilize initial pressure reading (3 seconds)
    set_initial_pressure();

    while (1) {
        // Receive the latest sensor data from the BMP280 task
        // This should be blocking at a fixed rate defined by BMP280Task
        xQueueReceive(varioQueue, &bmp280, portMAX_DELAY);

        // Update climb rate based on pressure reading
        update_vState();

        // Enqueue buzzer command if thresholds are exceeded
        if (vState.climb_rate_filt >= varioConfig.lift_threshold || vState.climb_rate_filt <= varioConfig.sink_threshold) {
            buzzMsg.type = BUZZ_VARIO;
            buzzMsg.vario_climb_rate = vState.climb_rate_filt;
            xQueueOverwrite(buzzerQueue, &buzzMsg);
        }

        // Enqueue display update with the latest data
        dispMsg.type = DISPLAY_VARIO_UPDATE;
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

    }
}
#include "my_tasks.h"
#include "bmp280.h"

// Holds current state and Kalman filter data
typedef struct {
    float pressure;     // pressure in Pa
    float dp_dt;        // pressure rate of change in Pa/s
    float climb_rate;   // clibm/sink in m/s
    float P[2][2];      // Matriz de covarianza de error
    float Q[2];         // Ruido de proceso (ajustable)
    float R;            // Ruido de medición (ajustable)
} varioState_td;

varioState_td vState = {
    .pressure = 0.0f, // must be initialized to an actual pressure reading
    .dp_dt = 0.0f,
    .climb_rate = 0.0f,
    .P =
    {
    {1.0f, 0.0f},
    {0.0f, 1.0f}
    },
    .Q =
    {
        0.01f, // Expected pressure variance
        0.01f  // Expected dp_dt variance
    },
    .R = 1.69f  // Sensor noise variance
};
bmp280_td bmp280 = {0};

void update_vState() {
    static const float dt_s = (float)DT_ms / 1000.0f;

    // Predicción de la próxima presión
    vState.pressure = vState.pressure + (vState.dp_dt * dt_s);
    // Asumimos que dp_dt es constante durante dt_s

    // P = F*P*F' + Q
    float P00 = vState.P[0][0] + dt_s * (vState.P[1][0] + vState.P[0][1] + dt_s * vState.P[1][1]) + vState.Q[0];
    float P01 = vState.P[0][1] + dt_s * vState.P[1][1];
    float P10 = vState.P[1][0] + dt_s * vState.P[1][1];
    float P11 = vState.P[1][1] + vState.Q[1];

    // ACTUALIZACIÓN (Ganancia de Kalman)
    float S = P00 + vState.R;
    float K0 = P00 / S;
    float K1 = P10 / S;

    // Corrección del estado (x = x + K*y)
    float y = bmp280.pressure_Pa - vState.pressure;
    vState.pressure += K0 * y;
    vState.dp_dt += K1 * y;

    // Conversion de Pa/s a m/s aproximando 1m ~ 12 Pa
    vState.climb_rate = -vState.dp_dt * 0.08333f;

    // Corrección de covarianza (P = (I - KH)*P)
    vState.P[0][0] = (1.0f - K0) * P00;
    vState.P[0][1] = (1.0f - K0) * P01;
    vState.P[1][0] = P10 - (K1 * P00);
    vState.P[1][1] = P11 - (K1 * P01);
}


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
    vState.pressure = p_init / 30.0f;
    vState.dp_dt = 0.0f;
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
        if (vState.climb_rate >= varioConfig.lift_threshold || vState.climb_rate <= varioConfig.sink_threshold) {
            buzzMsg.type = BUZZ_VARIO;
            buzzMsg.vario_climb_rate = vState.climb_rate;
            xQueueOverwrite(buzzerQueue, &buzzMsg);
        }

        // Enqueue display update with the latest data
        dispMsg.type = DISPLAY_VARIO_UPDATE;
        dispMsg.updateData.sensorData = (bmp280_td){bmp280.temperature_C, vState.pressure};
        dispMsg.updateData.climb_rate = vState.climb_rate;
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
#include "my_tasks.h"
#include "bmp280.h"
#include "portmacrocommon.h"
#include "projdefs.h"

// Holds current state and Kalman filter data
typedef struct {
    float pressure;     // pressure in Pa
    float dp_dt;        // pressure rate of change in Pa/s
    float climb_rate;   // clibm/sink in m/s
    float P[2][2];      // Matriz de covarianza de error
    float Q[2];         // Ruido de proceso (ajustable)
    float R;            // Ruido de medición (ajustable)
} sensorState_td;

sensorState_td sState = {
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
        0.001f, // Expected pressure variance per step
        1.0f    // Expected dp_dt variance per step
    },
    .R = 1.5f  // Sensor noise variance
};

// Initialize pressure value at 3 second average pressure
void initialize_kalman(){
    bmp280_td bmp280 = {0};
    float pressSum = 0.0f;

    for (uint8_t i = 0; i < 30; ++i){
        bmp280_read_data(&bmp280);
        pressSum += bmp280.pressure_Pa;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    sState.pressure = pressSum / 30;

    // Overwrite filter's sensitivity with user-selected value
    sState.Q[1] = varioConfig.sensitivity;
}

void apply_kalman_filter(float new_pressure) {
    static const float dt_s = (float)SENSOR_DT_MS / 1000.0f;

    // Predicción de la próxima presión
    sState.pressure = sState.pressure + (sState.dp_dt * dt_s);
    // Asumimos que dp_dt es constante durante dt_s

    // P = F*P*F' + Q
    float P00 = sState.P[0][0] + dt_s * (sState.P[1][0] + sState.P[0][1] + dt_s * sState.P[1][1]) + sState.Q[0];
    float P01 = sState.P[0][1] + dt_s * sState.P[1][1];
    float P10 = sState.P[1][0] + dt_s * sState.P[1][1];
    float P11 = sState.P[1][1] + sState.Q[1];

    // ACTUALIZACIÓN (Ganancia de Kalman)
    float S = P00 + sState.R;
    float K0 = P00 / S;
    float K1 = P10 / S;

    // Corrección del estado (x = x + K*y)
    float y = new_pressure - sState.pressure;
    sState.pressure += K0 * y;
    sState.dp_dt += K1 * y;

    // Conversion de Pa/s a m/s aproximando 1m ~ 12 Pa
    sState.climb_rate = -sState.dp_dt * 0.08333f;

    // Corrección de covarianza (P = (I - KH)*P)
    sState.P[0][0] = (1.0f - K0) * P00;
    sState.P[0][1] = (1.0f - K0) * P01;
    sState.P[1][0] = P10 - (K1 * P00);
    sState.P[1][1] = P11 - (K1 * P01);
}

// Update pressure and temperature data in varioQueue at a fixed rate
void BMP280Task(void *pvParameters) {
    bmp280_td bmp280 = {0};
    sensorQueueData_td sensorQueueData = {0};
    buzzerQueueData_td buzzMsg = {0};
    displayQueueData_td dispMsg = {0};
    TickType_t lastTick;
    const TickType_t sensorDelayTicks = pdMS_TO_TICKS(SENSOR_DT_MS);

    bmp280_init((bmp280_settings_td){
      .config = BMP_STANDBY_0_5ms | BMP_FILTER_2,
      .ctrl_meas = BMP_T_OSRS_1 | BMP_P_OSRS_4 | BMP_MODE_NORMAL
    });

sensor_off:
    // Wait until wake-up notification is received from variometer UI
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    
    // Notificar a la tarea del buzzer para que inicie
    buzzMsg.type = BUZZ_STARTUP;
    xQueueOverwrite(buzzerQueue, &buzzMsg);
    
    // Notificar al display que se inició un vuelo
    dispMsg.type = DISPLAY_START_FLIGHT;
    xQueueOverwrite(displayQueue, &dispMsg);

    // Get a stable pressure reading before applying filters
    initialize_kalman();

    lastTick = xTaskGetTickCount();
    
    while (1) {
        bmp280_read_data(&bmp280);
        apply_kalman_filter(bmp280.pressure_Pa);

        sensorQueueData.pressure_Pa = sState.pressure;
        sensorQueueData.temperature_C = bmp280.temperature_C;
        sensorQueueData.climb_rate_mps = sState.climb_rate;
        
        // Encolar los datos a la tarea del buzzer
        buzzMsg.type = BUZZ_VARIO;
        buzzMsg.vario_climb_rate = sensorQueueData.climb_rate_mps;
        xQueueOverwrite(buzzerQueue, &buzzMsg);

        // Encolar los datos a la tarea del display
        dispMsg.type = DISPLAY_UPDATE_VARIO;
        dispMsg.varioData = (genericSensorData_td)sensorQueueData;
        xQueueOverwrite(displayQueue, &dispMsg);

        vTaskDelayUntil(&lastTick, sensorDelayTicks);

        // Revisar si se debe frenar
        if (ulTaskNotifyTake(pdTRUE, 0) != 0) {
            // Notificar al buzzer para que haga el sonido de fin de vuelo
            buzzMsg.type = BUZZ_SHUTDOWN;
            xQueueOverwrite(buzzerQueue, &buzzMsg);

            goto sensor_off;
        }
    }
}
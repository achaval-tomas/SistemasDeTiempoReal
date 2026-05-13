#ifndef _MY_TASKS_H_
#define _MY_TASKS_H_

// Includes for all tasks and shared structures
#include "FreeRTOS.h"
#include "queue.h"

/* ----- General system configuration parameters ----- */
typedef struct {
  float sensitivity; // Coeficiente del filtro de velocidad, mas alto = MAS reacción  

  float lift_threshold; // Umbral de velocidad vertical para inciar sonidos de ascenso en m/s
  float sink_threshold; // Umbral de velocidad vertical para inciar sonidos de descenso en m/s

  float lift_hz_base; // Frecuencia inicial de tono de acsenso en Hz
  float lift_hz_scale; // Aumento de frecuencia por cada 1m/s de ascenso en Hz
  float sink_hz_base; // Frecuencia inicial de tono de descenso en Hz
  float sink_hz_scale; // Aumento de frecuencia por cada 1m/s de descenso en Hz
  float sink_hz_min; // Frecuencia mínima de tono de descenso en Hz

  float sealevel_hPa; // Presion al nivel del mar en hPa

  float takeoff_ASL_m; // Altitud sobre el nivel del mar, en metros, del despegue
} varioConfig_td;

static const varioConfig_td defaultConfig = {
  .sensitivity = 4,
  .lift_threshold = 0.2f,
  .sink_threshold = -0.3f,
  .lift_hz_base = 800,
  .lift_hz_scale = 100,
  .sink_hz_base = 300,
  .sink_hz_scale = 100,
  .sink_hz_min = 100,
  .sealevel_hPa = 1014.0f,
  .takeoff_ASL_m = 330.0f
 };

// Variables shared by all tasks
extern varioConfig_td varioConfig;
extern QueueHandle_t buzzerQueue, displayQueue, encoderEventQueue;

/* ----- Sensor-related structures for communication and task definition ----- */
// Time interval for sensor readings
#define SENSOR_DT_MS 40

typedef struct{
  float pressure_Pa;
  float temperature_C;
  float climb_rate_mps;
} genericSensorData_td;

typedef genericSensorData_td sensorQueueData_td;

/*  Sensor (BMP280) Task
 *  Reads and updates sensor data at a fixed rate (DT_ms)
 *  Keeps the latest data in buzzer and display queues through overwriting.
 */
void BMP280Task(void *pvParameters);


/* ----- Sound-related structures for communication and task definition ----- */
typedef enum {
  BUZZ_VARIO = 0,
  BUZZ_STARTUP = 1,
  BUZZ_SHUTDOWN = 2
} buzzerCommandType_td;

typedef struct {
  buzzerCommandType_td type;
  float vario_climb_rate; // Solo para comandos de tipo BUZZ_VARIO
} buzzerQueueData_td;

/*  Sound (Buzzer) Manager Task
 *  Waits for sound commands from variometer task and plays corresponding tones.
 *  Supports startup, shutdown and variometer feedback tunes.
 */
void BuzzerTask(void *pvParameters);


/* ----- Display-related structures for communication and task definition ----- */
// Time interval for display updates
#define DISPLAY_DT_MS 200

typedef enum {
  DISPLAY_ON,
  DISPLAY_UPDATE_MENU,
  DISPLAY_UPDATE_VARIO,
  DISPLAY_CLEAR,
  DISPLAY_START_FLIGHT,
  DISPLAY_OFF
} displayCommandType_td;

typedef genericSensorData_td displayUpdateData_td;

typedef struct {
  displayCommandType_td type;
  union {
    genericSensorData_td varioData; // Solo para comandos de tipo DISPLAY_UPDATE_VARIO
    struct {
      char lines[4][20];
      uint8_t selectedLine;
    } menuData; // Para comandos de tipo DISPLAY_UPDATE_MENU
  };
} displayQueueData_td;

/*  Display Manager Task
 *  Waits for commands from UI or variometer task and updates LCD accordingly.
 */
void DisplayTask(void *pvParameters);

/*  User Interface Task
 *  Responds to all possible rotary encoder actions from user. Provides a menu interface
 *  from which to set/reset configurations, and an option to start/stop flights.
 */
void UITask(void *pvParameters);

#endif // _MY_TASKS_H_
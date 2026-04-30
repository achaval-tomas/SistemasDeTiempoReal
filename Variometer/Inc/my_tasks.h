#ifndef _MY_TASKS_H_
#define _MY_TASKS_H_

// Includes for all tasks and shared structures
#include "FreeRTOS.h"
#include "queue.h"

extern QueueHandle_t buzzerQueue, displayQueue, varioQueue;

// Time interval for variometer updates in milliseconds
#define SENSOR_DT_MS 20
#define DISPLAY_DT_MS 200

typedef struct{
  float pressure_Pa;
  float temperature_C;
  float climb_rate_mps;
} genericSensorData_td;

/* 
 *  Task that reads and updates sensor data at a fixed rate (DT_ms)
 */
typedef genericSensorData_td sensorQueueData_td;

void BMP280Task(void *pvParameters);

/*
 *  VARIOMETER TASK DEFINITION AND CONFIGURATION
 */

 typedef struct {
  float stability;   // Coeficiente de varianza de presion, mas alto = MENOS estable
  float sensitivity; // Coeficiente del filtro de velocidad, mas alto = MAS reacción  

  float lift_threshold; // Umbral de velocidad vertical para inciar sonidos de ascenso en m/s
  float sink_threshold; // Umbral de velocidad vertical para inciar sonidos de descenso en m/s

  float lift_hz_base; // Frecuencia inicial de tono de acsenso en Hz
  float lift_hz_scale; // Aumento de frecuencia por cada 1m/s de ascenso en Hz
  float sink_hz_base; // Frecuencia inicial de tono de descenso en Hz
  float sink_hz_scale; // Aumento de frecuencia por cada 1m/s de descenso en Hz
  float sink_hz_min; // Frecuencia mínima de tono de descenso en Hz

  float sealevel_pa; // Presion al nivel del mar en pascales
 } varioConfig_td;

static const varioConfig_td defaultConfig = {
  .stability = 0.001f,
  .sensitivity = 0.02f,
  .lift_threshold = 0.2f,
  .sink_threshold = -0.3f,
  .lift_hz_base = 800,
  .lift_hz_scale = 100,
  .sink_hz_base = 300,
  .sink_hz_scale = 100,
  .sink_hz_min = 100,
  .sealevel_pa = 101400.0f
 };

// This configuration is shared by all tasks
static varioConfig_td varioConfig = defaultConfig;

/* MAIN VARIOMETER TASK
 * Switch on/off through user button.
 * Filters and processes pressure data to estimate climb/descent rate.
 * Provides sound feedback through buzzer based on vertical speed.
 * Fully configurable through defined parameters avobe.
 */
void VariometerTask(void *pvParameters);


/*
 *   BUZZER TASK DEFINITION AND COMMUNICATION STRUCTURES
 */

typedef struct {
  uint32_t frequencyHZ;
  uint32_t durationMS;
} buzzerParams_td;

typedef enum {
  BUZZ_VARIO = 0,
  BUZZ_STARTUP = 1,
  BUZZ_SHUTDOWN = 2
} buzzerCommandType_td;

typedef struct {
  buzzerCommandType_td type;
  float vario_climb_rate; // Solo para comandos de tipo BUZZ_VARIO
} buzzerQueueData_td;

/* BUZZER TASK
 * Waits for sound commands from variometer task and plays corresponding tones.
 * Supports startup, shutdown and variometer feedback tunes.
 */
void BuzzerTask(void *pvParameters);


/*
 *   DISPLAY TASK DEFINITION AND COMMUNICATION STRUCTURES
 */
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
  }
} displayQueueData_td;

/* DISPLAY TASK
 * Waits for display commands from variometer task and updates LCD accordingly.
 * Can show altitude and climb rate, as well as handle display on/off and clear commands.
 */
void DisplayTask(void *pvParameters);

#endif // _MY_TASKS_H_
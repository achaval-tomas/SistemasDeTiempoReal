#ifndef _MY_TASKS_H_
#define _MY_TASKS_H_

// Includes for all tasks and shared structures
#include "FreeRTOS.h"
#include "queue.h"

// Include configuration structure and default config
#include "vario_config.h"

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
  BUZZ_SHUTDOWN = 2,
  BUZZ_NEW_VOLUME = 3
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
#define DISPLAY_DT_MS 250

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
      char lines[4][21];
      uint8_t selectedLine;
      uint8_t currentPage;
      uint8_t totalPages;
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
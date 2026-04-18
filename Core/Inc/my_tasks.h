#ifndef _MY_TASKS_H_
#define _MY_TASKS_H_

#include "stm32h5xx_hal.h"
#include "FreeRTOS.h"
#include "bmp280.h"
#include "lcd.h"
#include "queue.h"
#include <stdint.h>
#include <stdio.h>

extern QueueHandle_t buzzerQueue, displayQueue;

/*
 *  VARIOMETER TASK DEFINITION AND CONFIGURATION
 */

// Valor entre 0 y 1 para actualizar valores de presión.
// Mayor alpha = más rápido pero más ruidoso.
#define ALPHA 0.3f

// Valor entre 0 y 1 para actualizar cambios de altitud.
// Mayor beta = más rápido pero más ruidoso.
#define BETA 0.5f

// Umbrales de velocidad vertical para inciar sonidos. (m/s)
#define CLIMB_RATE_THRESHOLD 0.2f
#define DESCENT_RATE_THRESHOLD -0.3f

// Valores en Hz para configurar tonos de ascenso/descenso.
#define CLIMB_FREQ_BASE 720
#define CLIMB_FREQ_SCALE 800

#define DESCENT_FREQ_BASE 300
#define DESCENT_FREQ_SCALE 100
#define DESCENT_FREQ_MIN 150

#define SEA_LEVEL_PRESSURE_PA (float)101400.0f

/* MAIN VARIOMETER TASK
 * Switch on/off through user button.
 * Reads, filters and processes pressure data to estimate climb/descent rate.
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
  buzzerParams_td vario; // Solo para comandos de tipo BUZZ_VARIO
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
  DISPLAY_UPDATE = 0,
  DISPLAY_CLEAR = 1,
  DISPLAY_ON = 2,
  DISPLAY_OFF = 3
} displayCommandType_td;

typedef struct {
  bmp280_td sensorData;
  float climb_rate;
} displayUpdateData_td;

typedef struct {
  displayCommandType_td type;
  displayUpdateData_td updateData; // Solo para comandos de tipo DISPLAY_UPDATE
} displayQueueData_td;

/* DISPLAY TASK
 * Waits for display commands from variometer task and updates LCD accordingly.
 * Can show altitude and climb rate, as well as handle display on/off and clear commands.
 */
void DisplayTask(void *pvParameters);

#endif // _MY_TASKS_H_
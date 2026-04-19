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

 typedef struct {
  float alpha; // Valor entre 0 y 1 para actualizar valores de presión. Mayor alpha = más rápido pero más ruidoso.
  float beta; // Valor entre 0 y 1 para actualizar cambios de altitud. Mayor beta = más rápido pero más ruidoso.

  float lift_threshold; // Umbral de velocidad vertical para inciar sonidos de ascenso en m/s
  float sink_threshold; // Umbral de velocidad vertical para inciar sonidos de descenso en m/s

  uint16_t lift_hz_base; // Frecuencia inicial de tono de acsenso en Hz
  uint16_t lift_hz_scale; // Aumento de frecuencia por cada 0.1m/s de ascenso en Hz
  uint16_t sink_hz_base; // Frecuencia inicial de tono de descenso en Hz
  uint16_t sink_hz_scale; // Aumento de frecuencia por cada 0.1m/s de descenso en Hz
  uint16_t sink_hz_min; // Frecuencia mínima de tono de descenso en Hz

  float sealevel_pa; // Presion al nivel del mar en pascales
 } varioConfig_td;

 static const varioConfig_td defaultConfig = {
  .alpha = 0.3f,
  .beta = 0.5f,
  .lift_threshold = 0.2f,
  .sink_threshold = -0.3f,
  .lift_hz_base = 720,
  .lift_hz_scale = 150,
  .sink_hz_base = 300,
  .sink_hz_scale = 100,
  .sink_hz_min = 100,
  .sealevel_pa = 100930.0f
 };
 static varioConfig_td varioConfig = defaultConfig;

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
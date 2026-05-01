#include "main_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "my_tasks.h"
#include "encoder.h"

#define SENSOR_PRIORITY  5  // Must be able to run at a fixed rate
#define BUZZER_PRIORITY  4  // Must wake up as soon as data is enqueued
#define UI_PRIORITY   3     // Encoder actions more responsive than display
#define DISPLAY_PRIORITY 2  // Response time not as important

TaskHandle_t variometer_task_handle = NULL, sensor_task_handle = NULL;
QueueHandle_t buzzerQueue = NULL, displayQueue = NULL, varioQueue = NULL;

// Inicializar configuración del variometro, después será manejada y leida por los tasks
varioConfig_td varioConfig = {
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

void start_variometer(void){
    // Create queues for task communication, mostly used as mutexes
    buzzerQueue = xQueueCreate(1, sizeof(buzzerQueueData_td));
    displayQueue = xQueueCreate(1, sizeof(displayQueueData_td));
    varioQueue = xQueueCreate(1, sizeof(sensorQueueData_td));

    // Create tasks
    xTaskCreate(
        BMP280Task,
        "bmp280 Task",
        configMINIMAL_STACK_SIZE*4,
        (void*) NULL,
        SENSOR_PRIORITY,
        (void*) &sensor_task_handle
    );

    xTaskCreate(
        UITask,
        "UI Task",
        configMINIMAL_STACK_SIZE*4,
        (void*) NULL,
        UI_PRIORITY,
        (void*) NULL
    );

    xTaskCreate(
        BuzzerTask,
        "Buzzer Task",
        configMINIMAL_STACK_SIZE*2,
        (void*) NULL,
        BUZZER_PRIORITY,
        NULL
    );

    xTaskCreate(
        DisplayTask,
        "Display Task",
        configMINIMAL_STACK_SIZE*2,
        (void*) NULL,
        DISPLAY_PRIORITY,
        NULL
    );
    
    vTaskStartScheduler();
}

// Callback para cuando se SUELTA el botón
void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
    RE_EXTI_Rising_Callback(GPIO_Pin);
}

// Callback para cuando se PRESIONA el botón
void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
    RE_EXTI_Falling_Callback(GPIO_Pin);
}
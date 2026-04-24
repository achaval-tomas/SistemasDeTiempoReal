#include "main_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "my_tasks.h"
#include "button.h"

#define SENSOR_PRIORITY  5  // Must be able to run at a fixed rate
#define BUZZER_PRIORITY  4  // Must wake up as soon as data is enqueued
#define VARIO_PRIORITY   3  // Wakes up when SENSOR sleeps
#define DISPLAY_PRIORITY 2  // Response time not as important

TaskHandle_t variometer_task_handle = NULL;
QueueHandle_t buzzerQueue = NULL, displayQueue = NULL, varioQueue = NULL;

void start_variometer(void){
    // Initialize peripherals
    UserButtonEXTI_Init();

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
        (void*) NULL
    );

    xTaskCreate(
        VariometerTask,
        "Variometer Task",
        configMINIMAL_STACK_SIZE*4,
        (void*) NULL,
        VARIO_PRIORITY,
        (void*) &variometer_task_handle
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
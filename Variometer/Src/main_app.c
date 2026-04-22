#include "main_app.h"
#include "FreeRTOS.h"
#include "bmp280.h"
#include "lcd.h"
#include "task.h"
#include "my_tasks.h"
#include "button.h"

#define SENSOR_PRIORITY 5
#define VARIO_PRIORITY 4
#define BUZZER_PRIORITY 3
#define DISPLAY_PRIORITY 2

TaskHandle_t variometer_task_handle = NULL;
QueueHandle_t buzzerQueue = NULL, displayQueue = NULL, varioQueue = NULL;

void start_variometer(void){
    // Initialize peripherals
    UserButtonEXTI_Init();
    bmp280_init((bmp280_settings_td){
      .config = 0b00001000, // standby 0.5ms, filter x4
      .ctrl_meas = 0b00110011 // temp x1, pressure x8, normal mode
    });
    lcd_init();

    // Create queues for task communication, mostly used as mutexes
    buzzerQueue = xQueueCreate(1, sizeof(buzzerQueueData_td));
    displayQueue = xQueueCreate(1, sizeof(displayQueueData_td));
    varioQueue = xQueueCreate(1, sizeof(bmp280_td));

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
#include "main_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "my_tasks.h"
#include "button.h"

#define VARIO_PRIORITY 4
#define BUZZER_PRIORITY 3
#define DISPLAY_PRIORITY 2

TaskHandle_t variometer_task_handle = NULL;
QueueHandle_t buzzerQueue = NULL, displayQueue = NULL;

void start_variometer(void){
    // Initialize peripherals
    UserButtonEXTI_Init();
    bmp280_init((bmp280_settings_td){
        .config = 0b00010000, // standby 0.5ms, filter x16
        .ctrl_meas = 0b01010111 // temp x2, pressure x16, normal mode
    });
    lcd_init();

    // Create queues for task communication
    buzzerQueue = xQueueCreate(1, sizeof(buzzerQueueData_td));
    displayQueue = xQueueCreate(1, sizeof(displayQueueData_td));

    // Create tasks
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
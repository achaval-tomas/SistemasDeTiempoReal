#include "main_app.h"
#include "FreeRTOS.h"
#include "task.h"
#include "my_tasks.h"
#include "encoder.h"

#define SENSOR_PRIORITY  (configMAX_PRIORITIES - 2)  // Máxima prioridad para asegurar DT constante
#define BUZZER_PRIORITY  (configMAX_PRIORITIES - 3)  // Primera tarea que debe responder a los datos del sensor
#define UI_PRIORITY      (configMAX_PRIORITIES - 4)  // Acciones del encoder más importantes que la actualización de la pantalla
#define DISPLAY_PRIORITY (configMAX_PRIORITIES - 5)  // Menor prioridad, sólo actualizar la pantalla cuando no hay nada más que hacer

TaskHandle_t sensor_task_handle = NULL;
QueueHandle_t buzzerQueue = NULL, displayQueue = NULL, encoderEventQueue = NULL;

// Inicializar configuración del variometro, después será manejada y leida por los tasks
varioConfig_td varioConfig = {0};

void start_variometer(void){
    // Cargar configuración por defecto al iniciar el variometro
    varioConfig = defaultConfig;

    // Crear colas para comunicación entre tareas
    buzzerQueue = xQueueCreate(1, sizeof(buzzerQueueData_td));
    displayQueue = xQueueCreate(1, sizeof(displayQueueData_td));
    encoderEventQueue = xQueueCreate(8, sizeof(encoderEvent_td));

    // Crear tareas del variometro
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

    // Nunca se debería alcanzar este punto
    // Si se alcanza, error handler reseteará el sistema
    Error_Handler();
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
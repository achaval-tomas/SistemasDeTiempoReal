#include "my_tasks.h"
#include "portmacrocommon.h"
#include "encoder.h"
#include "projdefs.h"
#include "stm32h5xx_hal.h"
#include "tim.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Estado general del sistema
typedef enum {
    STATE_MAIN_MENU,
    STATE_EDITING_SETTING,
    STATE_IN_FLIGHT
} systemState_td;

// Tipos de items del menú
typedef enum {
    ITEM_TYPE_ACTION,   // Llama a una función de callback
    ITEM_TYPE_SETTING   // Modifica un valor de configuracion
} itemType_td;

// Tipo de funciones de callback, deben devolver el estado al que transicionan
typedef systemState_td (*menuAction_td)(void); 

typedef enum {
    FLOAT,
    INT
} settingType_td; // para mostrar configuraciones como float o int

// Formato de cada item del menú
typedef struct {
    const char name[20];
    itemType_td type;
    union {
        menuAction_td action;   // Función a ejecutar
        struct {
            settingType_td type; // Para mostrar como float o int, aunque internamente se guardan floats
            float *ptr;          // Puntero a la variable a cambiar
            float step;          // Cuánto aumenta por cada paso del encoder
            float min;
            float max;
        } setting;
    };
} menuItem_td;

// Funciones auxiliares
void update_display(systemState_td state, uint8_t menuIndex);

// Funciones que responden a las acciones del encoder
systemState_td start_flight_action(void);
systemState_td reset_config_action(void);
systemState_td handle_menu_click(const menuItem_td *item);
void update_setting(int16_t delta, const menuItem_td *item);
uint8_t update_index(uint8_t currentIndex, int16_t delta, uint8_t maxItems);

// MENÚ
const menuItem_td menu[] = {
    {"Comenzar vuelo",     ITEM_TYPE_ACTION,  {.action = start_flight_action}},
    {"Volumen",            ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.volume, 1.0f, 0.0f, 5.0f}}},
    {"Sensibilidad",       ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.sensitivity, 1.0f, 1.0f, 10.0f}}},
    {"Ajustar QNH",        ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.sealevel_hPa, 1.0f, 950.0f, 1300.0f}}},
    {"Umbral subida",      ITEM_TYPE_SETTING, {.setting = {FLOAT, &varioConfig.lift_threshold, 0.1f, 0.1f, 5.0f}}},
    {"Umbral bajada",      ITEM_TYPE_SETTING, {.setting = {FLOAT, &varioConfig.sink_threshold, 0.1f, -10.0f, -0.1f}}},
    {"Tono lift (Hz)",     ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.lift_hz_base, 10.0f, 500.0f, 1500.0f}}},
    {"Paso lift (Hz)",     ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.lift_hz_scale, 10.0f, 0.0f, 200.0f}}},
    {"Tono sink (Hz)",     ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.sink_hz_base, 10.0f, 100.0f, 500.0f}}},
    {"Paso sink (Hz)",     ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.sink_hz_scale, 10.0f, 0.0f, 200.0f}}},
    {"Reset config",       ITEM_TYPE_ACTION,  {.action = reset_config_action}}
};

#define MENU_ITEMS_COUNT (sizeof(menu) / sizeof(menu[0]))
#define NO_SELECTION 5

extern TaskHandle_t sensor_task_handle;

void UITask(void *pvParameters) {
    // Inicializar encoder, mandará eventos a encoderEventQueue
    RE_Init(&htim3, encoderEventQueue);

    systemState_td currentState = STATE_MAIN_MENU;
    uint8_t menuIndex = 0;
    const menuItem_td *selectedItem = &menu[0]; // Inicia con el item 0 seleccionado
    encoderEvent_td event = {ENCODER_EVENT_NONE, 0};
    displayQueueData_td dispMsg = {0};

system_OFF:
    // Esperar una pulsacion larga para encenderse
    do xQueueReceive(encoderEventQueue, &event, portMAX_DELAY); while (!is_long_press(event));

    currentState = STATE_MAIN_MENU;
    menuIndex = 0;
    selectedItem = &menu[0];

    // Habilitar la lectura de rotaciones del encoder
    RE_Enable_Rotations();

    // Dar tiempo a que se encienda la pantalla antes de actualizarla
    dispMsg.type = DISPLAY_ON;
    xQueueOverwrite(displayQueue, &dispMsg);
    vTaskDelay(pdMS_TO_TICKS(75));

    update_display(currentState, menuIndex);
    
    while (1) {    
        // Bloquearse hasta recibir evento del encoder
        xQueueReceive(encoderEventQueue, &event, portMAX_DELAY);

        // Lógica de estados
        switch (currentState) {
            
            case STATE_MAIN_MENU:
                if (is_rotation(event)) {
                    menuIndex = update_index(menuIndex, event.delta, MENU_ITEMS_COUNT);

                } else if (is_click(event)) {
                    selectedItem = &menu[menuIndex];
                    currentState = handle_menu_click(selectedItem);

                } else if (is_long_press(event)) {
                    // APAGADO: Apagar display y dejar de leer rotaciones
                    dispMsg.type = DISPLAY_OFF;
                    xQueueOverwrite(displayQueue, &dispMsg);
                    RE_Disable_Rotations();
                    goto system_OFF;
                }

                break;

            case STATE_EDITING_SETTING:
                if (is_rotation(event)) {
                    update_setting(event.delta, selectedItem);
                } else if (is_click(event)) {
                    currentState = STATE_MAIN_MENU; // Volver al menú
                }
                break;

            case STATE_IN_FLIGHT:

                if (is_long_press(event)) {
                    /* 
                        FIN DE VUELO
                        Notificar a la tarea de sensado para que frene
                        Rehabilitar la lectura de rotaciones del encoder
                        Y volver al menú principal
                    */ 
                    xTaskNotifyGive(sensor_task_handle);
                    RE_Enable_Rotations();
                    currentState = STATE_MAIN_MENU;
                    menuIndex = 0;
                }
                break;
        }
        
        // Actualizar pantalla según el estado actual
        update_display(currentState, menuIndex);
    }
    
}

void update_display(systemState_td currentState, uint8_t menuIndex){
    displayQueueData_td dispMsg = {0};
    
    switch (currentState){
        case STATE_IN_FLIGHT:
            // La UI de vuelo se maneja en la tarea de display con datos del sensor 
            break;
        case STATE_MAIN_MENU:
            uint8_t currentPage = menuIndex / 4;
            uint8_t totalPages = (MENU_ITEMS_COUNT + 3) / 4;
            uint8_t firstItem = currentPage * 4;

            dispMsg.type = DISPLAY_UPDATE_MENU;

            for (uint8_t i = 0; i < 4; i++) {
                uint8_t item = firstItem + i;

                if (item < MENU_ITEMS_COUNT) {
                    strncpy(dispMsg.menuData.lines[i],
                            menu[item].name,
                            sizeof(dispMsg.menuData.lines[i]) - 1);
                    dispMsg.menuData.lines[i][sizeof(dispMsg.menuData.lines[i]) - 1] = '\0';
                } else {
                    dispMsg.menuData.lines[i][0] = '\0';
                }
            }

            dispMsg.menuData.selectedLine = menuIndex % 4;
            dispMsg.menuData.currentPage = currentPage + 1;
            dispMsg.menuData.totalPages = totalPages;

            xQueueOverwrite(displayQueue, &dispMsg);
            break;

        case STATE_EDITING_SETTING:
            dispMsg.type = DISPLAY_UPDATE_MENU; 
            
            // Linea 1: Nombre de la configuración
            strncpy(dispMsg.menuData.lines[0], menu[menuIndex].name, sizeof(dispMsg.menuData.lines[0]) - 1);
            dispMsg.menuData.lines[0][sizeof(dispMsg.menuData.lines[0]) - 1] = '\0';
            
            // Formatear el valor para la linea 3, dependiendo de si es float o int
            static char valStr[21];
            if (menu[menuIndex].setting.type == FLOAT) {
                snprintf(valStr, sizeof(valStr), "     > %.2f <    ", *(menu[menuIndex].setting.ptr));
            } else {
                snprintf(valStr, sizeof(valStr), "      > %.0f <    ", *(menu[menuIndex].setting.ptr));
            }
            
            // Linea 3: El valor formateado
            strncpy(dispMsg.menuData.lines[2], valStr, sizeof(dispMsg.menuData.lines[1]) - 1);
            dispMsg.menuData.lines[2][sizeof(dispMsg.menuData.lines[1]) - 1] = '\0';

            // Lineas 2 y 4: En blanco
            dispMsg.menuData.lines[1][0] = '\0';
            dispMsg.menuData.lines[3][0] = '\0';
            
            dispMsg.menuData.selectedLine = NO_SELECTION;
            dispMsg.menuData.totalPages = 0;
            
            xQueueOverwrite(displayQueue, &dispMsg);
            break;
        default:
            break;
    }
}

// Calcula el nuevo índice con wrap-around
uint8_t update_index(uint8_t currentIndex, int16_t delta, uint8_t maxItems) {
    int16_t newIndex = (int16_t)currentIndex + delta;
    
    if (newIndex < 0) {
        newIndex = maxItems - 1;
    } else if (newIndex >= maxItems) {
        newIndex = 0;
    }
    
    return (uint8_t)newIndex;
}

systemState_td handle_menu_click(const menuItem_td *item) {
    switch (item->type){

        case ITEM_TYPE_ACTION:
            if (item->action != NULL) {
                // Ejecuta la accion, devolviendo el estado que la acción requiera
                return item->action(); 
            }
            return STATE_MAIN_MENU;

        case ITEM_TYPE_SETTING:
            return STATE_EDITING_SETTING;
        
        default:
            return STATE_MAIN_MENU;

    }
}

void update_setting(int16_t delta, const menuItem_td *item){
    if (item->type != ITEM_TYPE_SETTING) return;

    float *valPtr = item->setting.ptr;

    // Actualizar el valor de acuerdo a delta*step
    *valPtr += (float)delta * (item->setting.step);

    if (*valPtr > (item->setting.max)) {
        *valPtr = item->setting.max;
    } else if (*valPtr < (item->setting.min)) {
        *valPtr = item->setting.min;
    }

    // Si la configuración cambiada es el volumen, notificar a la tarea del buzzer
    if (valPtr == &(varioConfig.volume)) {
        buzzerQueueData_td bqData = {BUZZ_NEW_VOLUME, 0.0f};
        xQueueSend(buzzerQueue, &bqData, portMAX_DELAY);
    }
}

// Acciones
systemState_td start_flight_action(void) {
    // Frenar la lectura de rotaciones para reducir jitter en estado de vuelo
    RE_Disable_Rotations();

    // Notificar a la tarea de sensado para que comience
    xTaskNotifyGive(sensor_task_handle);

    return STATE_IN_FLIGHT; // Cambiar la UI a estado de vuelo
}

systemState_td reset_config_action(void) {
    // Mostrar por 3 segundos que la configuración fue reestablecida
    displayQueueData_td dispMsg = {0};
    dispMsg.type = DISPLAY_UPDATE_MENU;
    
    strncpy(dispMsg.menuData.lines[0], ">                  <", 21);
    strncpy(dispMsg.menuData.lines[1], ">  Configuracion   <", 21);
    strncpy(dispMsg.menuData.lines[2], ">  Reestablecida   <", 21);
    strncpy(dispMsg.menuData.lines[3], ">                  <", 21);
    dispMsg.menuData.selectedLine = NO_SELECTION;
    dispMsg.menuData.totalPages = 0;

    xQueueOverwrite(displayQueue, &dispMsg);
    vTaskDelay(pdMS_TO_TICKS(3000));

    varioConfig = defaultConfig;
    return STATE_MAIN_MENU; // Al volver, seguir en el menú
}
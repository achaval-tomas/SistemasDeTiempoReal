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

// Typo de funciones de callback, deben devolver el estado al que transicionan
typedef systemState_td (*menuAction_td)(void); 

// Formato de cada item del menú
typedef struct {
    const char name[20];
    itemType_td type;
    union {
        menuAction_td action;   // Función a ejecutar
        struct {
            float *ptr;        // Puntero a la variable a cambiar
            float step;        // Cuánto aumenta por cada paso del encoder
            float min;
            float max;
        } setting;
    };
} menuItem_td;

// TODO: manejar estas comunicaciones
void update_display(systemState_td state, uint8_t menuIndex);
void end_flight(void);

// Funciones que responden a las acciones del encoder
systemState_td start_flight_action(void);
systemState_td reset_config_action(void);
systemState_td handle_menu_click(const menuItem_td *item);
void update_setting(int16_t delta, const menuItem_td *item);
uint8_t update_index(uint8_t currentIndex, int16_t delta, uint8_t maxItems);

// MENÚ
const menuItem_td menu[] = {
    {"Comenzar vuelo",   ITEM_TYPE_ACTION,  {.action = start_flight_action}},
    {"Umbral subida",    ITEM_TYPE_SETTING, {.setting = {&varioConfig.lift_threshold, 0.1f, 0.1f, 5.0f}}},
    {"Umbral bajada",    ITEM_TYPE_SETTING, {.setting = {&varioConfig.sink_threshold, 0.1f, -10.0f, -0.1f}}},
    {"Reset config",     ITEM_TYPE_ACTION,  {.action = reset_config_action}}
};

#define MENU_ITEMS_COUNT (sizeof(menu) / sizeof(menu[0]))

extern TaskHandle_t sensor_task_handle;

void UITask(void *pvParameters) {
    // Inicializar encoder
    RE_Init(&htim3);

    systemState_td currentState = STATE_MAIN_MENU;
    uint8_t menuIndex = 0;
    const menuItem_td *selectedItem = &menu[0]; // Inicia con el item 0 seleccionado
    encoderEvent_t event = {ENCODER_EVENT_NONE, 0, 0};
    displayQueueData_td dispMsg = {0};

system_OFF:
    // Esperar una pulsacion larga para encenderse
    while (event.type != ENCODER_EVENT_LONG_PRESS) {
        RE_GetEvent(&event, portMAX_DELAY);
    }

    dispMsg.type = DISPLAY_ON;
    xQueueOverwrite(displayQueue, &dispMsg);
    // Wait until display is ON
    vTaskDelay(pdMS_TO_TICKS(150));

    update_display(currentState, menuIndex);
    
    for (;;) {    
        // Bloquearse hasta recibir evento del encoder
        if (RE_GetEvent(&event, portMAX_DELAY)) {

            // Lógica de estados
            switch (currentState) {
                
                case STATE_MAIN_MENU:
                    if (event.type == ENCODER_EVENT_ROTATION) {
                        menuIndex = update_index(menuIndex, event.delta, MENU_ITEMS_COUNT);
                    } 
                    else if (event.type == ENCODER_EVENT_CLICK) {
                        selectedItem = &menu[menuIndex];
                        currentState = handle_menu_click(selectedItem);
                    } else if (event.type == ENCODER_EVENT_LONG_PRESS) {
                        dispMsg.type = DISPLAY_OFF;
                        xQueueOverwrite(displayQueue, &dispMsg);

                        // Remove the LONG PRESS type
                        event.type = ENCODER_EVENT_NONE;
                        goto system_OFF;
                    }
                    break;

                case STATE_EDITING_SETTING:
                    if (event.type == ENCODER_EVENT_ROTATION) {
                        update_setting(event.delta, selectedItem);
                    } 
                    else if (event.type == ENCODER_EVENT_CLICK) {
                        currentState = STATE_MAIN_MENU; // Volver al menú
                    }
                    break;

                case STATE_IN_FLIGHT:

                    if (event.type == ENCODER_EVENT_LONG_PRESS) {
                        end_flight();

                        currentState = STATE_MAIN_MENU;
                        menuIndex = 0; // Volver al menú inicial
                    }
                    break;
            }
            
            // Actualizar pantalla según el estado actual
            update_display(currentState, menuIndex);
        }
    }
}

void update_display(systemState_td currentState, uint8_t menuIndex){
    displayQueueData_td dispMsg = {0};
    int8_t max_start;
    uint8_t lineIndex;
    
    switch (currentState){
        case STATE_MAIN_MENU:
            max_start = (int8_t)MENU_ITEMS_COUNT - 2;
            if (max_start < 0) max_start = 0;
            lineIndex = (menuIndex > (uint8_t)max_start) ? (uint8_t)max_start : menuIndex;

            dispMsg.type = DISPLAY_UPDATE_MENU;
            
            // Cargar hasta 4 lineas
            for(int i = 0; i < 4; i++) {
                if ((lineIndex + i) < MENU_ITEMS_COUNT) {
                    strncpy(dispMsg.menuData.lines[i], menu[lineIndex + i].name, sizeof(dispMsg.menuData.lines[i]) - 1);
                    
                    // Forzar caracter nulo al final
                    dispMsg.menuData.lines[i][sizeof(dispMsg.menuData.lines[i]) - 1] = '\0'; 
                } else {
                    // String vacio = caracter nulo al inicio
                    dispMsg.menuData.lines[i][0] = '\0'; 
                }
            }

            dispMsg.menuData.selectedLine = menuIndex - lineIndex;
            xQueueOverwrite(displayQueue, &dispMsg);
            break;

        case STATE_EDITING_SETTING:
            dispMsg.type = DISPLAY_UPDATE_MENU; 
            
            // Line 1: Nombre de la configuración
            strncpy(dispMsg.menuData.lines[0], menu[menuIndex].name, sizeof(dispMsg.menuData.lines[0]) - 1);
            dispMsg.menuData.lines[0][sizeof(dispMsg.menuData.lines[0]) - 1] = '\0';
            
            // Format the current float value into a string for line 2
            static char valStr[20];
            snprintf(valStr, sizeof(valStr), "> %.2f <", *(menu[menuIndex].setting.ptr));
            
            // Line 2: El valor formateado
            strncpy(dispMsg.menuData.lines[1], valStr, sizeof(dispMsg.menuData.lines[1]) - 1);
            dispMsg.menuData.lines[1][sizeof(dispMsg.menuData.lines[1]) - 1] = '\0';

            // Line 3 y 4: En blanco
            dispMsg.menuData.lines[2][0] = '\0';
            dispMsg.menuData.lines[3][0] = '\0';
            
            dispMsg.menuData.selectedLine = 1; // Cursor on the value
            
            xQueueOverwrite(displayQueue, &dispMsg);
            break;

        case STATE_IN_FLIGHT:
            // in-flight UI is controlled by sensor task    
            break;

        default:
            break;
    }
}

void end_flight(){
    // Notificar a la tarea de sensado para que frene
    xTaskNotifyGive(sensor_task_handle);
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
}

// Acciones

systemState_td start_flight_action(void) {
    // Notificar a la tarea de sensado para que comience
    xTaskNotifyGive(sensor_task_handle);

    return STATE_IN_FLIGHT; // Transition the UI task into flight mode
}

systemState_td reset_config_action(void) {
    varioConfig = defaultConfig;
    return STATE_MAIN_MENU; // Stay in the menu
}
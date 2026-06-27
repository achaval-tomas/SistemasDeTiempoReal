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

typedef enum {
    FLOAT,
    INT
} settingType_td; // whether to display as float or int

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
void end_flight(void);

// Funciones que responden a las acciones del encoder
systemState_td start_flight_action(void);
systemState_td reset_config_action(void);
systemState_td handle_menu_click(const menuItem_td *item);
void update_setting(int16_t delta, const menuItem_td *item);
uint8_t update_index(uint8_t currentIndex, int16_t delta, uint8_t maxItems);

// MENÚ
const menuItem_td menu[] = {
    {"Comenzar vuelo",     ITEM_TYPE_ACTION,  {.action = start_flight_action}},
    {"Umbral subida",      ITEM_TYPE_SETTING, {.setting = {FLOAT, &varioConfig.lift_threshold, 0.1f, 0.1f, 5.0f}}},
    {"Umbral bajada",      ITEM_TYPE_SETTING, {.setting = {FLOAT, &varioConfig.sink_threshold, 0.1f, -10.0f, -0.1f}}},
    {"Sensibilidad",       ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.sensitivity, 1.0f, 1.0f, 10.0f}}},
    {"Volumen",            ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.volume, 1.0f, 0.0f, 5.0f}}},
    {"Ajustar QNH",        ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.sealevel_hPa, 1.0f, 950.0f, 1300.0f}}},
    {"Sonido subida (Hz)", ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.lift_hz_base, 10.0f, 500.0f, 1500.0f}}},
    {"Paso subida (Hz)",   ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.lift_hz_scale, 10.0f, 0.0f, 200.0f}}},
    {"Sonido bajada (Hz)", ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.sink_hz_base, 10.0f, 100.0f, 500.0f}}},
    {"Paso bajada (Hz)",   ITEM_TYPE_SETTING, {.setting = {INT, &varioConfig.sink_hz_scale, 10.0f, 0.0f, 200.0f}}},
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
    while (!is_long_press(event)) {
        xQueueReceive(encoderEventQueue, &event, portMAX_DELAY);
    }

    currentState = STATE_MAIN_MENU;
    menuIndex = 0;
    selectedItem = &menu[0];

    // Habilitar la lectura de rotaciones del encoder
    RE_Enable_Rotations();

    dispMsg.type = DISPLAY_ON;
    xQueueOverwrite(displayQueue, &dispMsg);
    // Wait until display is ON
    vTaskDelay(pdMS_TO_TICKS(150));

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

                    // Switch display OFF
                    dispMsg.type = DISPLAY_OFF;
                    xQueueOverwrite(displayQueue, &dispMsg);

                    // Remove the LONG PRESS type
                    event.type = ENCODER_EVENT_NONE;

                    // Stop checking for rotation events
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

void update_display(systemState_td currentState, uint8_t menuIndex){
    displayQueueData_td dispMsg = {0};
    int8_t max_start;
    uint8_t lineIndex;
    
    switch (currentState){
        case STATE_MAIN_MENU:
            max_start = (int8_t)MENU_ITEMS_COUNT - 4;
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

    // Habilitar la lectura de rotaciones del encoder
    RE_Enable_Rotations();
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

    // If setting was volume, notify buzzer task to play a beep
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

    return STATE_IN_FLIGHT; // Transition the UI task into flight mode
}

systemState_td reset_config_action(void) {
    // Show configuration reset message for 3 seconds
    displayQueueData_td dispMsg = {0};
    dispMsg.type = DISPLAY_UPDATE_MENU;
    
    strncpy(dispMsg.menuData.lines[0], ">                  <", 21);
    strncpy(dispMsg.menuData.lines[1], ">  Configuracion   <", 21);
    strncpy(dispMsg.menuData.lines[2], ">  Reestablecida   <", 21);
    strncpy(dispMsg.menuData.lines[3], ">                  <", 21);
    dispMsg.menuData.selectedLine = NO_SELECTION;

    xQueueOverwrite(displayQueue, &dispMsg);
    vTaskDelay(pdMS_TO_TICKS(3000));

    varioConfig = defaultConfig;
    return STATE_MAIN_MENU; // Stay in the menu
}
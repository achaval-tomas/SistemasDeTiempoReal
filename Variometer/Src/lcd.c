#include "lcd.h"
#include "stm32h5xx_hal.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* LCD Hardware Mapping */
#define SLAVE_ADDRESS_LCD 0x4E
#define PIN_RS            (1 << 0)
#define PIN_RW            (1 << 1)
#define PIN_EN            (1 << 2)
#define BACKLIGHT         (1 << 3)

/* HD44780 Command Set */
typedef enum {
    LCD_CMD_CLEAR_DISPLAY   = 0x01,
    LCD_CMD_RETURN_HOME     = 0x02,
    LCD_CMD_ENTRY_MODE_SET  = 0x06,
    LCD_CMD_DISPLAY_CONTROL = 0x08,
    LCD_CMD_CURSOR_SHIFT    = 0x10,
    LCD_CMD_FUNCTION_SET    = 0x20,
    LCD_CMD_SET_CGRAM_ADDR  = 0x40,
    LCD_CMD_SET_DDRAM_ADDR  = 0x80
} lcd_commands_t;

/* Display Control Flags */
#define LCD_DISPLAY_ON      0x04
#define LCD_DISPLAY_OFF     0x00
#define LCD_CURSOR_ON       0x02
#define LCD_CURSOR_OFF      0x00
#define LCD_BLINK_ON        0x01
#define LCD_BLINK_OFF       0x00
#define LCD_4BIT_MODE       0x28

static uint8_t lcd_backlight_val = 0; // Initialized OFF

void lcd_send_internal(uint8_t data, uint8_t flags) {
    uint8_t up = data & 0xF0;
    uint8_t lo = (data << 4) & 0xF0;
    uint8_t data_t[4]; 
    
    data_t[0] = up | flags | lcd_backlight_val | PIN_EN;
    data_t[1] = up | flags | lcd_backlight_val;
    data_t[2] = lo | flags | lcd_backlight_val | PIN_EN;
    data_t[3] = lo | flags | lcd_backlight_val;

    HAL_I2C_Master_Transmit(&hi2c2, SLAVE_ADDRESS_LCD, data_t, 4, 100);
}

void lcd_send_cmd(uint8_t cmd) {
    lcd_send_internal(cmd, 0);
}

void lcd_send_data(uint8_t data) {
    lcd_send_internal(data, PIN_RS);
}

void lcd_load_custom_characters() {
    const uint8_t up_arrow[8] = {0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04, 0x00};
    const uint8_t down_arrow[8] = {0x04, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00};
    
    lcd_create_custom_char(CHAR_UP_ARROW, up_arrow);
    lcd_create_custom_char(CHAR_DOWN_ARROW, down_arrow);
}

void lcd_init(void) {
    HAL_Delay(50);

    uint8_t cmd = 0x30 | lcd_backlight_val;
    uint8_t data_t[2] = {cmd | PIN_EN, cmd}; 
    
    for(int i = 0; i < 3; i++) {
        HAL_I2C_Master_Transmit(&hi2c2, SLAVE_ADDRESS_LCD, data_t, 2, 100);
        HAL_Delay(i == 0 ? 5 : 1);
    }

    uint8_t data_4bit[2] = {(0x20 | lcd_backlight_val) | PIN_EN, (0x20 | lcd_backlight_val)};
    HAL_I2C_Master_Transmit(&hi2c2, SLAVE_ADDRESS_LCD, data_4bit, 2, 100);
    HAL_Delay(10);

    lcd_send_cmd(LCD_4BIT_MODE);
    lcd_send_cmd(LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_OFF);
    lcd_send_cmd(LCD_CMD_CLEAR_DISPLAY);
    HAL_Delay(2);
    lcd_send_cmd(LCD_CMD_ENTRY_MODE_SET);

    lcd_load_custom_characters();
}

void lcd_send_string(char *str) {
    while (*str) lcd_send_data(*str++);
}

void lcd_put_cur(uint8_t row, uint8_t col) {
    uint8_t pos = (row == 0) ? (LCD_CMD_SET_DDRAM_ADDR | col) : (0xC0 | col);
    lcd_send_cmd(pos);
}

void lcd_clear(void) {
    lcd_send_cmd(LCD_CMD_CLEAR_DISPLAY);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd_printf_at(uint8_t row, uint8_t col, const char *fmt, ...) {
    lcd_put_cur(row, col);

    char buffer[17]; // 16 characters + null
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    lcd_send_string(buffer);
}

void lcd_backlight(uint8_t state) {
    lcd_backlight_val = state ? BACKLIGHT : 0;
    lcd_send_cmd(LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_ON);
}

void lcd_off(void) {
    lcd_backlight_val = 0;
    lcd_send_cmd(LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_OFF);
}

void lcd_on(void) {
    lcd_backlight_val = BACKLIGHT;
    lcd_send_cmd(LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_ON);
}

void lcd_cursor(uint8_t mode) {
    switch(mode) {
        case 0: lcd_send_cmd(LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_ON | LCD_CURSOR_OFF); break;
        case 1: lcd_send_cmd(LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_ON | LCD_CURSOR_ON); break;
        case 2: lcd_send_cmd(LCD_CMD_DISPLAY_CONTROL | LCD_DISPLAY_ON | LCD_CURSOR_ON | LCD_BLINK_ON); break;
    }
}

// Saves a custom character to 1 of 8 CGRAM slots (locations 8-15)
void lcd_create_custom_char(uint8_t location, const uint8_t charmap[]) {
    // Convert 8-15 to 0-7 for proper addressing
    uint8_t hardware_slot = (uint8_t)location & 0x07; 
    
    lcd_send_cmd(LCD_CMD_SET_CGRAM_ADDR | (hardware_slot << 3));
    for (int i = 0; i < 8; i++) {
        lcd_send_data(charmap[i]);
    }
    lcd_send_cmd(LCD_CMD_SET_DDRAM_ADDR); 
}
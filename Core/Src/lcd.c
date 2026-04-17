#include "lcd.h"
#include "stm32h5xx_hal.h"
#include "i2c.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* Configuración de Hardware */
#define SLAVE_ADDRESS_LCD 0x4E   // Dirección I2C (0x27 << 1)

/* Mapeo de pines del PCF8574 hacia el LCD */
#define PIN_RS    (1 << 0)       // P0 -> RS
#define PIN_RW    (1 << 1)       // P1 -> RW (usualmente a GND, pero mapeado aquí)
#define PIN_EN    (1 << 2)       // P2 -> Enable
#define BACKLIGHT (1 << 3)       // P3 -> Backlight (1 = ON)

static uint8_t lcd_backlight_val = BACKLIGHT; // Por defecto encendido (PIN_BACKLIGHT)

/*
 *  Envía comandos o datos usando la secuencia de 4 pasos (Strobe) por cada nibble
 */
void lcd_send_internal(uint8_t data, uint8_t flags) {
    uint8_t up = data & 0xF0;
    uint8_t lo = (data << 4) & 0xF0;
    uint8_t data_t[4]; 
    
    // Nibble Superior
    data_t[0] = up | flags | lcd_backlight_val | PIN_EN;
    data_t[1] = up | flags | lcd_backlight_val;           // EN=0

    // Nibble Inferior
    data_t[2] = lo | flags | lcd_backlight_val | PIN_EN;  // EN=1
    data_t[3] = lo | flags | lcd_backlight_val;           // EN=0

    // Se envían los 4 bytes en una sola ráfaga I2C
    HAL_I2C_Master_Transmit(&hi2c2, SLAVE_ADDRESS_LCD, data_t, 4, 100);
}

void lcd_send_cmd(char cmd) {
    lcd_send_internal(cmd, 0); // RS=0 para comandos
}

void lcd_send_data(char data) {
    lcd_send_internal(data, PIN_RS); // RS=1 para datos
}

void lcd_init(void) {
    HAL_Delay(50); // Wait for VCC to stabilize

    // Sequence to reset the LCD controller into a known state
    uint8_t cmd;
    
    cmd = 0x30 | lcd_backlight_val;
    uint8_t data_t[2] = {cmd | PIN_EN, cmd}; 
    HAL_I2C_Master_Transmit(&hi2c2, SLAVE_ADDRESS_LCD, data_t, 2, 100);
    HAL_Delay(5);

    HAL_I2C_Master_Transmit(&hi2c2, SLAVE_ADDRESS_LCD, data_t, 2, 100);
    HAL_Delay(1);

    HAL_I2C_Master_Transmit(&hi2c2, SLAVE_ADDRESS_LCD, data_t, 2, 100);
    HAL_Delay(10);

    // Switch to 4-bit mode
    cmd = 0x20 | lcd_backlight_val;
    uint8_t data_4bit[2] = {cmd | PIN_EN, cmd};
    HAL_I2C_Master_Transmit(&hi2c2, SLAVE_ADDRESS_LCD, data_4bit, 2, 100);
    HAL_Delay(10);

    // Standard functions because it is now in 4-bit mode
    lcd_send_cmd(0x28); // 2 lines, 5x8 font
    lcd_send_cmd(0x08); // Display OFF
    lcd_send_cmd(0x01); // Clear Display
    HAL_Delay(2);
    lcd_send_cmd(0x06); // Entry mode
    lcd_send_cmd(0x0C); // Display ON, Cursor OFF
}

void lcd_send_string(char *str) {
    while (*str) lcd_send_data(*str++);
}

void lcd_put_cur(int row, int col) {
    uint8_t pos = (row == 0) ? (0x80 | col) : (0xC0 | col);
    lcd_send_cmd(pos);
}

void lcd_clear(void) {
    lcd_send_cmd(0x01);
    HAL_Delay(2);
}

void lcd_printf(const char *fmt, ...) {
    char buffer[20]; // El buffer debe ser al menos del tamaño del LCD (16 o 20)
    va_list args;
    
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    lcd_send_string(buffer);
}

void lcd_backlight(uint8_t state) {
    if (state) lcd_backlight_val = (1 << 3); // Encender
    else       lcd_backlight_val = 0;        // Apagar
    
    // Comando vacío para actualizar el estado
    lcd_send_cmd(0x00); 
}

void lcd_off(void){
    lcd_backlight(0);
    lcd_clear();
}
void lcd_on(void){
    lcd_clear();
    lcd_backlight(1);
}

/**
 * mode 0 = Invisible, 1 = Visible (raya), 2 = Visible + Parpadeo (bloque)
 */
void lcd_cursor(uint8_t mode) {
    switch(mode) {
        case 0: lcd_send_cmd(0x0C); break; // Solo pantalla
        case 1: lcd_send_cmd(0x0E); break; // Pantalla + Cursor
        case 2: lcd_send_cmd(0x0F); break; // Pantalla + Cursor + Parpadeo
        default: break;
    }
}
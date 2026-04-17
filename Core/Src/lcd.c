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

/**
 * @brief Envía comandos o datos usando la secuencia de 4 pasos (Strobe) por cada nibble
 */
void lcd_send_internal(uint8_t data, uint8_t flags) {
    uint8_t up = data & 0xF0;
    uint8_t lo = (data << 4) & 0xF0;
    uint8_t data_t[4]; 

    /* Secuencia de 4 bits: El LCD captura en el flanco de bajada de EN */
    
    // Nibble Superior
    data_t[0] = up | flags | BACKLIGHT | PIN_EN;  // EN=1
    data_t[1] = up | flags | BACKLIGHT;           // EN=0

    // Nibble Inferior
    data_t[2] = lo | flags | BACKLIGHT | PIN_EN;  // EN=1
    data_t[3] = lo | flags | BACKLIGHT;           // EN=0

    // Se envían los 4 bytes en una sola ráfaga I2C
    HAL_I2C_Master_Transmit(&hi2c1, SLAVE_ADDRESS_LCD, data_t, 4, 100);
}

void lcd_send_cmd(char cmd) {
    lcd_send_internal(cmd, 0); // RS=0 para comandos
}

void lcd_send_data(char data) {
    lcd_send_internal(data, PIN_RS); // RS=1 para datos
}

void lcd_init(void) {
    // 1. Espera extra larga para que el voltaje se estabilice
    HAL_Delay(100); 

    // 2. Secuencia de reset manual (Modo 8-bit inicial)
    // Se envía 0x30 tres veces para despertar al controlador
    lcd_send_cmd(0x30);
    HAL_Delay(10);
    lcd_send_cmd(0x30);
    HAL_Delay(1);
    lcd_send_cmd(0x30);
    HAL_Delay(1);

    // 3. Establecer modo 4-bits
    lcd_send_cmd(0x20); 
    HAL_Delay(10);

    // 4. Configuración final
    lcd_send_cmd(0x28); // 2 líneas, 5x8
    HAL_Delay(1);
    lcd_send_cmd(0x0C); // Display ON, Cursor OFF
    HAL_Delay(1);
    lcd_send_cmd(0x01); // Clear
    HAL_Delay(5);       // El clear necesita mucho tiempo
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

/**
 * @brief Imprime texto formateado en el LCD (estilo printf)
 * @param fmt: Cadena de formato (ej: "Temp: %d C")
 * @param ...: Argumentos variables
 */
void lcd_printf(const char *fmt, ...) {
    char buffer[20]; // El buffer debe ser al menos del tamaño del LCD (16 o 20)
    va_list args;
    
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    lcd_send_string(buffer);
}
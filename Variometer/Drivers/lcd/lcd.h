#ifndef I2C_LCD_H_
#define I2C_LCD_H_
#include <stdint.h>

#define CHAR_UP_ARROW (uint8_t)8
#define CHAR_DOWN_ARROW (uint8_t)9

void lcd_init (void);   // initialize lcd

void lcd_off(void);
void lcd_on(void);

void lcd_send_cmd (uint8_t cmd);  // send command to the lcd
void lcd_send_data (uint8_t data);  // send data to the lcd
void lcd_send_string (char *str);  // send string to the lcd

void lcd_cursor(uint8_t mode);
void lcd_put_cur(uint8_t row, uint8_t col);  // put cursor at the entered position

void lcd_clear (void);
void lcd_backlight(uint8_t state);

// Print formatted text at specific position
void lcd_printf_at(uint8_t row, uint8_t col, const char *fmt, ...);

// Custom character creation 
void lcd_create_custom_char(uint8_t location, const uint8_t charmap[]);

#endif
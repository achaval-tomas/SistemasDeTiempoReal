#ifndef I2C_LCD_H_
#define I2C_LCD_H_
#include <stdint.h>

void lcd_init (void);   // initialize lcd

// All functions below should be called from TASKS
void lcd_off(void);
void lcd_on(void);
void lcd_send_cmd (uint8_t cmd);  // send command to the lcd
void lcd_send_data (uint8_t data);  // send data to the lcd
void lcd_send_string (char *str);  // send string to the lcd
void lcd_cursor(uint8_t mode);
void lcd_put_cur(int row, int col);  // put cursor at the entered position
void lcd_clear (void);
void lcd_backlight(uint8_t state);
void lcd_printf(const char *fmt, ...);

#endif
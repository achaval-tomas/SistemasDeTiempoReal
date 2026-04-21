/*  
    My own library for I2C communication with
    the BMP280 barometric pressure and temperature
    sensor.

    Must initialize i2c1 before using and call bmp280_init() to set up the sensor.
    Then call bmp280_read_data() to get compensated temperature and pressure.
*/
#ifndef __BMP280_H
#define __BMP280_H

// Device address and ID for I2C communication
#define BMP280_ADDR 0x76
#define BMP280_ID 0x58

#include <stdint.h>

/*
    Configuration settings for bmp280_init.
*/
typedef struct {
    uint8_t config;
    uint8_t ctrl_meas;
} bmp280_settings_td;

/*
    Main structure to hold compensated temperature and pressure readings.
*/
typedef struct {
  float temperature_C;
  float pressure_Pa;
} bmp280_td;

/*
    Initializes the BMP280 sensor with the provided settings.
    Must be called before any other function to ensure valid readings.
*/
void bmp280_init(bmp280_settings_td settings);

/*
    Initializes the BMP280 sensor with default (maximum resolution) settings.
    Must be called before any other function to ensure valid readings.
*/
void bmp280_init_default();

/*
    Reads raw data from the sensor, applies compensation formulas and fills
    the provided structure with temperature in Celsius and pressure in Pascals.
    Must be called after bmp280_init() to get valid readings.
*/
void bmp280_read_data(bmp280_td *bmp280);

/*  
    Estimates altitude based on current pressure reading and sea level pressure.
*/
float bmp280_estimate_altitude(bmp280_td bmp280, float seaLevelPressure_Pa);

#endif /* __BMP280_H */
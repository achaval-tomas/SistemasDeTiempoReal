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

#define BMP_STANDBY_0_5ms  0b00000000
#define BMP_STANDBY_62_5ms 0b00100000

#define BMP_FILTER_OFF 0b00000000
#define BMP_FILTER_2   0b00000100
#define BMP_FILTER_4   0b00001000
#define BMP_FILTER_8   0b00001100
#define BMP_FILTER_16  0b00010000

#define BMP_T_OSRS_1  0b00100000  // Temperature oversampling x1
#define BMP_T_OSRS_2  0b01000000  // Temperature oversampling

#define BMP_P_OSRS_1  0b00000100  // Pressure oversampling x1
#define BMP_P_OSRS_2  0b00001000  // Pressure oversampling x2
#define BMP_P_OSRS_4  0b00001100  // Pressure oversampling x4
#define BMP_P_OSRS_8  0b00010000  // Pressure oversampling x8
#define BMP_P_OSRS_16 0b00010100  // Pressure oversampling x16 

#define BMP_MODE_SLEEP  0b00
#define BMP_MODE_FORCED 0b01
#define BMP_MODE_NORMAL 0b11

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
float bmp280_estimate_altitude(float pressure_Pa, float temperature_C, float seaLevelPressure_Pa);

#endif /* __BMP280_H */
/*  
    My own library for I2C communication with
    the BMP280 barometric pressure and temperature
    sensor.
*/

#include "bmp280.h"
#include <stdint.h>
#include "i2c.h"
#include "math.h"
#include "stm32h5xx_hal_i2c.h"

// Calibration coefficients for temperature and pressure compensation,
// as well as the intermediate t_fine variable used in compensation formulas.
typedef struct {
    // calibration coefficients for temperature sensor
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;

    // calibration coefficients for pressure sensor
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;

    // Variable to store the intermediate temperature coefficient
    int32_t t_fine;
} bmp280_calib_data_td;

// Private global variable to hold calibration data
static bmp280_calib_data_td calib = {0};

// Private helper function prototypes
void bmp280_calibrate();
float compensate_temperature_data(int32_t adc_T);
float compensate_pressure_data(int32_t adc_P);

void bmp280_calibrate(){
  uint8_t data[24];
  HAL_I2C_Mem_Read(&hi2c1, BMP280_ADDR << 1, 0x88, 1, data, 24, 100);

  calib.dig_t1 = (data[1] << 8) | data[0];
  calib.dig_t2 = (data[3] << 8) | data[2];
  calib.dig_t3 = (data[5] << 8) | data[4];

  calib.dig_p1 = (data[7] << 8) | data[6];
  calib.dig_p2 = (data[9] << 8) | data[8];
  calib.dig_p3 = (data[11] << 8) |  data[10];
  calib.dig_p4 = (data[13] << 8) |  data[12];
  calib.dig_p5 = (data[15] << 8) |  data[14];
  calib.dig_p6 = (data[17] << 8) |  data[16];
  calib.dig_p7 = (data[19] << 8) |  data[18];
  calib.dig_p8 = (data[21] << 8) |  data[20];
  calib.dig_p9 = (data[23] << 8) |  data[22];
}

void bmp280_init(bmp280_settings_td settings){
    HAL_StatusTypeDef res;
    uint16_t id = 0;
    
    // Check if sensor is connected by reading the ID register
    res = HAL_I2C_Mem_Read(&hi2c1, BMP280_ADDR << 1, 0xD0, 1, (uint8_t*)&id, 1, 100);
    if (res != HAL_OK){
      printf("Error: Failed to read from BMP280 (i2c read failed)!\n");
      while (1);
    } else if (id != BMP280_ID){
      printf("Error: BMP280 not detected (wrong id)!\n");
      while (1);
    }

    // Check if sensor is ready
    res = HAL_I2C_IsDeviceReady(&hi2c1, BMP280_ADDR << 1, 3, 100);
    if (res != HAL_OK){
      printf("Error: BMP280 not ready!\n");
      while (1);
    }
  
    // Write calibration data into global struct
    bmp280_calibrate();

    // Apply configuration parameters
    HAL_I2C_Mem_Write(&hi2c1, BMP280_ADDR << 1, 0xF5, 1, &settings.config, 1, 100);
    
    if (((settings.ctrl_meas & 0b11) == 0b01) || ((settings.ctrl_meas & 0b11) == 0b10)) {
      // If forced mode is selected, clear mode bits to start with sensor off
      // because it will be triggered by read_data calls.
      settings.ctrl_meas &= 0b11111100;
    }
    HAL_I2C_Mem_Write(&hi2c1, BMP280_ADDR << 1, 0xF4, 1, &settings.ctrl_meas, 1, 100);

    printf("BMP280 initialized successfully!\n");
  }

// Always use before pressure compensation to update t_fine
float compensate_temperature_data(int32_t adc_T){
  int32_t var1, var2, T;

  var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_t1 << 1))) * ((int32_t)calib.dig_t2)) >> 11;
  var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_t1)) *
          ((adc_T >> 4) - ((int32_t)calib.dig_t1))) >> 12) *
          ((int32_t)calib.dig_t3)) >> 14;

  calib.t_fine = var1 + var2;
  T = (calib.t_fine * 5 + 128) >> 8;

  return T / 100.0f;
}

float compensate_pressure_data(int32_t adc_P){
  uint32_t P;
  int64_t var1, var2, p;

  var1 = ((int64_t)calib.t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)calib.dig_p6;
  var2 = var2 + ((var1 * (int64_t)calib.dig_p5) << 17);
  var2 = var2 + (((int64_t)calib.dig_p4) << 35);
  var1 = ((var1 * var1 * (int64_t)calib.dig_p3) >> 8) +
        ((var1 * (int64_t)calib.dig_p2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * (int64_t)calib.dig_p1 >> 33;

  if (var1 == 0) {
      return 0; // avoid division by zero
  }

  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = ((int64_t)calib.dig_p9 * (p >> 13) * (p >> 13)) >> 25;
  var2 = ((int64_t)calib.dig_p8 * p) >> 19;
  p = ((p + var1 + var2) >> 8) + ((int64_t)calib.dig_p7 << 4);
  P = (uint32_t)p;

  // Return pressure in Pa
  return P / 256.0f;
}


void bmp280_read_data(bmp280_td *bmp280) {
    uint8_t data[6];

    HAL_I2C_Mem_Read(&hi2c1, BMP280_ADDR << 1, 0xF7, 1, data, 6, 100);

    int32_t adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
    int32_t adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);

    bmp280->temperature_C = compensate_temperature_data(adc_T);
    bmp280->pressure_Pa = compensate_pressure_data(adc_P);
  }


float bmp280_estimate_altitude(bmp280_td bmp280, float seaLevelPressure_Pa) {
  if (seaLevelPressure_Pa <= 0.0f){
    seaLevelPressure_Pa = 102000.0f; // Default value
  }

  float absT = bmp280.temperature_C + 273.15f;

  return (287.05f * absT / 9.80665f) * logf(seaLevelPressure_Pa / bmp280.pressure_Pa);
}
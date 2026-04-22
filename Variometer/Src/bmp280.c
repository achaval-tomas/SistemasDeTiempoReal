/*  
    My own library for I2C communication with
    the BMP280 barometric pressure and temperature
    sensor.
*/

#include "bmp280.h"
#include <stdint.h>
#include "i2c.h"
#include "math.h"
#include "stm32h5xx_hal.h"
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
    uint8_t id = 0;
    
    // Check if sensor is connected by reading the ID register
    res = HAL_I2C_Mem_Read(&hi2c1, BMP280_ADDR << 1, 0xD0, 1, &id, 1, 100);
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
    
    // Perform a soft reset to ensure proper calibration values are set
    uint8_t reset = 0xB6;
    HAL_I2C_Mem_Write(&hi2c1, BMP280_ADDR << 1, 0xE0, 1, &reset, 1, 100);
  
    // Wait for reset to be done (~2ms according to Bosch)
    HAL_Delay(5);

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

  void bmp280_init_default(){
    bmp280_init((bmp280_settings_td){
      .config = 0b00001000, // standby 0.5ms, filter x4
      .ctrl_meas = 0b00110011 // temp x1, pressure x8, normal mode
    });
  }

// Always use before pressure compensation to update t_fine
float compensate_temperature_data(int32_t adc_T){
  float var1, var2, T;
    
  var1 = (((float)adc_T) / 16384.0 - ((float)calib.dig_t1) / 1024.0) * ((float)calib.dig_t2);
  
  var2 = ((((float)adc_T) / 131072.0 - ((float)calib.dig_t1) / 8192.0) *
          (((float)adc_T) / 131072.0 - ((float)calib.dig_t1) / 8192.0)) * ((float)calib.dig_t3);
          
  calib.t_fine = (int32_t)(var1 + var2);
  T = (var1 + var2) / 5120.0;
  
  return T;
}

float compensate_pressure_data(int32_t adc_P){
  float var1, var2, p;
    
  var1 = ((float)calib.t_fine / 2.0) - 64000.0;
  var2 = var1 * var1 * ((float)calib.dig_p6) / 32768.0;
  var2 = var2 + var1 * ((float)calib.dig_p5) * 2.0;
  var2 = (var2 / 4.0) + (((float)calib.dig_p4) * 65536.0);
  
  var1 = (((float)calib.dig_p3) * var1 * var1 / 524288.0 + 
          ((float)calib.dig_p2) * var1) / 524288.0;
  var1 = (1.0 + var1 / 32768.0) * ((float)calib.dig_p1);
  
  if (var1 == 0.0) {
      return 0; // avoid exception caused by division by zero
  }
  
  p = 1048576.0 - (float)adc_P;
  p = (p - (var2 / 4096.0)) * 6250.0 / var1;
  
  var1 = ((float)calib.dig_p9) * p * p / 2147483648.0;
  var2 = p * ((float)calib.dig_p8) / 32768.0;
  p = p + (var1 + var2 + ((float)calib.dig_p7)) / 16.0;
  
  return p;
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
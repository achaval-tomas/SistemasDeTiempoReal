/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os2.h"
#include "i2c.h"
#include "icache.h"
#include "stm32h5xx_hal.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "portmacrocommon.h"
#include "projdefs.h"
#include "stm32h533xx.h"
#include "stm32h5xx_hal_gpio.h"
#include "stm32h5xx_nucleo.h"
#include "task.h"
#include <math.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  uint32_t frequencyHZ;
  uint32_t durationMS;
} buzzerParams_td;

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
} bmp280_calib_data_t;

typedef struct {
  float temperature_C;
  float pressure_Pa;
} sensorData_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TIM2_TICKS_PER_SEC 1000000
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

/* USER CODE BEGIN PV */
TaskHandle_t variometer_task_handle = NULL;

bmp280_calib_data_t calib_data = {0};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
void Variometer(void *pvParameters);
void Buzzer(buzzerParams_td buzzerParams);

// Simple absolute value function for floats
float absf(float x){
  return x < 0 ? -x : x;
}

void bmp280_calibrate(){
  uint8_t calib[24];
  HAL_I2C_Mem_Read(&hi2c1, 0x76 << 1, 0x88, 1, calib, 24, 100);

  calib_data.dig_t1 = (calib[1] << 8) | calib[0];
  calib_data.dig_t2 = (calib[3] << 8) | calib[2];
  calib_data.dig_t3 = (calib[5] << 8) | calib[4];

  calib_data.dig_p1 = (calib[7] << 8) | calib[6];
  calib_data.dig_p2 = (calib[9] << 8) | calib[8];
  calib_data.dig_p3 = (calib[11] << 8) |  calib[10];
  calib_data.dig_p4 = (calib[13] << 8) |  calib[12];
  calib_data.dig_p5 = (calib[15] << 8) |  calib[14];
  calib_data.dig_p6 = (calib[17] << 8) |  calib[16];
  calib_data.dig_p7 = (calib[19] << 8) |  calib[18];
  calib_data.dig_p8 = (calib[21] << 8) |  calib[20];
  calib_data.dig_p9 = (calib[23] << 8) |  calib[22];

}

void bmp280_init(){
  // Check if sensor is connected by reading the ID register
  uint16_t id = 0;
  HAL_I2C_Mem_Read(&hi2c1, 0x76 << 1, 0xD0, 1, (uint8_t*)&id, 1, 100);
  if (id != 0x58){
    printf("Error: BMP280 not detected!\n");
    while (1);
  };

  // Check if sensor is ready
  HAL_StatusTypeDef res;
  res = HAL_I2C_IsDeviceReady(&hi2c1, 0x76 << 1, 3, 100);
  if (res != HAL_OK){
    printf("Error: BMP280 not ready!\n");
    while (1);
  };

  // Write calibration data to global struct
  bmp280_calibrate();

  // temp oversampling x1
  // pressure oversampling x1
  // normal mode
  uint8_t ctrl_meas = 0x27;
  HAL_I2C_Mem_Write(&hi2c1, 0x76 << 1, 0xF4, 1, &ctrl_meas, 1, 100);

  // standby + filter settings (default-ish)
  uint8_t config = 0xA0;
  HAL_I2C_Mem_Write(&hi2c1, 0x76 << 1, 0xF5, 1, &config, 1, 100);

  HAL_Delay(10);
}

// always use before pressure compensation
float compensate_temperature_data(int32_t adc_T){
  // Compensation formula for temperature
  int32_t var1, var2, T;

  uint16_t dig_T1 = calib_data.dig_t1;
  int16_t dig_T2 = calib_data.dig_t2;
  int16_t dig_T3 = calib_data.dig_t3;

  var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
          ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) *
          ((int32_t)dig_T3)) >> 14;

  calib_data.t_fine = var1 + var2;
  T = (calib_data.t_fine * 5 + 128) >> 8;

  return T / 100.0f;
}

float compensate_pressure_data(int32_t adc_P){
  // Compensation formula for pressure
  uint32_t P;
  int64_t var1, var2, p;

  uint16_t dig_P1 = calib_data.dig_p1;
  int16_t dig_P2 = calib_data.dig_p2;
  int16_t dig_P3 = calib_data.dig_p3;
  int16_t dig_P4 = calib_data.dig_p4;
  int16_t dig_P5 = calib_data.dig_p5;
  int16_t dig_P6 = calib_data.dig_p6;
  int16_t dig_P7 = calib_data.dig_p7;
  int16_t dig_P8 = calib_data.dig_p8;
  int16_t dig_P9 = calib_data.dig_p9;

  var1 = ((int64_t)calib_data.t_fine) - 128000;
  var2 = var1 * var1 * (int64_t)dig_P6;
  var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
  var2 = var2 + (((int64_t)dig_P4) << 35);
  var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) +
        ((var1 * (int64_t)dig_P2) << 12);
  var1 = (((((int64_t)1) << 47) + var1)) * (int64_t)dig_P1 >> 33;

  if (var1 == 0) {
      return 0; // avoid division by zero
  }

  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = ((int64_t)dig_P9 * (p >> 13) * (p >> 13)) >> 25;
  var2 = ((int64_t)dig_P8 * p) >> 19;
  p = ((p + var1 + var2) >> 8) + ((int64_t)dig_P7 << 4);
  P = (uint32_t)p;

  // Return pressure in Pa
  return P / 256.0f;
}

// always use after calibrating the sensor
void bmp280_read_data(sensorData_t *bmp280){
  uint8_t data[6];
  HAL_I2C_Mem_Read(&hi2c1, 0x76 << 1, 0xF7, 1, data, 6, 100);

  int32_t adc_P = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);
  int32_t adc_T = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);

  bmp280->temperature_C = compensate_temperature_data(adc_T);
  bmp280->pressure_Pa = compensate_pressure_data(adc_P);
}

float estimate_altitude(sensorData_t bmp280){
  float SLPressure_hPa = 101900.0f;
  float T = bmp280.temperature_C + 273.15f;  // convert to Kelvin
  return (287.05f * T / 9.80665f) * logf(SLPressure_hPa / bmp280.pressure_Pa);
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ICACHE_Init();
  MX_TIM2_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  bmp280_init();

  xTaskCreate(
    Variometer,
    "Variometer Task",
    configMINIMAL_STACK_SIZE*4,
    (void*) NULL,
    1,
    (void*) &variometer_task_handle
  );

  vTaskStartScheduler();
  /* We should never get here as control is now taken by the scheduler */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV4;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the programming delay
  */
  __HAL_FLASH_SET_PROGRAM_DELAY(FLASH_PROGRAMMING_DELAY_0);
}

/* USER CODE BEGIN 4 */
void BSP_PB_Callback(Button_TypeDef Button){
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if (Button == BUTTON_USER){
    vTaskNotifyGiveFromISR(variometer_task_handle, &xHigherPriorityTaskWoken);
  }
  
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void Variometer(void *pvParameters){
  // Higher alpha = More responsive (and more noisy)
  float alpha = 0.1f;

  sensorData_t bmp280;
  bmp280_read_data(&bmp280);
  float p0 = 0;
  
  // Stabilize initial pressure with a 30-sample average (3 second initial wait)
  for (uint16_t i = 0; i < 30; i++){
    bmp280_read_data(&bmp280);
    p0 += bmp280.pressure_Pa;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  
  p0 = p0 / 30.0f;
  float pnew = p0;
	
  while (1){
    // Wait until ~20cm altitude change is detected from starting position (1m ~ 12Pa)
    while (absf(p0 - pnew) < 2.4f){
      vTaskDelay(pdMS_TO_TICKS(200));
      bmp280_read_data(&bmp280);
      pnew = pnew * (1 - alpha) + bmp280.pressure_Pa * alpha;
    }

    // Beep when altitude change is detected
    buzzerParams_td buzzData = {400, 500};
    Buzzer(buzzData);

    // reset starting pressure for next round
    bmp280_read_data(&bmp280);
    p0 = bmp280.pressure_Pa;
    pnew = p0;
    vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void Buzzer(buzzerParams_td buzzerParams){
  uint32_t freq = buzzerParams.frequencyHZ;

  // Calculate ARR from frequency in HZ
  uint32_t arr = TIM2_TICKS_PER_SEC / freq;

  // Set pulse to 50% arr for max volume
  uint32_t pulse = arr / 2;

  uint32_t duration = buzzerParams.durationMS;

  // Set up timer for PWM output
  __HAL_TIM_SET_AUTORELOAD(&htim2, arr);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);

  // Beep at the specified frequency and duration
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  vTaskDelay(pdMS_TO_TICKS(duration));
  HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};
  MPU_Attributes_InitTypeDef MPU_AttributesInit = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region 0 and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x08FFF000;
  MPU_InitStruct.LimitAddress = 0x08FFFFFF;
  MPU_InitStruct.AttributesIndex = MPU_ATTRIBUTES_NUMBER0;
  MPU_InitStruct.AccessPermission = MPU_REGION_ALL_RO;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Attribute 0 and the memory to be protected
  */
  MPU_AttributesInit.Number = MPU_ATTRIBUTES_NUMBER0;
  MPU_AttributesInit.Attributes = INNER_OUTER(MPU_NOT_CACHEABLE);

  HAL_MPU_ConfigMemoryAttributes(&MPU_AttributesInit);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

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
#include "FreeRTOSConfig.h"
#include "i2c.h"
#include "icache.h"
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
#include <stdint.h>
#include "bmp280.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  uint32_t frequencyHZ;
  uint32_t durationMS;
} buzzerParams_td;
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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

// System functions
void Variometer(void *pvParameters);
void Buzzer(buzzerParams_td buzzerParams);

// User button and LED functions
void UserButtonEXTI_Callback();
void UserButtonEXTI_Init();
void UserLed_Init();

// Helper functions
float absf(float x);

void UserLed_Init(){
  GPIO_InitTypeDef  gpio_init_structure;

  LED2_GPIO_CLK_ENABLE();

  gpio_init_structure.Pin   = GPIO_PIN_5;
  gpio_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio_init_structure.Pull  = GPIO_NOPULL;
  gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

  HAL_GPIO_Init(GPIOA, &gpio_init_structure);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

void UserButtonEXTI_Init() {
  BUTTON_USER_GPIO_CLK_ENABLE();

  GPIO_InitTypeDef gpio_init_structure;
  gpio_init_structure.Pin = GPIO_PIN_13;
  gpio_init_structure.Pull = GPIO_PULLDOWN;
  gpio_init_structure.Speed = GPIO_SPEED_FREQ_HIGH;
  gpio_init_structure.Mode = GPIO_MODE_IT_RISING;

  HAL_GPIO_Init(GPIOC, &gpio_init_structure);

  (void)HAL_EXTI_GetHandle(hpb_exti, BUTTON_USER_EXTI_LINE);
  (void)HAL_EXTI_RegisterCallback(hpb_exti, HAL_EXTI_COMMON_CB_ID, (void*) UserButtonEXTI_Callback);

  HAL_NVIC_SetPriority(BUTTON_USER_EXTI_IRQ, BSP_BUTTON_USER_IT_PRIORITY, 0x00);
  HAL_NVIC_EnableIRQ(BUTTON_USER_EXTI_IRQ);
}

// Simple absolute value function for floats
float absf(float x){
  return x < 0 ? -x : x;
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
  UserButtonEXTI_Init();
  UserLed_Init();
  bmp280_init((bmp280_settings_td){
    .config = 0b00010000, // standby 0.5ms, filter x16
    .ctrl_meas = 0b01010111 // temp x2, pressure x16, normal mode
  });
  
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
void UserButtonEXTI_Callback(){
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  // the only interrupt that can trigger this is the user button
  vTaskNotifyGiveFromISR(variometer_task_handle, &xHigherPriorityTaskWoken);
  
  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void PlayStartupTune(){
  buzzerParams_td buzzData = {0};

  for (int i = 0; i < 3; i++){
    buzzData.frequencyHZ = 500 + i*200; // Ascending frequencies
    buzzData.durationMS = 250 - i*50; // Decreasing duration

    Buzzer(buzzData);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void PlaySwitchOffTune(){
  buzzerParams_td buzzData = {0};

  for (int i = 0; i < 5; i++){
    buzzData.frequencyHZ = 1000 - i*150; // Descending frequencies
    buzzData.durationMS = 100 + i*25; // Increasing duration

    Buzzer(buzzData);
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// VARIO CONFIGURATION PARAMETERS

// Valor entre 0 y 1 para actualizar valores de presión.
// Mayor alpha = más rápido pero más ruidoso.
#define ALPHA 0.3f

// Valor entre 0 y 1 para actualizar cambios de altitud.
// Mayor beta = más rápido pero más ruidoso.
#define BETA 0.5f

// Umbrales de velocidad vertical para inciar sonidos. (m/s)
#define CLIMB_RATE_THRESHOLD 0.2f
#define DESCENT_RATE_THRESHOLD -0.2f

// Valores en Hz para configurar tonos de ascenso/descenso.
#define CLIMB_FREQ_BASE 720
#define CLIMB_FREQ_SCALE 800

#define DESCENT_FREQ_BASE 300
#define DESCENT_FREQ_SCALE 100
#define DESCENT_FREQ_MIN 150

#define SEA_LEVEL_PRESSURE_PA 101500.0f

/* MAIN VARIOMETER TASK
 * Switch on/off through user button.
 * Reads, filters and processes pressure data to estimate climb/descent rate.
 * Provides sound feedback through buzzer based on vertical speed.
 * Fully configurable through defined parameters avobe.
 */
void Variometer(void *pvParameters){
switched_off:

  // Wait until it is turned on by button
  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
  printf("Variometer on!\n");
  
  bmp280_td bmp280;
  
  float p0 = 0.0f;
  float pnew = 0.0f;
  float p_prev = 0.0f;
  
  float dp_dt = 0.0f;  // RATE OF PRESSURE CHANGE OVER TIME
  float climb_rate = 0.0f;
  float climb_rate_filt = 0.0f;
  
  // Timing variables for main loop
  float dt = 0.1f;
  TickType_t lastTick;
  
  buzzerParams_td buzzData = {1000, 200};
  
  // Play unique startup sound
  PlayStartupTune();

  // Stabilize initial pressure reading
  for (uint16_t i = 0; i < 30; i++){
    bmp280_read_data(&bmp280);
    p0 += bmp280.pressure_Pa;
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  p0 /= 30.0f;
  pnew = p0;
  p_prev = p0;

  lastTick = xTaskGetTickCount();
  dt = 0.1; // Initial guess

  // Main variometer function loop
  while (1){
    bmp280_read_data(&bmp280);

    // Low-pass filter pressure
    pnew = pnew * (1 - ALPHA) + bmp280.pressure_Pa * ALPHA;

    // Pressure rate (Pa/s)
    dp_dt = (pnew - p_prev) / dt;
    p_prev = pnew;

    // Convert to vertical speed (m/s) using 12Pa ~ 1m approximation 
    climb_rate = -dp_dt * 0.083f;

    // Low-pass filter climb rate
    climb_rate_filt = climb_rate_filt * (1.0f - BETA) + climb_rate * BETA;

    // SOUND FEEDBACK
    if (climb_rate_filt > CLIMB_RATE_THRESHOLD){
      // Frequency increases with climb rate
      buzzData.frequencyHZ = CLIMB_FREQ_BASE + (int)(climb_rate_filt * CLIMB_FREQ_SCALE);
      // Short beep
      buzzData.durationMS = 80;

      Buzzer(buzzData);

      // Faster beeps for stronger climb
      uint32_t delay = 200 - (uint32_t)(climb_rate_filt * 80);
      if (delay < 60) delay = 60;

      vTaskDelay(pdMS_TO_TICKS(delay));
    }
    else if (climb_rate_filt < DESCENT_RATE_THRESHOLD){
      // Lower pitch for descent
      buzzData.frequencyHZ = DESCENT_FREQ_BASE + (int)(climb_rate_filt * DESCENT_FREQ_SCALE);
      if (buzzData.frequencyHZ < DESCENT_FREQ_MIN) buzzData.frequencyHZ = DESCENT_FREQ_MIN;

      buzzData.durationMS = 200;

      Buzzer(buzzData);

      vTaskDelay(pdMS_TO_TICKS(300));
    }
    else{
      // Dead zone, not enough climb or descent to trigger sound
      vTaskDelay(pdMS_TO_TICKS(100));
    }

    
    // Debug print
    if (absf(climb_rate_filt) > 0.1f){
      printf("P: %u Pa | CL: %d cm/s | A: %u\n",
        (unsigned int)bmp280.pressure_Pa,
        (int)(climb_rate_filt*100),
        (unsigned int)bmp280_estimate_altitude(bmp280, SEA_LEVEL_PRESSURE_PA)
      );
    }
    
    // Check if button was pressed to switch off
    if (ulTaskNotifyTake(pdTRUE, 0) != 0){
      printf("Variometer off!\n");
      PlaySwitchOffTune();
      goto switched_off;
    }
  
    // Update dt for next iteration
    dt = (xTaskGetTickCount() - lastTick) / (float)configTICK_RATE_HZ;
    lastTick = xTaskGetTickCount();
  
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

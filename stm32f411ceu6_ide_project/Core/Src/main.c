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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "oled.h"
#include "sht30.h"
#include "ntc.h"
#include "encoder.h"
#include "control.h"
#include "display.h"
#include "fsm.h"
#include "uart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define STATUS_PORT 				GPIOB
#define ERROR_LED_PIN 				GPIO_PIN_13
#define PELTIER_LED_PIN 			GPIO_PIN_14
#define BUZZER_PIN 					GPIO_PIN_15

#define DRIVER_PORT					GPIOA
#define PELTIER_PIN 				GPIO_PIN_2
#define HOT_FAN_PIN 				GPIO_PIN_3

#define COLD_FAN_TIM_HANDLE			&htim3
#define COLD_FAN_CHANNEL 			TIM_CHANNEL_2

#define ENCODER_BUTTON_PIN			GPIO_PIN_4
#define ENCODER_MAIN_TIM_HANDLE		&htim2
#define ENCODER_TICK_TIM_HANDLE		&htim4

#define MAX_SENSOR_ERROR 			5
#define MAX_DISPLAY_ERROR 			10
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
oled_t oled = {0};
sht30_t sht30 = {0};
ntc_t ntc = {0};
encoder_t encoder = {0};
control_t control = {0};
volatile event_e event;
state_e current_state;
event_e local_event;
uint8_t sensor_error = 0;
uint8_t display_error = 0;
uint32_t oled_tick = 0;
uint32_t sensor_tick = 0;
uint32_t uart_tick = 0;
uint32_t error_tick, hot_fan_timeout;
sht30_status_e sht30_status;
ntc_status_e ntc_status;
oled_status_e oled_status;
volatile bool error_pending = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == encoder.button_pin) // Encoder Button is pressed
	{
		if ((HAL_GetTick() - encoder.debounce_tick) >= 200)
		{
			encoder.is_pressed_button = 1;
			encoder.debounce_tick = HAL_GetTick();
		}
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
	if (htim == &htim4)
	{
		encoder_read(&encoder);
		switch(encoder.status)
		{
		case ENCODER_SCROLL_UP:
			event = UP;
			break;
		case ENCODER_SCROLL_DOWN:
			event = DOWN;
			break;
		case ENCODER_PRESSED:
			event = PRESSED;
			break;
		case ENCODER_NONE:
			break;
		}
	}
}

void menu_setting(void)
{
	switch (current_state)
	{
	case SET_HUM_CHOOSE:
		if (local_event == UP) 	control.target_hum = (control.target_hum == 60) ? 60 : control.target_hum + 1;
		else if (local_event == DOWN) control.target_hum = (control.target_hum == 35) ? 35 : control.target_hum - 1;
		break;
	case SET_ALARM_BUZZER_CHOOSE:
		if (local_event == UP) control.is_buzzer = true;
		else if (local_event == DOWN) control.is_buzzer = false;
		break;
	case SET_ALARM_LIMIT_TEMP_CHOOSE:
		if (local_event == UP) control.temp_limit = (control.temp_limit == 100) ? 100 : control.temp_limit + 1;
		else if (local_event == DOWN) control.temp_limit = (control.temp_limit == 0) ? 0 : control.temp_limit - 1;
		break;
	case SET_ALARM_MARGIN_HUM_CHOOSE:
		if (local_event == UP) control.hum_margin = (control.hum_margin == 10) ? 10 : control.hum_margin + 1;
		else if (local_event == DOWN) control.hum_margin = (control.hum_margin == 0) ? 0 : control.hum_margin - 1;
		break;
	case SET_ALARM_DELAY_HUM_CHOOSE:
		if (local_event == UP) control.hum_delay_mins = (control.hum_delay_mins == 120) ? 120 : control.hum_delay_mins + 5;
		else if (local_event == DOWN) control.hum_delay_mins = (control.hum_delay_mins == 0) ? 0 : control.hum_delay_mins - 5;
		break;
	default: break;
	}
}

void error_raise(void)
{
	if (error_pending) return;

	error_pending = true;
	error_tick = HAL_GetTick();
	hot_fan_timeout = HAL_GetTick();

	// stop all the loads except Hot Fan, leave it to cool down Peltier
	HAL_GPIO_WritePin(DRIVER_PORT, PELTIER_PIN, 0);
	HAL_TIM_PWM_Stop(COLD_FAN_TIM_HANDLE, COLD_FAN_CHANNEL);
	HAL_GPIO_WritePin(STATUS_PORT, BUZZER_PIN, 0);
	HAL_GPIO_WritePin(STATUS_PORT, PELTIER_LED_PIN, 0);
	HAL_GPIO_WritePin(DRIVER_PORT, HOT_FAN_PIN, 1);
}

void error_task(void)
{
	if (!error_pending) return;

	if ((HAL_GetTick() - error_tick) >= 500)
	{
		HAL_GPIO_TogglePin(STATUS_PORT, ERROR_LED_PIN);
		error_tick = HAL_GetTick();
	}

	// After 1 minutes of error happening, turn off Hot Fan
	if ((HAL_GetTick() - hot_fan_timeout) >= 60000) HAL_GPIO_WritePin(DRIVER_PORT, HOT_FAN_PIN, 0);
}

void menu_task(void)
{
	__disable_irq();
	local_event = event;
	event = NONE;
	__enable_irq();

	state_e state_before_event = fsm_get();

	if ((state_before_event == RUNNING) && (local_event == PRESSED) && (control.is_alarm_hum || control.is_alarm_temp))
	{
		control_alarm_ack(&control);
		local_event = NONE; // overwrite if has ALARM
	}

	if (error_pending) local_event = ERROR_HAPPEN; // overwrite if has ERROR

	fsm_run(local_event);
	current_state = fsm_get();
	menu_setting();
}

// Because OLED using i2c interrupt so need this function to call error_raise() task
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
	if (oled.i2c == hi2c)
	{
		display_error++;
		if (display_error >= MAX_DISPLAY_ERROR) error_raise();
	}
}

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
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  // OLED
  oled_status = oled_init(&oled, &hi2c1, OLED_ADDR);
  if (oled_status != OLED_OK) error_raise();
  //

  // SHT30
  sht30_status = sht30_init(&sht30, &hi2c1, SHT30_ADDR, sht30_high_repeatability_mode);
  if (sht30_status != SHT30_OK) error_raise();
  //

  // NTC
  ntc_init(&ntc, &hadc1);
  //

  // ENCODER EC11
  encoder_init(&encoder, ENCODER_MAIN_TIM_HANDLE, ENCODER_BUTTON_PIN);
  HAL_TIM_Base_Start_IT(ENCODER_TICK_TIM_HANDLE);
  //

  // POWER DRIVER
  control_init(&control, COLD_FAN_TIM_HANDLE, COLD_FAN_CHANNEL,
		  DRIVER_PORT, PELTIER_PIN, DRIVER_PORT, HOT_FAN_PIN,
		  STATUS_PORT, PELTIER_LED_PIN, STATUS_PORT, BUZZER_PIN);
  //

  // FSM INIT
  fsm_init();
  //

  oled_clear_display(&oled);
  oled_status = oled_send_buffer(&oled);
  if (oled_status != OLED_OK) display_error++;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  menu_task(); // Update current state

	  error_task(); // Handle error

	  if (!error_pending)
	  {
		  control_buzzer_update(&control); // Update buzzer state

		  // Reading sensor values -> Checking alarm and Updating control state
		  if (((HAL_GetTick() - sensor_tick) >= 500) && (HAL_I2C_GetState(sht30.i2c)) == HAL_I2C_STATE_READY)
		  {
			  sht30_status = sht30_get(&sht30);
			  ntc_status = ntc_read_adc(&ntc);

			  if ((sht30_status == SHT30_OK) && (ntc_status == NTC_OK))
			  {
				  sht30_calculate(&sht30);
				  ntc_calculate_temp(&ntc);

				  control_alarm_hum_check(&control, &sht30);
				  control_alarm_temp_check(&control, &sht30);
				  control_update(&control, &ntc, &sht30);

				  sensor_error = 0; // reset sensor error when sht30 and ntc read OK
			  }
			  else
			  {
				  sensor_error++;
				  if (sensor_error >= MAX_SENSOR_ERROR) error_raise();
			  }

			  sensor_tick = HAL_GetTick();
		  }
	  }

	  // Displaying on OLED
	  if ((HAL_GetTick() - oled_tick) >= 250 && (HAL_I2C_GetState(oled.i2c)) == HAL_I2C_STATE_READY)
	  {
		  display_update(current_state, &oled, &sht30, &control);
		  oled_status = oled_send_buffer(&oled);

		  if (oled_status == OLED_OK)
		  {
			  display_error = 0; // reset display error when oled displays OK
		  }
		  else
		  {
			  display_error++;
			  if (display_error >= MAX_DISPLAY_ERROR) error_raise();
		  }

		  oled_tick = HAL_GetTick();
	  }

	  if ((HAL_GetTick() - uart_tick) >= 500)
	  {
		  uart_send_int(HAL_GetTick());
		  uart_send_char(',');
		  uart_send_int(sht30.hum*100);
		  uart_send_char(',');
		  uart_send_int(control.target_hum*100);
		  uart_send_char(',');
		  uart_send_int(control.debug_turn_on*100);
		  uart_send_char(',');
		  uart_send_int(control.debug_turn_off*100);
		  uart_send_char(',');
		  uart_send_int(control.debug_peltier);
		  uart_send_char(',');
		  uart_send_int(control.debug_cold_fan);
		  uart_send_char(',');
		  uart_send_int(ntc.temp*100);
		  uart_send_char(',');
		  uart_send_int(control.debug_dewpoint*100);
		  uart_send_char(',');
		  uart_send_int(control.state_elapsed_ms);
		  uart_send_char(',');
		  uart_send_int(control.debug_profile);
		  uart_send_char(',');
		  uart_send_int(control.debug_condensing_started);
		  uart_send_char(',');
		  uart_send_int(control.debug_fan_boost_allowed);
		  uart_send_char(',');
		  uart_send_int(sht30.temp*100);
		  uart_send_string("\r\n");
		  uart_tick = HAL_GetTick();
	  }
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 15;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 15;
  if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 79;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 15;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 1999;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2|GPIO_PIN_3, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA2 PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB13 PB14 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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

#ifdef  USE_FULL_ASSERT
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

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
#include <stdio.h>  // sscanf ve sprintf için
#include <string.h> // metin uzunluklarını ölçmek için
#include <stdlib.h> // genel matematik işlemleri için
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
// --- HABERLEŞME DEĞİŞKENLERİ ---
uint8_t rx_data;             // Gelen tek harf
uint8_t rx_buffer[50];       // Gelen cümlenin tamamı
uint8_t rx_index = 0;        // Sıra numarası
uint8_t komut_geldi_bayragi = 0; // Mesaj tamamlandı mı?

// --- SİSTEM AYARLARI ---
// Başlangıçta 416 (5V Modu) olsun ki güvenli başlasın.
uint16_t voltaj_limiti = 416;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// --- 1. VOLTAJ MODU KONTROLÜ ---
void Modu_Kontrol_Et(void) {
    // Switch GND'ye (Aşağı) çekiliyse -> PIN 0 (Low) okunur -> 5V MODU
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
        // 12V girişin %41.6'sı (Yaklaşık 5V çıkış)
        voltaj_limiti = 416;
    }
    // Switch Boşta (Yukarı) ise -> PIN 1 (High/Pull-up) okunur -> 12V MODU
    else {
        // Tam güç (12V çıkış)
        voltaj_limiti = 1000;
    }
}

// --- 2. MOTOR SÜRME FONKSİYONU ---
// Özellikler:
// - Minimum PWM Eşiği YOK (Lineer Hesap).
// - TB67 Sürücüsü için Geri Yön Düzeltmesi VAR.
// --- 2. MOTOR SÜRME FONKSİYONU (TB67H451FNG Datasheet Uyumlu) ---
void Motoru_Sur(uint8_t motor_id, uint8_t yon, uint8_t hiz) {

    // Güvenlik: Hız 100'den büyük olamaz.
    if (hiz > 100) hiz = 100;

    uint32_t hedef_pwm = 0;
    uint32_t final_pulse = 0;

    // 1. ADIM: Hedef PWM'i Voltaj Limitine Göre Hesapla
    // 5V Modu: (100 * 416) / 100 = 416 Pulse
    // 12V Modu: (100 * 1000) / 100 = 1000 Pulse
    if (hiz > 0) {
        hedef_pwm = (hiz * voltaj_limiti) / 100;
    } else {
        hedef_pwm = 0;
    }

    // 2. ADIM: Sürücü Mantığına Göre Pulse Ayarla
    // TB67H451FNG'de bir pin High iken diğerini PWM'lersek mantık ters işler.

    if (yon == 0) {
        // --- DURUM A: Yön Pini LOW (0) ---
        // IN1 = 0, IN2 = PWM
        // PWM High oldukça motor hızlanır (Düz Mantık).
        // Modlar: Stop (L/L) <-> Reverse (L/H)
        final_pulse = hedef_pwm;
    }
    else {
        // --- DURUM B: Yön Pini HIGH (1) ---
        // IN1 = 1, IN2 = PWM
        // PWM LOW oldukça motor hızlanır (Ters Mantık).
        // Modlar: Forward (H/L) <-> Brake (H/H)
        // 5V istiyorsak (416 güç), sinyalin %41'i LOW, %59'u HIGH olmalı.
        final_pulse = 1000 - hedef_pwm;
    }

    // 3. ADIM: Pinlere Uygula
    switch (motor_id) {
        case 1: // M1
            HAL_GPIO_WritePin(M1_DIR_GPIO_Port, M1_DIR_Pin, (yon ? GPIO_PIN_SET : GPIO_PIN_RESET));
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, final_pulse);
            break;

        case 2: // M2
            HAL_GPIO_WritePin(M2_DIR_GPIO_Port, M2_DIR_Pin, (yon ? GPIO_PIN_SET : GPIO_PIN_RESET));
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, final_pulse);
            break;

        case 3: // M3
            HAL_GPIO_WritePin(M3_DIR_GPIO_Port, M3_DIR_Pin, (yon ? GPIO_PIN_SET : GPIO_PIN_RESET));
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, final_pulse);
            break;

        case 4: // M4
            HAL_GPIO_WritePin(M4_DIR_GPIO_Port, M4_DIR_Pin, (yon ? GPIO_PIN_SET : GPIO_PIN_RESET));
            __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, final_pulse);
            break;
    }
}// --- 3. UART KESME (RX Callback) ---
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        // Satır sonu (Enter) karakteri geldi mi?
        if (rx_data == '\n' || rx_data == '\r') {
            rx_buffer[rx_index] = 0; // Stringi kapat
            komut_geldi_bayragi = 1; // Main'e haber ver
            rx_index = 0;            // Sayacı sıfırla
        } else {
            // Buffer taşmasın diye kontrol et
            if (rx_index < 49) {
                rx_buffer[rx_index++] = rx_data; // Harfi kaydet
            }
        }
        // Kesmeyi tekrar aktifleştir
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
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
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  // 1. Dört PWM kanalını da başlatıyoruz (Motorlara sinyal gitmeye başlar ama hız 0'dır)
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);

    // 2. UART Dinlemesini Başlatıyoruz (Kulağını aç, veri gelirse kesmeye git)
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);

    // 3. Başlangıç mesajı gönderelim (Opsiyonel ama hoş durur)
    char *msg = "Sistem Hazir! Komut Bekleniyor...\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 100);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  // ADIM A: Switch Durumunu Kontrol Et (Sürekli)
	      Modu_Kontrol_Et();

	      // ADIM B: UART'tan Komut Geldi mi?
	      if (komut_geldi_bayragi == 1) {
	          komut_geldi_bayragi = 0; // Bayrağı indir

	          int m_id, m_hiz;
	          char m_yon_char; // 'F' veya 'R'

	          // Gelen veriyi parçala: Örn "M1 F 100"
	          if (sscanf((char*)rx_buffer, "M%d %c %d", &m_id, &m_yon_char, &m_hiz) == 3) {

	              // Yön Harfini Sayıya Çevir: R=1(Geri), Diğerleri=0(İleri)
	              uint8_t yon_kodu = 0;
	              if (m_yon_char == 'R' || m_yon_char == 'r') {
	                  yon_kodu = 1;
	              }

	              // Motoru Sür
	              Motoru_Sur(m_id, yon_kodu, m_hiz);

	              // Geri Bildirim Gönder (Limit bilgisini de içerir)
	              char tx_msg[64];
	              sprintf(tx_msg, "OK -> Motor:%d Yon:%c Hiz:%d (Limit:%d)\r\n",
	                      m_id, m_yon_char, m_hiz, voltaj_limiti);
	              HAL_UART_Transmit(&huart1, (uint8_t*)tx_msg, strlen(tx_msg), 100);

	          } else {
	              // Hatalı Format Uyarısı
	              char *err = "HATA: Format 'M1 F 100' olmali.\r\n";
	              HAL_UART_Transmit(&huart1, (uint8_t*)err, strlen(err), 100);
	          }
	      }

	      // İşlemciyi çok yormamak için kısa bekleme
	      HAL_Delay(10);

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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
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
  hi2c1.Init.Timing = 0x40B285C2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  htim3.Init.Prescaler = 2;
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
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

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
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(M1_DIR_GPIO_Port, M1_DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, M2_DIR_Pin|M3_DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(M4_DIR_GPIO_Port, M4_DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : SW_VOLTAGE_Pin */
  GPIO_InitStruct.Pin = SW_VOLTAGE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SW_VOLTAGE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : M1_DIR_Pin */
  GPIO_InitStruct.Pin = M1_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(M1_DIR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : M2_DIR_Pin M3_DIR_Pin */
  GPIO_InitStruct.Pin = M2_DIR_Pin|M3_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : M4_DIR_Pin */
  GPIO_InitStruct.Pin = M4_DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(M4_DIR_GPIO_Port, &GPIO_InitStruct);

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

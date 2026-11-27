/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2026 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l0xx_hal.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "stm32l0538_discovery_epd.h"

/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

TSC_HandleTypeDef htsc;

UART_HandleTypeDef huart1;

PCD_HandleTypeDef hpcd_USB_FS;

/* USER CODE BEGIN PV */
volatile float stts_temp_C = 0.0f;
volatile float hts_feuchte_pct = 0.0f;
volatile float hts_temp_C = 0.0f;
volatile float lps_druck_hPa = 0.0f;
volatile float lps_temp_C = 0.0f;

volatile uint8_t ok_stts = 99;
volatile uint8_t ok_hts  = 99;
volatile uint8_t ok_lps  = 99;

// Min/Max-Speicher seit letztem Reset
float min_temp =  999.0f, max_temp = -999.0f;
float min_rh   =  999.0f, max_rh   = -999.0f;
float min_p    = 9999.0f, max_p    =    0.0f;
/* USER CODE END PV */


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_TSC_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USB_PCD_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
void split_float(float val, int *vorkomma, int *nachkomma) {
    *vorkomma = (int)val;
    int d = (int)((val - *vorkomma) * 10.0f);
    if (d < 0) d = -d;
    *nachkomma = d;
}
/* USER CODE END 0 */


int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

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
  //MX_SPI1_Init();
  MX_SPI2_Init();
  MX_TSC_Init();
  MX_USART1_UART_Init();
  MX_USB_PCD_Init();

  /* USER CODE BEGIN 2 */
  BSP_EPD_Init();
  BSP_EPD_Clear(EPD_COLOR_WHITE);
  BSP_EPD_SetFont(&Font12);
  uint16_t sekunden_zaehler = 300;
  uint8_t display_seite = 0;        // 0 = Hauptfenster, 1 = Alle Sensoren, 2 = Min/Max
  uint16_t zehntel_sekunden = 3000; // Startet bei 3000 -> erster Refresh sofort!
  uint8_t taster_alt = 0;


  // I2C1 einmal sauber zurücksetzen, falls der Bus nach einem Reset noch hing
  __HAL_RCC_I2C1_FORCE_RESET();
  HAL_Delay(5);
  __HAL_RCC_I2C1_RELEASE_RESET();
  MX_I2C1_Init();
  HAL_Delay(20);

  // 1. STTS751 (0x3B) starten (12-Bit, kontinuierlich)
  uint8_t stts_cfg = 0x08;
  HAL_I2C_Mem_Write(&hi2c1, (0x3B << 1), 0x03, I2C_MEMADD_SIZE_8BIT, &stts_cfg, 1, 100);

  // 2. HTS221 (0x5F) Kalibrierwerte lesen & einschalten
  uint8_t cal_buf[16] = {0};
  uint8_t *cal = cal_buf;
  HAL_I2C_Mem_Read(&hi2c1, (0x5F << 1), 0x30 | 0x80, I2C_MEMADD_SIZE_8BIT, cal, 16, 100);

  uint8_t h0_rh_x2 = *(cal + 0);
  uint8_t h1_rh_x2 = *(cal + 1);
  int16_t h0_t0_out = (int16_t)(*(cal + 6) | (*(cal + 7) << 8));
  int16_t h1_t0_out = (int16_t)(*(cal + 10) | (*(cal + 11) << 8));

  uint16_t t0_degC_x8 = *(cal + 2) | ((*(cal + 5) & 0x03) << 8);
  uint16_t t1_degC_x8 = *(cal + 3) | ((*(cal + 5) & 0x0C) << 6);
  int16_t t0_out = (int16_t)(*(cal + 12) | (*(cal + 13) << 8));
  int16_t t1_out = (int16_t)(*(cal + 14) | (*(cal + 15) << 8));

  uint8_t hts_ctrl = 0x81;
  HAL_I2C_Mem_Write(&hi2c1, (0x5F << 1), 0x20, I2C_MEMADD_SIZE_8BIT, &hts_ctrl, 1, 100);

  // 3. LPS33HW (0x5D) einschalten (10 Hz + BDU)
  uint8_t lps_ctrl = 0x22;
  HAL_I2C_Mem_Write(&hi2c1, (0x5D << 1), 0x10, I2C_MEMADD_SIZE_8BIT, &lps_ctrl, 1, 100);

  // Einmaliges Start-Blinken beim Booten (grüne LED LD3 an PB4)
  for (int blink = 0; blink < 5; blink++) {
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);   // LED an
	  HAL_Delay(200);                                       // 300 ms leuchten lassen
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET); // LED wieder aus
	  HAL_Delay(200);
  }


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  /* USER CODE BEGIN 3 */
	  // --- A) Alle 1,0 Sekunden (alle 10 Durchläufe) Sensoren auslesen & Min/Max aktualisieren ---
	  if (zehntel_sekunden % 10 == 0)
	  {
	      // HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_4); //blinkt bei jedem Durchlauf

	      // 1. STTS751 (0x3B)
	      int8_t t_hi = 0; uint8_t t_lo = 0;
	      ok_stts = HAL_I2C_Mem_Read(&hi2c1, (0x3B << 1), 0x00, I2C_MEMADD_SIZE_8BIT, (uint8_t*)&t_hi, 1, 100);
	      HAL_I2C_Mem_Read(&hi2c1, (0x3B << 1), 0x02, I2C_MEMADD_SIZE_8BIT, &t_lo, 1, 100);
	      if (ok_stts == HAL_OK) {
	          stts_temp_C = (float)t_hi + ((float)(t_lo >> 4) * 0.0625f);
	          if (stts_temp_C < min_temp) min_temp = stts_temp_C;
	          if (stts_temp_C > max_temp) max_temp = stts_temp_C;
	      }

	      // 2. HTS221 (0x5F)
	      uint8_t hts_buf[4] = {0};
	      uint8_t *hts = hts_buf;
	      ok_hts = HAL_I2C_Mem_Read(&hi2c1, (0x5F << 1), 0x28 | 0x80, I2C_MEMADD_SIZE_8BIT, hts, 4, 100);
	      if (ok_hts == HAL_OK) {
	          int16_t h_out = (int16_t)(*(hts + 0) | (*(hts + 1) << 8));
	          int16_t t_out = (int16_t)(*(hts + 2) | (*(hts + 3) << 8));

	          float h0 = h0_rh_x2 / 2.0f, h1 = h1_rh_x2 / 2.0f;
	          if (h1_t0_out != h0_t0_out) {
	              hts_feuchte_pct = h0 + (float)(h_out - h0_t0_out) * (h1 - h0) / (float)(h1_t0_out - h0_t0_out);
	              if (hts_feuchte_pct < min_rh) min_rh = hts_feuchte_pct;
	              if (hts_feuchte_pct > max_rh) max_rh = hts_feuchte_pct;
	          }
	          float t0 = t0_degC_x8 / 8.0f, t1 = t1_degC_x8 / 8.0f;
	          if (t1_out != t0_out) {
	              hts_temp_C = t0 + (float)(t_out - t0_out) * (t1 - t0) / (float)(t1_out - t0_out);
	          }
	      }

	      // 3. LPS33HW (0x5D)
	      uint8_t lps_buf[5] = {0};
	      uint8_t *lps = lps_buf;
	      ok_lps = HAL_I2C_Mem_Read(&hi2c1, (0x5D << 1), 0x28, I2C_MEMADD_SIZE_8BIT, lps, 5, 100);
	      if (ok_lps == HAL_OK) {
	          int32_t p_raw = (int32_t)((uint32_t)*(lps + 0) | ((uint32_t)*(lps + 1) << 8) | ((uint32_t)*(lps + 2) << 16));
	          if (p_raw & 0x00800000) p_raw |= 0xFF000000;
	          lps_druck_hPa = (float)p_raw / 4096.0f;
	          if (lps_druck_hPa > 300.0f) { // Plausibilitäts-Check gegen 0.0 beim Start
	              if (lps_druck_hPa < min_p) min_p = lps_druck_hPa;
	              if (lps_druck_hPa > max_p) max_p = lps_druck_hPa;
	          }

	          int16_t lps_t_raw = (int16_t)(*(lps + 3) | (*(lps + 4) << 8));
	          lps_temp_C = (float)lps_t_raw / 100.0f;
	      }

	      // --- Alle 5 Sensorwerte jede Sekunde per UART1 (PA9) senden ---
	            int ti, td, hti, htd, lti, ltd, rhi, rhd, pi, pd;
	            split_float(stts_temp_C, &ti, &td);
	            split_float(hts_temp_C, &hti, &htd);
	            split_float(lps_temp_C, &lti, &ltd);
	            split_float(hts_feuchte_pct, &rhi, &rhd);
	            split_float(lps_druck_hPa, &pi, &pd);

	            char uart_buf[96];
	            int len = sprintf(uart_buf, "STTS:%d.%dC | HTS:%d.%dC, %d.%d %% | LPS:%d.%dC, %d.%dhPa\r\n",
	                              ti, td, hti, htd, rhi, rhd, lti, ltd, pi, pd);

	            HAL_UART_Transmit(&huart1, (uint8_t*)uart_buf, len, 100);
	      }

	  // --- B) User-Taster B1 (PA0) abfragen (steigende Flanke -> Seite wechseln) ---
	  uint8_t taster_neu = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) ? 1 : 0;
	  uint8_t sofort_refresh = 0;

	  if (taster_neu == 1 && taster_alt == 0)
	  {
	      display_seite = (display_seite + 1) % 3; // Wechselt 0 -> 1 -> 2 -> 0
	      sofort_refresh = 1;
	  }
	  taster_alt = taster_neu;

	  // --- C) Display aktualisieren: Entweder bei Tastendruck ODER alle 5 Min (3000 * 100ms) ---
	  if (sofort_refresh || zehntel_sekunden >= 3000)
	  {
		  if (!sofort_refresh)
		  	      {
		  	          display_seite = 0; // Nach 5 Minuten automatisch zurück zum Hauptfenster
		  	      }
		  zehntel_sekunden = 0;
	      BSP_EPD_Clear(EPD_COLOR_WHITE);

	      int ti, td, rhi, rhd, pi, pd;
	      split_float(stts_temp_C, &ti, &td);
	      split_float(hts_feuchte_pct, &rhi, &rhd);
	      split_float(lps_druck_hPa, &pi, &pd);

	      if (display_seite == 0)
	      {
	          // === SEITE 0: Hauptfenster (dein Layout mit großer Temperatur oben) ===
	          char z1[32], z2[32], z3[32];
	          sprintf(z1, "Temp: %d.%d~", ti, td);
	          sprintf(z2, "Feuchte: %d.%d %%", rhi, rhd);
	          sprintf(z3, "Druck: %d.%d hPa", pi, pd);

	          BSP_EPD_SetFont(&Font20);
	          BSP_EPD_DisplayStringAt(2, 11, (uint8_t*)z1, LEFT_MODE);
	          // (Hier kannst du noch deine eigene Grad-Lösung für z1 ergänzen!)

	          BSP_EPD_SetFont(&Font12);
	          BSP_EPD_DisplayStringAt(2, 6,  (uint8_t*)z2, LEFT_MODE);
	          BSP_EPD_DisplayStringAt(2, 1,  (uint8_t*)z3, LEFT_MODE);
	      }
	      else if (display_seite == 1)
	      {
	          // === SEITE 1: Alle 5 Sensorwerte einheitlich in Font12 ===
	          int hti, htd, lti, ltd;
	          split_float(hts_temp_C, &hti, &htd);
	          split_float(lps_temp_C, &lti, &ltd);

	          char s1[32], s2[32], s3[32], s4[32], s5[32];
	          sprintf(s1, "STTS Temp:    %d.%d C", ti, td);
	          sprintf(s2, "HTS  Temp:    %d.%d C", hti, htd);
	          sprintf(s3, "LPS  Temp:    %d.%d C", lti, ltd);
	          sprintf(s4, "HTS  Feuchte: %d.%d %%", rhi, rhd);
	          sprintf(s5, "LPS  Druck.:  %d.%d hPa", pi, pd);

	          BSP_EPD_SetFont(&Font12);
	          BSP_EPD_DisplayStringAt(2, 13, (uint8_t*)s1, LEFT_MODE);
	          BSP_EPD_DisplayStringAt(2, 10, (uint8_t*)s2, LEFT_MODE);
	          BSP_EPD_DisplayStringAt(2, 7,  (uint8_t*)s3, LEFT_MODE);
	          BSP_EPD_DisplayStringAt(2, 4,  (uint8_t*)s4, LEFT_MODE);
	          BSP_EPD_DisplayStringAt(2, 1,  (uint8_t*)s5, LEFT_MODE);
	      }
	      else
	      {
	          // === SEITE 2: Min / Max seit Reset (Font12) ===
	          int t_min_i, t_min_d, t_max_i, t_max_d;
	          int rh_min_i, rh_min_d, rh_max_i, rh_max_d;
	          int p_min_i, p_min_d, p_max_i, p_max_d;

	          split_float(min_temp, &t_min_i, &t_min_d);
	          split_float(max_temp, &t_max_i, &t_max_d);
	          split_float(min_rh, &rh_min_i, &rh_min_d);
	          split_float(max_rh, &rh_max_i, &rh_max_d);
	          split_float(min_p, &p_min_i, &p_min_d);
	          split_float(max_p, &p_max_i, &p_max_d);

	          char m0[32], m1[32], m2[32], m3[32];
	          sprintf(m0, " MIN / MAX seit Reset");
	          sprintf(m1, "Temp.:   %d.%d / %d.%d C",   t_min_i, t_min_d, t_max_i, t_max_d);
	          sprintf(m2, "Feuchte: %d.%d / %d.%d %%",  rh_min_i, rh_min_d, rh_max_i, rh_max_d);
	          sprintf(m3, "Druck:   %d / %d hPa",       p_min_i, p_max_i);

	          BSP_EPD_SetFont(&Font12);
	          BSP_EPD_DisplayStringAt(2, 14, (uint8_t*)m0, LEFT_MODE);
	          BSP_EPD_DisplayStringAt(2, 10, (uint8_t*)m1, LEFT_MODE);
	          BSP_EPD_DisplayStringAt(2, 6,  (uint8_t*)m2, LEFT_MODE);
	          BSP_EPD_DisplayStringAt(2, 2,  (uint8_t*)m3, LEFT_MODE);
	      }

	      BSP_EPD_RefreshDisplay();
	  }

	  zehntel_sekunden++;
	  HAL_Delay(100);
	  /* USER CODE END 3 */

  }
}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Configure the main internal regulator output voltage 
    */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_HSI48;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLLMUL_12;
  RCC_OscInitStruct.PLL.PLLDIV = RCC_PLLDIV_3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_I2C1
                              |RCC_PERIPHCLK_USB;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* I2C1 init function */
static void MX_I2C1_Init(void)
{

  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00707CBB; // 100 kHz Standard Mode (entspanntes Timing für alle 3 Boards)
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure Analogue filter 
    */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure Digital filter 
    */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* SPI1 init function */
static void MX_SPI1_Init(void)
{

  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_1LINE;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* SPI2 init function */
static void MX_SPI2_Init(void)
{

  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* TSC init function */
static void MX_TSC_Init(void)
{

    /**Configure the TSC peripheral 
    */
  htsc.Instance = TSC;
  htsc.Init.CTPulseHighLength = TSC_CTPH_2CYCLES;
  htsc.Init.CTPulseLowLength = TSC_CTPL_2CYCLES;
  htsc.Init.SpreadSpectrum = DISABLE;
  htsc.Init.SpreadSpectrumDeviation = 1;
  htsc.Init.SpreadSpectrumPrescaler = TSC_SS_PRESC_DIV1;
  htsc.Init.PulseGeneratorPrescaler = TSC_PG_PRESC_DIV64;
  htsc.Init.MaxCountValue = TSC_MCV_255;
  htsc.Init.IODefaultMode = TSC_IODEF_OUT_PP_LOW;
  htsc.Init.SynchroPinPolarity = TSC_SYNC_POLARITY_FALLING;
  htsc.Init.AcquisitionMode = TSC_ACQ_MODE_NORMAL;
  htsc.Init.MaxCountInterrupt = DISABLE;
  htsc.Init.ChannelIOs = TSC_GROUP1_IO3|TSC_GROUP2_IO3|TSC_GROUP3_IO2;
  htsc.Init.SamplingIOs = TSC_GROUP1_IO4|TSC_GROUP2_IO4|TSC_GROUP3_IO3;
  if (HAL_TSC_Init(&htsc) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* USART1 init function */
static void MX_USART1_UART_Init(void)
{

  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/* USB init function */
static void MX_USB_PCD_Init(void)
{

  hpcd_USB_FS.Instance = USB;
  hpcd_USB_FS.Init.dev_endpoints = 8;
  hpcd_USB_FS.Init.speed = PCD_SPEED_FULL;
  hpcd_USB_FS.Init.ep0_mps = DEP0CTL_MPS_64;
  hpcd_USB_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_FS.Init.battery_charging_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_FS) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD_R_GPIO_Port, LD_R_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, ePD1_RESET_Pin|ePD1_PWR_ENn_Pin|ePD1_D_C_Pin|LD_G_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : MFX_IRQ_OUT_Pin */
  GPIO_InitStruct.Pin = MFX_IRQ_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MFX_IRQ_OUT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MFX_WAKEUP_Pin ePD1_BUSY_Pin */
  GPIO_InitStruct.Pin = MFX_WAKEUP_Pin|ePD1_BUSY_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD_R_Pin */
  GPIO_InitStruct.Pin = LD_R_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD_R_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : ePD1_RESET_Pin ePD1_PWR_ENn_Pin ePD1_D_C_Pin LD_G_Pin */
  GPIO_InitStruct.Pin = ePD1_RESET_Pin|ePD1_PWR_ENn_Pin|ePD1_D_C_Pin|LD_G_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void _Error_Handler(char * file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler_Debug */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

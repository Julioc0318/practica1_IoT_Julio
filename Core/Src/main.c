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
#include "can.h"
#include "gpio.h"
#include "rtc.h"
#include "usart.h"
#include "usb.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#define NODE_ID 0x123
#define OFFER_MSG_ID 0x0134

#define INICIAL_DELAY_MS 50
#define MAX_DELAY_MS 3200
#define OFFER_PERIOD_MS 1000
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

/* USER CODE BEGIN PV */
// 这里专门写全局变量
CAN_TxHeaderTypeDef TxHeader;                 // 发送帧头结构体
CAN_RxHeaderTypeDef RxHeader;                 // 接收帧头结构体
uint8_t TxData[8];                            // 发送数据缓冲区（最多8字节）
uint8_t RxData[8];                            // 接收数据缓冲区（最多8字节）
uint32_t TxMailbox;                           // 发送邮箱变量
uint16_t current_delay_ms = INICIAL_DELAY_MS; // 当前目标 ID
static volatile uint32_t last_offer_tick = 0U;
static volatile uint32_t ld_off_tick = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// 这里专门写函数声明
HAL_StatusTypeDef CAN_Configurar_Filtrado(void);
// 当 FIFO0 收到新报文触发中断时的回调函数
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);
// 发送数据帧或远程帧的函数声明
HAL_StatusTypeDef CAN_TX_SendFrame(uint32_t stdId, uint8_t *pData, uint8_t len);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// 这里专门写函数体
HAL_StatusTypeDef CAN_Configurar_Filtrado(void) {
  CAN_FilterTypeDef sFilterConfig = {0};

  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;

  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterIdHigh = (OFFER_MSG_ID << 5);
  sFilterConfig.FilterMaskIdHigh = (0x7FF << 5);

  if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK) {
    Error_Handler();
  }

  return HAL_OK;
}

HAL_StatusTypeDef CAN_TX_SendDataFrame(uint32_t stdId, unit_t *pData,
                                       uint8_t len) {
  TxHeader.StdId = stdId;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_DATA;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = len;
  TxHeader.TransmitGlobalTime = DISABLE;

  return HAL_CAN_AddTxMessage(&hcan, &TxHeader, pData, &TxMailbox);
}

// 无需手动挂载或注册到某个地方
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  // 自动从 FIFO0 提取收到的报文头和数据
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK) {
    if (RxHeader.StdId == OFFERED_MSG_ID && RxHeader.RTR == CAN_RTR_REMOTE) {
      uint8_t resp_data[2];
      resp_data[0] = (uint_8)(current_delay_ms & 0xFF);
      resp_data[1] = (uint_8)((current_delay_ms >> 8) & 0xFF);

      CAN_TX_SendFrame(OFFER_MSG_ID, resp_data, 2)

          current_delay_ms *= 2;
      if (current_delay_ms > MAX_DELAY_MS) {
        current_delay_ms = INICIAL_DELAY_MS;
      }

      HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_SET);
      ld_off_tick = HAL_GetTick() + 100U;
    }
  }
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU
   * Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the
   * Systick.
   */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_RTC_Init();
  MX_USART3_UART_Init();
  MX_USB_PCD_Init();
  MX_CAN_Init();

  /* USER CODE BEGIN 2 */
  // 定义按钮上次状态变量位未按下
  GPIO_PinState last_btn = GPIO_PIN_RESET;

  CAN_Configurar_Filtrado(0);

  // 启用 NVIC 中与 USB 低优先级共享的 CAN RX0 中断通道。
  HAL_NVIC_SetPriority(USB_LP_CAN_RX0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);

  // 激活 CAN RX FIFO0 消息挂起中断通知
  // 当 FIFO0 收到新报文时，触发中断，调用 HAL_CAN_RxFifo0MsgPendingCallback()
  // 回调函数
  if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) !=
      HAL_OK) {
    Error_Handler();
  }

  // 启动 CAN 硬件控制器
  if (HAL_CAN_Start(&hcan) != HAL_OK) {
    Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // 这里是业务逻辑
    uint32_t current_tick = HAL_GetTick();
    if (current_tick - last_offer_tick >= OFFER_PERIOD_MS) {
      last_offer_tick = current_tick;

      uint8_t offer_data[2];
      offer_data[0] = (uint_8)((OFFER_MSG_ID >> 8) & 0xFF);
      offer_data[1] = (uint_8)(OFFER_MSG_ID & 0xFF);

      CAN_TX_SendDataFrame(NODE_ID, offer_data, 2);
    }

    if (ld_off_tick != 0U && current_tick - ld_off_tick >= 0) {
      HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN\RESET);
      ld3_off_tick = 0U;
    }

    HAL_Delay(10);
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection =
      RCC_PERIPHCLK_USB | RCC_PERIPHCLK_USART3 | RCC_PERIPHCLK_RTC;
  PeriphClkInit.Usart3ClockSelection = RCC_USART3CLKSOURCE_PCLK1;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state
   */
  __disable_irq();
  while (1) {
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
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
     file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
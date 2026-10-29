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
CAN_TxHeaderTypeDef TxHeader; // 发送帧头结构体
CAN_RxHeaderTypeDef RxHeader; // 接收帧头结构体
uint8_t TxData[8];            // 发送数据缓冲区（最多8字节）
uint8_t RxData[8];            // 接收数据缓冲区（最多8字节）
uint32_t TxMailbox;           // 发送邮箱变量
uint32_t msg_count = 0;
uint16_t current_target_id = 0; // 当前目标 ID
uint8_t offer_received = 0;
static volatile uint32_t ld1_off_tick = 0U;
static volatile uint32_t ld2_off_tick = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// 这里专门写函数声明
HAL_StatusTypeDef CAN_Configurar_Filtrado(uint16_t target_id);
// 当 FIFO0 收到新报文触发中断时的回调函数
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan);
// 发送数据帧或远程帧的函数声明
HAL_StatusTypeDef CAN_TX_SendFrame(uint32_t stdId, uint8_t *pData, uint8_t len);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// 这里专门写函数体
HAL_StatusTypeDef CAN_Configurar_Filtrado(uint16_t target_id) {
  CAN_FilterTypeDef sFilterConfig = {0};

  sFilterConfig.FilterMode = CAN_FILTERMODE_IDMASK;
  sFilterConfig.FilterScale = CAN_FILTERSCALE_32BIT;
  sFilterConfig.FilterIdLow = 0x0000;
  sFilterConfig.FilterMaskIdLow = 0x0000;
  sFilterConfig.FilterFIFOAssignment = CAN_RX_FIFO0;
  sFilterConfig.FilterActivation = ENABLE;
  sFilterConfig.SlaveStartFilterBank = 14;

  sFilterConfig.FilterBank = 0;
  sFilterConfig.FilterIdHigh = (0x000 << 5);
  sFilterConfig.FilterMaskIdHigh = (0x780 << 5);

  if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK) {
    Error_Handler();
  }

  if (target_id >= 128) {
    sFilterConfig.FilterBank = 1;
    sFilterConfig.FilterIdHigh = (target_id << 5);
    sFilterConfig.FilterMaskIdHigh = (0x7FF << 5);
  }

  if (HAL_CAN_ConfigFilter(&hcan, &sFilterConfig) != HAL_OK) {
    Error_Handler();
  }

  return HAL_OK;
}

HAL_StatusTypeDef CAN_TX_SendRemoteFrame(uint32_t stdId) {
  TxHeader.StdId = stdId;
  TxHeader.ExtId = 0;
  TxHeader.RTR = CAN_RTR_REMOTE;
  TxHeader.IDE = CAN_ID_STD;
  TxHeader.DLC = 0;
  TxHeader.TransmitGlobalTime = DISABLE;

  return HAL_CAN_AddTxMessage(&hcan, &TxHeader, NULL, &TxMailbox);
}

// 无需手动挂载或注册到某个地方
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  // 自动从 FIFO0 提取收到的报文头和数据
  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &RxHeader, RxData) == HAL_OK) {
    msg_count++;
    if (RxHeader.StdId < 128 && RxHeader.RTR == CAN_RTR_DATA &&
        RxHeader.DLC == 2) {
      uint16_t new_target_id = (RxData[0] << 8) | RxData[1];
      HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_SET);
      ld1_off_tick = HAL_GetTick() + 100U;
      if (new_target_id >= 128 && new_target_id != current_target_id) {
        current_target_id = new_target_id;
        offer_received = 1;
        CAN_Configurar_Filtrado(current_target_id);
      }
    } else if (offer_received && RxHeader.StdId == current_target_id &&
               RxHeader.RTR == CAN_RTR_DATA && RxHeader.DLC > 2) {
      uint16_t delay_ms = (uint16_t)((RxData[0] << 8) | RxData[1]);
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
      ld2_off_tick = HAL_GetTick() + (uint32_t)delay_ms;
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
    if (ld1_off_tick != 0U && (int32_t)(HAL_GetTick() - ld1_off_tick) >= 0) {
      HAL_GPIO_WritePin(LD1_GPIO_Port, LD1_Pin, GPIO_PIN_RESET);
      ld1_off_tick = 0U;
    }

    if (ld2_off_tick != 0U && (int32_t)(HAL_GetTick() - ld2_off_tick) >= 0) {
      HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
      ld2_off_tick = 0U;
    }

    // 读取蓝色按键状态
    GPIO_PinState btn = HAL_GPIO_ReadPin(USER_Btn_GPIO_Port, USER_Btn_Pin);

    // 检测到按键按下（上升沿触发）
    if (btn == GPIO_PIN_SET && last_btn == GPIO_PIN_RESET) {
      if (offer_received && current_target_id >= 128) {
        // 发送远程帧请求数据
        CAN_TX_SendRemoteFrame(current_target_id);
      }
    }
    last_btn = btn;

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
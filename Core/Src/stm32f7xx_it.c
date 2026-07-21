/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f7xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "stm32f7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "cmsis_os.h"
#include "app.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if 0
/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern CAN_HandleTypeDef hcan1;
extern TIM_HandleTypeDef htim5;
extern UART_HandleTypeDef huart7;
extern UART_HandleTypeDef huart3;
extern TIM_HandleTypeDef htim7;

/* USER CODE BEGIN EV */
#endif
extern TIM_HandleTypeDef htim7;
/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M7 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  ecu_force_safe_outputs();
  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  ecu_force_safe_outputs();
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  ecu_force_safe_outputs();
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  ecu_force_safe_outputs();
  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  ecu_force_safe_outputs();
  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32F7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f7xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles CAN1 RX0 interrupts.
  */
void CAN1_RX0_IRQHandler(void)
{
  /* USER CODE BEGIN CAN1_RX0_IRQn 0 */
	extern app_data_t app;

	CAN_HandleTypeDef *hcan1 = app.board.canbus.hcan;
#if 0
  /* USER CODE END CAN1_RX0_IRQn 0 */
  HAL_CAN_IRQHandler(&hcan1);
  /* USER CODE BEGIN CAN1_RX0_IRQn 1 */
#endif
  HAL_CAN_IRQHandler(hcan1);
  /* USER CODE END CAN1_RX0_IRQn 1 */
}

/**
  * @brief This function handles USART3 global interrupt.
  */
void USART3_IRQHandler(void)
{
  /* USER CODE BEGIN USART3_IRQn 0 */
	extern app_data_t app;

	UART_HandleTypeDef *huart3 = app.board.cli.huart;
#if 0
  /* USER CODE END USART3_IRQn 0 */
  HAL_UART_IRQHandler(&huart3);
  /* USER CODE BEGIN USART3_IRQn 1 */
#endif
  HAL_UART_IRQHandler(huart3);
  /* USER CODE END USART3_IRQn 1 */
}

/**
  * @brief This function handles TIM5 global interrupt.
  */
void TIM5_IRQHandler(void)
{
  /* USER CODE BEGIN TIM5_IRQn 0 */
	extern app_data_t app;

	TIM_HandleTypeDef *htim5 = app.board.cool_flow.htim;
#if 0
  /* USER CODE END TIM5_IRQn 0 */
  HAL_TIM_IRQHandler(&htim5);
  /* USER CODE BEGIN TIM5_IRQn 1 */
#endif
  HAL_TIM_IRQHandler(htim5);
  /* USER CODE END TIM5_IRQn 1 */
}

/**
  * @brief This function handles TIM7 global interrupt.
  */
void TIM7_IRQHandler(void)
{
  /* USER CODE BEGIN TIM7_IRQn 0 */

  /* USER CODE END TIM7_IRQn 0 */
  HAL_TIM_IRQHandler(&htim7);
  /* USER CODE BEGIN TIM7_IRQn 1 */

  /* USER CODE END TIM7_IRQn 1 */
}

/**
  * @brief This function handles UART7 global interrupt.
  */
void UART7_IRQHandler(void)
{
  /* USER CODE BEGIN UART7_IRQn 0 */
	extern app_data_t app;

	UART_HandleTypeDef *huart7 = app.board.stm32f767.huart7;
#if 0
  /* USER CODE END UART7_IRQn 0 */
  HAL_UART_IRQHandler(&huart7);
  /* USER CODE BEGIN UART7_IRQn 1 */
#endif
  HAL_UART_IRQHandler(huart7);
  /* USER CODE END UART7_IRQn 1 */
}

/* USER CODE BEGIN 1 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	extern app_data_t app;
	cli_t *cli = &app.board.cli;
	HAL_StatusTypeDef ret;

	if((cli->huart != NULL) && (cli->huart->Instance == huart->Instance))
	{
		/* Preserve a completed command until the CLI task has copied it.  New
		 * bytes are discarded while pending instead of overwriting that line. */
		if(cli->msg_pending)
		{
			/* Nothing to mutate. */
		}
		else if(cli->c == '\r')
		{
			cli->line[cli->index] = '\0';
			cli->index = 0u;
			if(strnlen(cli->line, CLI_LINESZ) > 0u)
			{
				cli->msg_pending = true;
				cli->msg_count++;
			}
		}
		else if(cli->c == '\n')
		{
			/* Ignore line-feed. Carriage-return terminates CLI commands. */
		}
		else if((cli->c == 127u) || (cli->c == '\b'))
		{
			if(cli->index != 0u)
			{
				cli->index--;
				cli->line[cli->index] = '\0';
			}
		}
		else if((cli->c >= 32u) && (cli->c <= 126u))
		{
			if(cli->index < (CLI_LINESZ - 1u))
			{
				cli->line[cli->index++] = (char)cli->c;
			}
		}

		ret = HAL_UART_Receive_IT(cli->huart, (uint8_t *)&cli->c, 1);
		app.cli_fault = (ret != HAL_OK);
	}
}


void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
	extern app_data_t app;
	canbus_t *canbus = &app.board.canbus;
	canbus_packet_t *rx_packet = &canbus->rx_packet;
    ams_t *ams = &app.board.ams;
	cm200_t *cm200 = &app.board.cm200;

    if((canbus == NULL) || (canbus->hcan == NULL) || (hcan != canbus->hcan))
    {
        return;
    }

    /* Drain a bounded number of frames per callback.  This prevents a busy bus
     * from monopolizing interrupt time while avoiding one-callback/one-frame
     * backlog growth. */
    for(uint8_t received = 0u;
        (received < CANBUS_RX_ISR_BUDGET) &&
        (HAL_CAN_GetRxFifoFillLevel(canbus->hcan, CAN_RX_FIFO0) != 0u);
        received++)
    {
        CAN_RxHeaderTypeDef rx_header = {0};
        uint8_t rx_data[DATALEN] = {0};
        uint32_t std_id;
        bool is_standard;
        bool is_data;
        bool known_ams;
        bool known_cm200;
        bool parsed = false;

        if(HAL_CAN_GetRxMessage(canbus->hcan, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK)
        {
            app.canbus_rx_fault = true;
            return;
        }

        is_standard = (rx_header.IDE == CAN_ID_STD);
        is_data = (rx_header.RTR == CAN_RTR_DATA);
        std_id = rx_header.StdId;
        known_ams = (is_standard && ams_is_known_can_id(std_id));
        known_cm200 = (is_standard && cm200_is_known_can_id(std_id));

        rx_packet->id = is_standard ? std_id : rx_header.ExtId;
        memcpy(rx_packet->data, rx_data, sizeof(rx_packet->data));

        if(!is_data)
        {
            canbus->rx_remote_count++;
            if(known_ams)
            {
                ams->bad_rx_count++;
                ams_invalidate_can_frame(ams, std_id);
            }
            if(known_cm200)
            {
                cm200->bad_rx_count++;
                cm200_invalidate_can_frame(cm200, std_id);
            }
            if(known_ams || known_cm200)
            {
                canbus->rx_malformed_count++;
            }
            else
            {
                canbus->rx_ignored_count++;
            }
            continue;
        }

        if(known_ams)
        {
            parsed = ams_parse_can_frame(ams,
                                         std_id,
                                         true,
                                         (uint8_t)rx_header.DLC,
                                         rx_data,
                                         HAL_GetTick());
        }
        else if(known_cm200)
        {
            parsed = cm200_parse_can_frame(cm200,
                                           std_id,
                                           true,
                                           (uint8_t)rx_header.DLC,
                                           rx_data,
                                           HAL_GetTick());
        }

        if(parsed)
        {
            canbus->rx_accepted_count++;
        }
        else if(known_ams || known_cm200)
        {
            canbus->rx_malformed_count++;
        }
        else
        {
            canbus->rx_ignored_count++;
        }
    }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    extern app_data_t app;

    if((app.board.canbus.hcan == NULL) || (hcan != app.board.canbus.hcan))
    {
        return;
    }

    app.can_error_code = HAL_CAN_GetError(hcan);
    app.canbus_hw_fault = true;
    if((app.can_error_code & HAL_CAN_ERROR_RX_FOV0) != 0u)
    {
        app.can_rx_overrun_count++;
        app.canbus_rx_fault = true;
    }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
	extern app_data_t app;
	flow_sensor_t *cool_flow = &app.board.cool_flow;

	if((htim != NULL) && (cool_flow->htim != NULL) &&
	   (htim->Instance == cool_flow->htim->Instance)){
		flow_sensor_read(cool_flow);
	}
}
/* USER CODE END 1 */

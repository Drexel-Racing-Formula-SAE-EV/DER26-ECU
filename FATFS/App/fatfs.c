/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
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
#include "fatfs.h"
#include "app.h"

uint8_t retUSER;    /* Return value for USER */
char USERPath[4];   /* USER logical drive path */
FATFS USERFatFS;    /* File system object for USER logical drive */
FIL USERFile;       /* File object for USER */

/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

void MX_FATFS_Init(void)
{
  /*## FatFS: Link the USER driver ###########################*/
  retUSER = FATFS_LinkDriver(&USER_Driver, USERPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void)
{
  /* USER CODE BEGIN get_fattime */
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};
  RTC_HandleTypeDef *rtc = app.board.stm32f767.hrtc;
  if((rtc == NULL) ||
     (HAL_RTC_GetTime(rtc, &time, RTC_FORMAT_BIN) != HAL_OK) ||
     (HAL_RTC_GetDate(rtc, &date, RTC_FORMAT_BIN) != HAL_OK))
  {
    /* FAT's earliest representable date. */
    return (DWORD)((1u << 21u) | (1u << 16u));
  }

  uint32_t year = 2000u + (uint32_t)date.Year;
  if(year < 1980u) year = 1980u;
  if(year > 2107u) year = 2107u;
  return (DWORD)(((year - 1980u) << 25u) |
                 ((uint32_t)date.Month << 21u) |
                 ((uint32_t)date.Date << 16u) |
                 ((uint32_t)time.Hours << 11u) |
                 ((uint32_t)time.Minutes << 5u) |
                 ((uint32_t)time.Seconds / 2u));
  /* USER CODE END get_fattime */
}

/* USER CODE BEGIN Application */

/* USER CODE END Application */

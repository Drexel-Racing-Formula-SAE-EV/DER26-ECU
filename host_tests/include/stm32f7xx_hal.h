#ifndef HOST_STM32F7XX_HAL_H
#define HOST_STM32F7XX_HAL_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    HAL_OK = 0x00u,
    HAL_ERROR = 0x01u,
    HAL_BUSY = 0x02u,
    HAL_TIMEOUT = 0x03u
} HAL_StatusTypeDef;

typedef struct { uintptr_t tag; } ADC_HandleTypeDef;
typedef struct { uintptr_t tag; } UART_HandleTypeDef;
typedef struct { uintptr_t tag; } I2C_HandleTypeDef;
typedef struct { uintptr_t tag; } CAN_HandleTypeDef;
typedef struct { uintptr_t tag; } RTC_HandleTypeDef;
typedef struct { uintptr_t tag; } SPI_HandleTypeDef;

typedef struct {
    volatile uint32_t CCR1;
    volatile uint32_t CCR2;
    volatile uint32_t CCR3;
    volatile uint32_t CCR4;
} TIM_TypeDef;

typedef struct {
    TIM_TypeDef *Instance;
    uintptr_t tag;
} TIM_HandleTypeDef;

typedef uint32_t HAL_TIM_ActiveChannel;

#define TIM_CHANNEL_1 0x00000000u
#define TIM_CHANNEL_2 0x00000004u
#define TIM_CHANNEL_3 0x00000008u
#define TIM_CHANNEL_4 0x0000000Cu

#define CAN_ID_STD 0u
#define CAN_RTR_DATA 0u
#define CAN_TX_MAILBOX0 1u
#define CAN_TX_MAILBOX1 2u
#define CAN_TX_MAILBOX2 4u
#define CAN_FILTERMODE_IDMASK 0u
#define CAN_FILTERMODE_IDLIST 1u
#define CAN_FILTERSCALE_16BIT 0u
#define CAN_FILTERSCALE_32BIT 1u
#define CAN_RX_FIFO0 0u

/* Match STM32F7 HAL CAN error bit assignments used by production canbus.c.
 * Keeping the host stub numerically aligned catches accidental mask/ownership
 * regressions instead of compiling against invented test-only values. */
#define HAL_CAN_ERROR_NONE            0x00000000u
#define HAL_CAN_ERROR_EWG             0x00000001u
#define HAL_CAN_ERROR_EPV             0x00000002u
#define HAL_CAN_ERROR_BOF             0x00000004u
#define HAL_CAN_ERROR_STF             0x00000008u
#define HAL_CAN_ERROR_FOR             0x00000010u
#define HAL_CAN_ERROR_ACK             0x00000020u
#define HAL_CAN_ERROR_BR              0x00000040u
#define HAL_CAN_ERROR_BD              0x00000080u
#define HAL_CAN_ERROR_CRC             0x00000100u
#define HAL_CAN_ERROR_RX_FOV0         0x00000200u
#define HAL_CAN_ERROR_RX_FOV1         0x00000400u
#define HAL_CAN_ERROR_TX_ALST0        0x00000800u
#define HAL_CAN_ERROR_TX_TERR0        0x00001000u
#define HAL_CAN_ERROR_TX_ALST1        0x00002000u
#define HAL_CAN_ERROR_TX_TERR1        0x00004000u
#define HAL_CAN_ERROR_TX_ALST2        0x00008000u
#define HAL_CAN_ERROR_TX_TERR2        0x00010000u
#define HAL_CAN_ERROR_TIMEOUT         0x00020000u
#define HAL_CAN_ERROR_NOT_INITIALIZED 0x00040000u
#define HAL_CAN_ERROR_NOT_READY       0x00080000u
#define HAL_CAN_ERROR_NOT_STARTED     0x00100000u
#define HAL_CAN_ERROR_PARAM           0x00200000u
#define HAL_CAN_ERROR_INTERNAL        0x00800000u

#define ENABLE 1u
#define DISABLE 0u

typedef struct {
    uint32_t StdId;
    uint32_t ExtId;
    uint32_t IDE;
    uint32_t RTR;
    uint32_t DLC;
    uint32_t TransmitGlobalTime;
} CAN_TxHeaderTypeDef;

typedef struct {
    uint32_t FilterIdHigh;
    uint32_t FilterIdLow;
    uint32_t FilterMaskIdHigh;
    uint32_t FilterMaskIdLow;
    uint32_t FilterFIFOAssignment;
    uint32_t FilterBank;
    uint32_t FilterMode;
    uint32_t FilterScale;
    uint32_t FilterActivation;
    uint32_t SlaveStartFilterBank;
} CAN_FilterTypeDef;

#define I2C_MEMADD_SIZE_8BIT 1u

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *htim);
HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef *htim, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_IC_Start(TIM_HandleTypeDef *htim, uint32_t channel);
uint32_t HAL_TIM_ReadCapturedValue(TIM_HandleTypeDef *htim, uint32_t channel);
uint32_t HAL_GetTick(void);

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *data,
                                    uint16_t len, uint32_t timeout);

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *hi2c,
                                        uint16_t dev_addr, uint32_t trials,
                                        uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c,
                                    uint16_t dev_addr, uint16_t mem_addr,
                                    uint16_t mem_addr_size, uint8_t *data,
                                    uint16_t len, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c,
                                   uint16_t dev_addr, uint16_t mem_addr,
                                   uint16_t mem_addr_size, uint8_t *data,
                                   uint16_t len, uint32_t timeout);

HAL_StatusTypeDef HAL_CAN_ConfigFilter(CAN_HandleTypeDef *hcan,
                                       CAN_FilterTypeDef *filter);
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan);
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(CAN_HandleTypeDef *hcan);
HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan,
                                       CAN_TxHeaderTypeDef *header,
                                       uint8_t data[], uint32_t *mailbox);
HAL_StatusTypeDef HAL_CAN_AbortTxRequest(CAN_HandleTypeDef *hcan,
                                        uint32_t mailbox);

#endif

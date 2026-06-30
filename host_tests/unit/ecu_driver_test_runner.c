#include "ext_drivers/canbus.h"
#include "ext_drivers/cli.h"
#include "ext_drivers/dashboard.h"
#include "ext_drivers/flow_sensor.h"
#include "ext_drivers/map.h"
#include "ext_drivers/mpu6050.h"
#include "ext_drivers/ntc.h"
#include "ext_drivers/poten.h"
#include "ext_drivers/pressure_sensor.h"
#include "ext_drivers/pwm.h"
#include "ext_drivers/rtc.h"
#include "ext_drivers/stm32f767.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

ADC_HandleTypeDef hadc1 = { .dummy = 11 };
ADC_HandleTypeDef hadc2 = { .dummy = 22 };
ADC_HandleTypeDef hadc3 = { .dummy = 33 };
CAN_HandleTypeDef hcan1 = { .dummy = 44 };
I2C_HandleTypeDef hi2c2 = { .dummy = 55 };
RTC_HandleTypeDef hrtc = { .dummy = 66 };
SPI_HandleTypeDef hspi6 = { .dummy = 77 };
TIM_HandleTypeDef htim3 = { .dummy = 88 };
TIM_HandleTypeDef htim4 = { .dummy = 99 };
TIM_HandleTypeDef htim5 = { .dummy = 111 };
UART_HandleTypeDef huart7 = { .dummy = 122 };
UART_HandleTypeDef huart3 = { .dummy = 133 };


#define EXPECT_TRUE(expr) do { if(!(expr)) { printf("FAIL %s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); failures++; } } while(0)
#define EXPECT_FALSE(expr) EXPECT_TRUE(!(expr))
#define EXPECT_EQ_INT(actual, expected) do { int a_=(int)(actual); int e_=(int)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%d expected=%d\n", __FILE__, __LINE__, #actual, a_, e_); failures++; } } while(0)
#define EXPECT_EQ_U16(actual, expected) do { uint16_t a_=(uint16_t)(actual); uint16_t e_=(uint16_t)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%u expected=%u\n", __FILE__, __LINE__, #actual, (unsigned)a_, (unsigned)e_); failures++; } } while(0)
#define EXPECT_EQ_U32(actual, expected) do { uint32_t a_=(uint32_t)(actual); uint32_t e_=(uint32_t)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%lu expected=%lu\n", __FILE__, __LINE__, #actual, (unsigned long)a_, (unsigned long)e_); failures++; } } while(0)
#define EXPECT_EQ_STATUS(actual, expected) do { HAL_StatusTypeDef a_=(HAL_StatusTypeDef)(actual); HAL_StatusTypeDef e_=(HAL_StatusTypeDef)(expected); if(a_ != e_) { printf("FAIL %s:%d: %s=%d expected=%d\n", __FILE__, __LINE__, #actual, (int)a_, (int)e_); failures++; } } while(0)
#define EXPECT_NEAR_FLOAT(actual, expected, tol) do { float a_=(float)(actual); float e_=(float)(expected); float t_=(float)(tol); if(fabsf(a_ - e_) > t_) { printf("FAIL %s:%d: %s=%f expected=%f tol=%f\n", __FILE__, __LINE__, #actual, (double)a_, (double)e_, (double)t_); failures++; } } while(0)

static HAL_StatusTypeDef mock_i2c_ready_status;
static HAL_StatusTypeDef mock_i2c_write_status[8];
static uint32_t mock_i2c_write_count;
static uint16_t mock_i2c_last_dev_addr;
static uint16_t mock_i2c_written_regs[8];
static uint8_t mock_i2c_written_values[8];
static HAL_StatusTypeDef mock_i2c_read_status;
static uint8_t mock_i2c_read_data[14];
static uint32_t mock_i2c_read_count;

static HAL_StatusTypeDef mock_tim_pwm_start_status;
static HAL_StatusTypeDef mock_tim_base_start_status;
static HAL_StatusTypeDef mock_tim_ic_start_it_status;
static HAL_StatusTypeDef mock_tim_ic_start_status;
static uint32_t mock_tim_pwm_start_count;
static uint32_t mock_tim_pwm_last_channel;
static uint32_t mock_tim_capture_total;
static uint32_t mock_tim_capture_high;
static uint32_t mock_tim_base_start_count;
static uint32_t mock_tim_ic_start_it_count;
static uint32_t mock_tim_ic_start_count;

static HAL_StatusTypeDef mock_can_start_status;
static uint32_t mock_can_start_count;
static uint32_t mock_can_free_level;
static uint32_t mock_tick_value;
static uint32_t mock_tick_increment;
static uint32_t mock_yield_count;
static HAL_StatusTypeDef mock_can_add_status;
static uint32_t mock_can_add_count;
static uint32_t mock_can_last_mailbox;
static CAN_TxHeaderTypeDef mock_can_last_header;
static uint8_t mock_can_last_data[DATALEN];

static int mock_inside_isr;
static HAL_StatusTypeDef mock_uart_tx_status[4];
static HAL_StatusTypeDef mock_uart_tx_it_status[4];
static uint32_t mock_uart_tx_count;
static uint32_t mock_uart_tx_it_count;
static char mock_uart_last_blocking[CLI_LINESZ];
static char mock_uart_last_it[CLI_LINESZ];


static HAL_StatusTypeDef mock_adc_start_status;
static HAL_StatusTypeDef mock_adc_poll_status;
static HAL_StatusTypeDef mock_adc_stop_status;
static HAL_StatusTypeDef mock_adc_config_status;
static uint32_t mock_adc_start_count;
static uint32_t mock_adc_poll_count;
static uint32_t mock_adc_get_count;
static uint32_t mock_adc_stop_count;
static uint32_t mock_adc_config_count;
static uint32_t mock_adc_value;
static uint32_t mock_adc_last_timeout;
static ADC_ChannelConfTypeDef mock_adc_last_config;

static uint32_t mock_mutex_new_count;
static const osMutexAttr_t *mock_mutex_attrs[8];

static void reset_mocks(void)
{
    memset(mock_i2c_write_status, 0, sizeof(mock_i2c_write_status));
    memset(mock_i2c_written_regs, 0, sizeof(mock_i2c_written_regs));
    memset(mock_i2c_written_values, 0, sizeof(mock_i2c_written_values));
    memset(mock_i2c_read_data, 0, sizeof(mock_i2c_read_data));
    memset(mock_can_last_data, 0, sizeof(mock_can_last_data));
    memset(&mock_can_last_header, 0, sizeof(mock_can_last_header));
    memset(mock_uart_tx_status, 0, sizeof(mock_uart_tx_status));
    memset(mock_uart_tx_it_status, 0, sizeof(mock_uart_tx_it_status));
    memset(mock_uart_last_blocking, 0, sizeof(mock_uart_last_blocking));
    memset(mock_uart_last_it, 0, sizeof(mock_uart_last_it));

    mock_i2c_ready_status = HAL_OK;
    mock_i2c_write_count = 0u;
    mock_i2c_last_dev_addr = 0u;
    mock_i2c_read_status = HAL_OK;
    mock_i2c_read_count = 0u;

    mock_tim_pwm_start_status = HAL_OK;
    mock_tim_base_start_status = HAL_OK;
    mock_tim_ic_start_it_status = HAL_OK;
    mock_tim_ic_start_status = HAL_OK;
    mock_tim_pwm_start_count = 0u;
    mock_tim_pwm_last_channel = 0u;
    mock_tim_capture_total = 0u;
    mock_tim_capture_high = 0u;
    mock_tim_base_start_count = 0u;
    mock_tim_ic_start_it_count = 0u;
    mock_tim_ic_start_count = 0u;

    mock_can_start_status = HAL_OK;
    mock_can_start_count = 0u;
    mock_can_free_level = 1u;
    mock_tick_value = 0u;
    mock_tick_increment = 1u;
    mock_yield_count = 0u;
    mock_can_add_status = HAL_OK;
    mock_can_add_count = 0u;
    mock_can_last_mailbox = 0u;

    mock_inside_isr = 0;
    mock_uart_tx_count = 0u;
    mock_uart_tx_it_count = 0u;

    mock_adc_start_status = HAL_OK;
    mock_adc_poll_status = HAL_OK;
    mock_adc_stop_status = HAL_OK;
    mock_adc_config_status = HAL_OK;
    mock_adc_start_count = 0u;
    mock_adc_poll_count = 0u;
    mock_adc_get_count = 0u;
    mock_adc_stop_count = 0u;
    mock_adc_config_count = 0u;
    mock_adc_value = 0u;
    mock_adc_last_timeout = 0u;
    memset(&mock_adc_last_config, 0, sizeof(mock_adc_last_config));

    mock_mutex_new_count = 0u;
    memset(mock_mutex_attrs, 0, sizeof(mock_mutex_attrs));
}

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *hi2c, uint16_t dev_addr, uint32_t trials, uint32_t timeout)
{
    (void)hi2c;
    (void)trials;
    (void)timeout;
    mock_i2c_last_dev_addr = dev_addr;
    return mock_i2c_ready_status;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t dev_addr, uint16_t mem_addr, uint16_t mem_size, uint8_t *data, uint16_t size, uint32_t timeout)
{
    HAL_StatusTypeDef ret = HAL_OK;
    (void)hi2c;
    (void)mem_size;
    (void)size;
    (void)timeout;
    mock_i2c_last_dev_addr = dev_addr;
    if(mock_i2c_write_count < 8u)
    {
        mock_i2c_written_regs[mock_i2c_write_count] = mem_addr;
        mock_i2c_written_values[mock_i2c_write_count] = *data;
        ret = mock_i2c_write_status[mock_i2c_write_count];
    }
    mock_i2c_write_count++;
    return ret;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t dev_addr, uint16_t mem_addr, uint16_t mem_size, uint8_t *data, uint16_t size, uint32_t timeout)
{
    (void)hi2c;
    (void)mem_addr;
    (void)mem_size;
    (void)timeout;
    mock_i2c_last_dev_addr = dev_addr;
    mock_i2c_read_count++;
    if((mock_i2c_read_status == HAL_OK) && (data != NULL))
    {
        memcpy(data, mock_i2c_read_data, size);
    }
    return mock_i2c_read_status;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    mock_tim_pwm_start_count++;
    mock_tim_pwm_last_channel = channel;
    return mock_tim_pwm_start_status;
}

HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *htim)
{
    (void)htim;
    mock_tim_base_start_count++;
    return mock_tim_base_start_status;
}

HAL_StatusTypeDef HAL_TIM_IC_Start_IT(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    (void)channel;
    mock_tim_ic_start_it_count++;
    return mock_tim_ic_start_it_status;
}

HAL_StatusTypeDef HAL_TIM_IC_Start(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    (void)channel;
    mock_tim_ic_start_count++;
    return mock_tim_ic_start_status;
}

uint32_t HAL_TIM_ReadCapturedValue(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    if(channel == HAL_TIM_ACTIVE_CHANNEL_1)
    {
        return mock_tim_capture_total;
    }
    if(channel == HAL_TIM_ACTIVE_CHANNEL_2)
    {
        return mock_tim_capture_high;
    }
    return 0u;
}

HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan)
{
    (void)hcan;
    mock_can_start_count++;
    return mock_can_start_status;
}

uint32_t HAL_CAN_GetTxMailboxesFreeLevel(CAN_HandleTypeDef *hcan)
{
    (void)hcan;
    return mock_can_free_level;
}

HAL_StatusTypeDef HAL_CAN_AddTxMessage(CAN_HandleTypeDef *hcan, CAN_TxHeaderTypeDef *header, uint8_t *data, uint32_t *mailbox)
{
    (void)hcan;
    mock_can_add_count++;
    mock_can_last_header = *header;
    memcpy(mock_can_last_data, data, DATALEN);
    if(mailbox != NULL)
    {
        *mailbox = 3u;
        mock_can_last_mailbox = *mailbox;
    }
    return mock_can_add_status;
}

uint32_t HAL_GetTick(void)
{
    uint32_t ret = mock_tick_value;
    mock_tick_value += mock_tick_increment;
    return ret;
}

void taskYIELD(void)
{
    mock_yield_count++;
}

int xPortIsInsideInterrupt(void)
{
    return mock_inside_isr;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size, uint32_t timeout)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint32_t idx = mock_uart_tx_count;
    (void)huart;
    (void)timeout;
    if(size < sizeof(mock_uart_last_blocking))
    {
        memcpy(mock_uart_last_blocking, data, size);
        mock_uart_last_blocking[size] = '\0';
    }
    if(idx < 4u)
    {
        ret = mock_uart_tx_status[idx];
    }
    mock_uart_tx_count++;
    return ret;
}

HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size)
{
    HAL_StatusTypeDef ret = HAL_OK;
    uint32_t idx = mock_uart_tx_it_count;
    (void)huart;
    if(size < sizeof(mock_uart_last_it))
    {
        memcpy(mock_uart_last_it, data, size);
        mock_uart_last_it[size] = '\0';
    }
    if(idx < 4u)
    {
        ret = mock_uart_tx_it_status[idx];
    }
    mock_uart_tx_it_count++;
    return ret;
}

HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    mock_adc_start_count++;
    return mock_adc_start_status;
}

HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *hadc, uint32_t timeout)
{
    (void)hadc;
    mock_adc_poll_count++;
    mock_adc_last_timeout = timeout;
    return mock_adc_poll_status;
}

uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    mock_adc_get_count++;
    return mock_adc_value;
}

HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *hadc)
{
    (void)hadc;
    mock_adc_stop_count++;
    return mock_adc_stop_status;
}

HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *hadc, ADC_ChannelConfTypeDef *config)
{
    (void)hadc;
    mock_adc_config_count++;
    if(config != NULL)
    {
        mock_adc_last_config = *config;
    }
    return mock_adc_config_status;
}

osMutexId_t osMutexNew(const osMutexAttr_t *attr)
{
    static uintptr_t fake_mutexes[8];

    if(mock_mutex_new_count < 8u)
    {
        mock_mutex_attrs[mock_mutex_new_count] = attr;
        fake_mutexes[mock_mutex_new_count] = (uintptr_t)(0x1000u + mock_mutex_new_count);
        mock_mutex_new_count++;
        return (osMutexId_t)&fake_mutexes[mock_mutex_new_count - 1u];
    }

    mock_mutex_new_count++;
    return NULL;
}

static void put_i16_be(uint8_t *dst, int16_t value)
{
    uint16_t u = (uint16_t)value;
    dst[0] = (uint8_t)(u >> 8u);
    dst[1] = (uint8_t)(u & 0xFFu);
}

static void test_map_edge_cases(void)
{
    EXPECT_NEAR_FLOAT((float)map(50L, 0L, 100L, 0L, 1000L), 500.5f, 0.01f);
    EXPECT_NEAR_FLOAT((float)map(25L, 0L, 100L, 1000L, 0L), 749.5f, 0.01f);
    EXPECT_NEAR_FLOAT((float)map(10L, 5L, 5L, 20L, 40L), 30.0f, 0.01f);
    EXPECT_NEAR_FLOAT((float)map(0L, -100L, 100L, -50L, 50L), 0.5f, 0.01f);
}

static void test_poten_filter_clamp_and_fault_helpers(void)
{
    poten_t pot;
    ADC_HandleTypeDef adc;
    poten_init(&pot, 100u, 1100u, &adc);
    EXPECT_EQ_U16(pot.min, 100u);
    EXPECT_EQ_U16(pot.max, 1100u);
    EXPECT_TRUE(pot.handle == &adc);

    pot.count = 1100u;
    for(uint16_t i = 0u; i < HISTSZ; i++)
    {
        (void)poten_get_percent(&pot);
    }
    EXPECT_NEAR_FLOAT(poten_get_percent(&pot), 100.0f, 0.01f);

    pot.count = 2000u;
    EXPECT_NEAR_FLOAT(poten_get_percent(&pot), 100.0f, 0.01f);
    pot.count = 0u;
    for(uint16_t i = 0u; i < HISTSZ; i++)
    {
        (void)poten_get_percent(&pot);
    }
    EXPECT_NEAR_FLOAT(poten_get_percent(&pot), 0.0f, 0.01f);

    EXPECT_EQ_INT(poten_check_failure(100.0f, 200, 100), 1);
    EXPECT_EQ_INT(poten_check_failure(200.0f, 200, 100), 1);
    EXPECT_EQ_INT(poten_check_failure(99.0f, 200, 100), 0);
    EXPECT_EQ_INT(poten_check_failure(201.0f, 200, 100), 0);

    (void)poten_check_plausibility(10.0f, 10.0f, 5, 2);
    EXPECT_EQ_INT(poten_check_plausibility(20.0f, 0.0f, 5, 2), 1);
    EXPECT_EQ_INT(poten_check_plausibility(20.0f, 0.0f, 5, 2), 1);
    EXPECT_EQ_INT(poten_check_plausibility(20.0f, 0.0f, 5, 2), 0);
    EXPECT_EQ_INT(poten_check_plausibility(10.0f, 10.0f, 5, 2), 1);
    EXPECT_EQ_INT(poten_check_plausibility(20.0f, 0.0f, 5, 2), 1);
}

static void test_pressure_sensor_percent_and_plausibility(void)
{
    pressure_sensor_t sensor;
    ADC_HandleTypeDef adc;
    pressure_sensor_init(&sensor, 100u, 1100u, &adc, 7u);
    EXPECT_EQ_U16(sensor.min, 100u);
    EXPECT_EQ_U16(sensor.max, 1100u);
    EXPECT_TRUE(sensor.handle == &adc);
    EXPECT_EQ_U32(sensor.channel, 7u);

    sensor.count = 600u;
    EXPECT_NEAR_FLOAT(pressure_sensor_get_percent(&sensor), 50.5f, 0.01f);
    sensor.count = 2000u;
    EXPECT_NEAR_FLOAT(pressure_sensor_get_percent(&sensor), 100.0f, 0.01f);
    sensor.count = 0u;
    EXPECT_NEAR_FLOAT(pressure_sensor_get_percent(&sensor), 0.0f, 0.01f);

    EXPECT_EQ_INT(pressure_sensor_check_failure(100.0f, 200, 100), 1);
    EXPECT_EQ_INT(pressure_sensor_check_failure(200.0f, 200, 100), 1);
    EXPECT_EQ_INT(pressure_sensor_check_failure(99.0f, 200, 100), 0);
    EXPECT_EQ_INT(pressure_sensor_check_failure(201.0f, 200, 100), 0);

    (void)pressure_sensor_check_implausibility(10.0f, 10.0f, 5, 1);
    EXPECT_EQ_INT(pressure_sensor_check_implausibility(20.0f, 0.0f, 5, 1), 1);
    EXPECT_EQ_INT(pressure_sensor_check_implausibility(20.0f, 0.0f, 5, 1), 0);
    EXPECT_EQ_INT(pressure_sensor_check_implausibility(10.0f, 10.0f, 5, 1), 1);
}

static void test_pwm_init_and_clamp_behavior(void)
{
    pwm_t pwm;
    TIM_HandleTypeDef htim;
    TIM_TypeDef tim;
    uint32_t ccr = 1234u;
    reset_mocks();

    EXPECT_EQ_INT(pwm_device_init(NULL, &tim, &htim, 1000u, &ccr, 1), -1);
    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, NULL, 1000u, &ccr, 1), -1);
    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, &htim, 0u, &ccr, 1), -1);
    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, &htim, 1000u, NULL, 1), -1);
    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, &htim, 1000u, &ccr, 0), -1);
    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, &htim, 1000u, &ccr, 5), -1);

    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, &htim, 1000u, &ccr, 2), 0);
    EXPECT_EQ_U32(mock_tim_pwm_start_count, 1u);
    EXPECT_EQ_U32(mock_tim_pwm_last_channel, 4u);
    EXPECT_EQ_U32(ccr, 0u);

    EXPECT_EQ_INT(pwm_set_percent(&pwm, 25.0f), 0);
    EXPECT_EQ_U32(ccr, 250u);
    EXPECT_NEAR_FLOAT(pwm.duty_cycle, 25.0f, 0.01f);

    EXPECT_EQ_INT(pwm_set_percent(&pwm, 150.0f), 0);
    EXPECT_EQ_U32(ccr, 1000u);
    EXPECT_NEAR_FLOAT(pwm.duty_cycle, 100.0f, 0.01f);

    EXPECT_EQ_INT(pwm_set_percent(&pwm, -20.0f), 0);
    EXPECT_EQ_U32(ccr, 0u);
    EXPECT_NEAR_FLOAT(pwm.duty_cycle, 0.0f, 0.01f);
    EXPECT_EQ_INT(pwm_set_percent(NULL, 50.0f), -1);
}

static void test_flow_sensor_read_zero_and_nonzero_capture(void)
{
    flow_sensor_t flow;
    TIM_HandleTypeDef htim;
    TIM_TypeDef tim;
    reset_mocks();

    EXPECT_EQ_INT(flow_sensor_read(NULL), -1);
    flow_sensor_init(&flow, 1000000u, &htim, &tim, HAL_TIM_ACTIVE_CHANNEL_2, HAL_TIM_ACTIVE_CHANNEL_1);
    EXPECT_EQ_INT(flow.ret, 0);
    EXPECT_EQ_U32(mock_tim_base_start_count, 1u);
    EXPECT_EQ_U32(mock_tim_ic_start_it_count, 1u);
    EXPECT_EQ_U32(mock_tim_ic_start_count, 1u);

    mock_tim_capture_total = 0u;
    mock_tim_capture_high = 123u;
    EXPECT_EQ_INT(flow_sensor_read(&flow), 0);
    EXPECT_EQ_U32(flow.high_count, 0u);
    EXPECT_NEAR_FLOAT(flow.duty, 0.0f, 0.01f);
    EXPECT_NEAR_FLOAT(flow.freq, 0.0f, 0.01f);

    mock_tim_capture_total = 1000u;
    mock_tim_capture_high = 250u;
    EXPECT_EQ_INT(flow_sensor_read(&flow), 0);
    EXPECT_EQ_U32(flow.total_count, 1000u);
    EXPECT_EQ_U32(flow.high_count, 250u);
    EXPECT_NEAR_FLOAT(flow.duty, 25.0f, 0.01f);
    EXPECT_NEAR_FLOAT(flow.freq, 1000.0f, 0.01f);

    mock_tim_capture_total = 1000u;
    mock_tim_capture_high = 1250u;
    EXPECT_EQ_INT(flow_sensor_read(&flow), 0);
    EXPECT_EQ_U32(flow.high_count, 1000u);
    EXPECT_NEAR_FLOAT(flow.duty, 100.0f, 0.01f);

    reset_mocks();
    mock_tim_ic_start_status = HAL_ERROR;
    flow_sensor_init(&flow, 1000000u, &htim, &tim, HAL_TIM_ACTIVE_CHANNEL_2, HAL_TIM_ACTIVE_CHANNEL_1);
    EXPECT_EQ_INT(flow.ret, -1);

    flow.clock_freq = 0u;
    EXPECT_EQ_INT(flow_sensor_read(&flow), -1);
}

static void test_mpu6050_init_validation_and_status_propagation(void)
{
    mpu6050_t dev;
    mpu6050_config_t conf = {0};
    I2C_HandleTypeDef i2c;
    reset_mocks();

    conf.addr_7bit = MPU6050_ADDR1;
    conf.sample_rate_divisor = 4u;
    conf.external_sync = EXT_SYNC_ACC_XOUT_L0;
    conf.lowpass_filter = DLPF_44HZ_BW;
    conf.gyro_scale = FS_SEL_500;
    conf.acc_scale = AFS_SEL_8;
    conf.clock = CLKSEL_XGYRO;

    EXPECT_EQ_STATUS(mpu6050_init(NULL, &conf, &i2c), HAL_ERROR);
    EXPECT_EQ_STATUS(mpu6050_init(&dev, NULL, &i2c), HAL_ERROR);
    EXPECT_EQ_STATUS(mpu6050_init(&dev, &conf, NULL), HAL_ERROR);

    conf.gyro_scale = (FS_SEL)4;
    EXPECT_EQ_STATUS(mpu6050_init(&dev, &conf, &i2c), HAL_ERROR);
    conf.gyro_scale = FS_SEL_500;
    conf.acc_scale = (AFS_SEL)4;
    EXPECT_EQ_STATUS(mpu6050_init(&dev, &conf, &i2c), HAL_ERROR);
    conf.acc_scale = AFS_SEL_8;
    conf.external_sync = (EXT_SYNC_SET)8;
    EXPECT_EQ_STATUS(mpu6050_init(&dev, &conf, &i2c), HAL_ERROR);
    conf.external_sync = EXT_SYNC_ACC_XOUT_L0;
    conf.lowpass_filter = (DLPF_CFG)7;
    EXPECT_EQ_STATUS(mpu6050_init(&dev, &conf, &i2c), HAL_ERROR);
    conf.lowpass_filter = DLPF_44HZ_BW;
    conf.clock = (CLKSEL)7;
    EXPECT_EQ_STATUS(mpu6050_init(&dev, &conf, &i2c), HAL_ERROR);
    conf.clock = CLKSEL_XGYRO;

    EXPECT_EQ_STATUS(mpu6050_init(&dev, &conf, &i2c), HAL_OK);
    EXPECT_TRUE(dev.hi2c == &i2c);
    EXPECT_EQ_U16(mock_i2c_last_dev_addr, (uint16_t)(MPU6050_ADDR1 << 1u));
    EXPECT_EQ_U32(mock_i2c_write_count, 5u);
    EXPECT_EQ_U16(mock_i2c_written_regs[0], REG_SMPLRT_DIV);
    EXPECT_EQ_U16(mock_i2c_written_regs[1], REG_CONFIG);
    EXPECT_EQ_U16(mock_i2c_written_regs[2], REG_CONFIG_GYRO);
    EXPECT_EQ_U16(mock_i2c_written_regs[3], REG_CONFIG_ACC);
    EXPECT_EQ_U16(mock_i2c_written_regs[4], REG_PWR_MGMT_1);
    EXPECT_NEAR_FLOAT(dev.gyro_div, 65.5f, 0.01f);
    EXPECT_NEAR_FLOAT(dev.acc_div, 4096.0f, 0.01f);

    reset_mocks();
    mock_i2c_write_status[2] = HAL_TIMEOUT;
    EXPECT_EQ_STATUS(mpu6050_init(&dev, &conf, &i2c), HAL_TIMEOUT);
    EXPECT_EQ_STATUS(dev.error, HAL_TIMEOUT);
}

static void test_mpu6050_read_signed_scaling_and_failure_no_overwrite(void)
{
    mpu6050_t dev;
    mpu6050_config_t conf = {0};
    I2C_HandleTypeDef i2c;
    reset_mocks();

    conf.addr_7bit = MPU6050_ADDR0;
    conf.gyro_scale = FS_SEL_250;
    conf.acc_scale = AFS_SEL_8;
    EXPECT_EQ_STATUS(mpu6050_init(&dev, &conf, &i2c), HAL_OK);

    put_i16_be(&mock_i2c_read_data[0], 16384);  /* +4.0 g with 4096 LSB/g */
    put_i16_be(&mock_i2c_read_data[2], -4096);  /* -1.0 g */
    put_i16_be(&mock_i2c_read_data[4], 0);
    put_i16_be(&mock_i2c_read_data[6], 340);    /* 37.53 C */
    put_i16_be(&mock_i2c_read_data[8], 131);    /* +1 deg/s */
    put_i16_be(&mock_i2c_read_data[10], -262);  /* -2 deg/s */
    put_i16_be(&mock_i2c_read_data[12], 0);

    EXPECT_EQ_STATUS(mpu6050_read(&dev), HAL_OK);
    EXPECT_NEAR_FLOAT(dev.x_acc, 4.0f, 0.01f);
    EXPECT_NEAR_FLOAT(dev.y_acc, -1.0f, 0.01f);
    EXPECT_NEAR_FLOAT(dev.z_acc, 0.0f, 0.01f);
    EXPECT_NEAR_FLOAT(dev.x_gyro, 1.0f, 0.01f);
    EXPECT_NEAR_FLOAT(dev.y_gyro, -2.0f, 0.01f);
    EXPECT_NEAR_FLOAT(dev.z_gyro, 0.0f, 0.01f);
    EXPECT_NEAR_FLOAT(dev.temp, 37.53f, 0.01f);

    dev.x_acc = 12.5f;
    mock_i2c_read_status = HAL_ERROR;
    EXPECT_EQ_STATUS(mpu6050_read(&dev), HAL_ERROR);
    EXPECT_EQ_STATUS(dev.error, HAL_ERROR);
    EXPECT_NEAR_FLOAT(dev.x_acc, 12.5f, 0.01f);

    dev.acc_div = 0.0f;
    EXPECT_EQ_STATUS(mpu6050_read(&dev), HAL_ERROR);
    EXPECT_EQ_STATUS(dev.error, HAL_ERROR);

    dev.acc_div = 4096.0f;
    dev.gyro_div = 0.0f;
    EXPECT_EQ_STATUS(mpu6050_read(&dev), HAL_ERROR);
    EXPECT_EQ_STATUS(dev.error, HAL_ERROR);

    dev.gyro_div = 131.0f;
    dev.hi2c = NULL;
    EXPECT_EQ_STATUS(mpu6050_read(&dev), HAL_ERROR);
    EXPECT_EQ_STATUS(mpu6050_read(NULL), HAL_ERROR);
}

static void test_canbus_init_transmit_and_timeout(void)
{
    canbus_t canbus;
    CAN_HandleTypeDef hcan;
    CAN_TxHeaderTypeDef header;
    canbus_packet_t packet = {0};
    reset_mocks();

    canbus_device_init(NULL, &hcan, &header);
    EXPECT_EQ_U32(mock_can_start_count, 0u);
    canbus_device_init(&canbus, NULL, &header);
    EXPECT_EQ_U32(mock_can_start_count, 0u);
    canbus_device_init(&canbus, &hcan, NULL);
    EXPECT_EQ_U32(mock_can_start_count, 0u);

    canbus_device_init(&canbus, &hcan, &header);
    EXPECT_EQ_U32(mock_can_start_count, 1u);
    EXPECT_EQ_U32(header.IDE, CAN_ID_STD);
    EXPECT_EQ_U32(header.RTR, CAN_RTR_DATA);
    EXPECT_EQ_U32(header.DLC, DATALEN);

    packet.id = 0x123u;
    for(uint16_t i = 0u; i < DATALEN; i++)
    {
        packet.data[i] = (uint8_t)(0xA0u + i);
    }
    EXPECT_EQ_STATUS(canbus_transmit(&canbus, &packet, 10u), HAL_OK);
    EXPECT_EQ_U32(mock_can_add_count, 1u);
    EXPECT_EQ_U32(mock_can_last_header.StdId, 0x123u);
    EXPECT_EQ_U32(mock_can_last_header.DLC, DATALEN);
    EXPECT_EQ_U32(mock_can_last_mailbox, 3u);
    EXPECT_EQ_U16(mock_can_last_data[7], 0xA7u);

    EXPECT_EQ_STATUS(canbus_transmit(NULL, &packet, 10u), HAL_ERROR);
    EXPECT_EQ_STATUS(canbus_transmit(&canbus, NULL, 10u), HAL_ERROR);
    packet.id = 0x800u;
    EXPECT_EQ_STATUS(canbus_transmit(&canbus, &packet, 10u), HAL_ERROR);
    EXPECT_EQ_U32(mock_can_add_count, 1u);
    packet.id = 0x123u;

    reset_mocks();
    canbus_device_init(&canbus, &hcan, &header);
    mock_can_free_level = 0u;
    mock_tick_value = 0u;
    mock_tick_increment = 5u;
    EXPECT_EQ_STATUS(canbus_transmit(&canbus, &packet, 10u), HAL_TIMEOUT);
    EXPECT_EQ_U32(mock_can_add_count, 0u);
    EXPECT_TRUE(mock_yield_count > 0u);

    reset_mocks();
    canbus_device_init(&canbus, &hcan, &header);
    mock_can_add_status = HAL_ERROR;
    EXPECT_EQ_STATUS(canbus_transmit(&canbus, &packet, 10u), HAL_ERROR);
}

static void test_cli_tokenize_and_uart_paths(void)
{
    cli_t cli;
    UART_HandleTypeDef uart;
    char line[] = "ssa 50 now";
    char *toks[4] = {0};
    reset_mocks();

    cli_device_init(&cli, &uart);
    EXPECT_TRUE(cli.huart == &uart);
    EXPECT_EQ_U32(cli.index, 0u);
    EXPECT_FALSE(cli.msg_pending);

    EXPECT_EQ_INT(tokenize(line, toks, 4, " "), 3);
    EXPECT_TRUE(strcmp(toks[0], "ssa") == 0);
    EXPECT_TRUE(strcmp(toks[1], "50") == 0);
    EXPECT_TRUE(strcmp(toks[2], "now") == 0);
    EXPECT_TRUE(toks[3] == NULL);

    EXPECT_EQ_INT(cli_printline(&cli, "hello"), HAL_OK);
    EXPECT_EQ_U32(mock_uart_tx_count, 2u);
    EXPECT_TRUE(strcmp(mock_uart_last_blocking, "\r\n") == 0);

    reset_mocks();
    mock_uart_tx_status[1] = HAL_TIMEOUT;
    EXPECT_EQ_INT(cli_printline(&cli, "hello"), HAL_TIMEOUT);

    reset_mocks();
    mock_inside_isr = 1;
    EXPECT_EQ_INT(cli_printline(&cli, "irq"), HAL_OK);
    EXPECT_EQ_U32(mock_uart_tx_it_count, 2u);
    EXPECT_TRUE(strcmp(mock_uart_last_it, "\r\n") == 0);
}

static void test_dashboard_ntc_and_rtc_helpers(void)
{
    dashboard_t dash;
    ntc_t ntc;
    UART_HandleTypeDef uart;
    ADC_HandleTypeDef adc;
    reset_mocks();

    EXPECT_EQ_INT(dashboard_init(&dash, &uart), 0);
    EXPECT_TRUE(dash.huart == &uart);
    EXPECT_EQ_STATUS(dash.ret, HAL_OK);
    EXPECT_EQ_STATUS(dashboard_write(&dash, "dash"), HAL_OK);
    EXPECT_EQ_U32(mock_uart_tx_count, 1u);
    EXPECT_TRUE(strcmp(mock_uart_last_blocking, "dash") == 0);

    ntc_init(&ntc, &adc, 12u);
    EXPECT_TRUE(ntc.hadc == &adc);
    EXPECT_EQ_U32(ntc.channel, 12u);
    EXPECT_EQ_U16(ntc.count, 0u);
    EXPECT_NEAR_FLOAT(ntc.temp, 0.0f, 0.01f);

    EXPECT_EQ_U16(rtc_bcd_to_dec(0x59u), 59u);
    EXPECT_EQ_U16(rtc_bcd_to_dec(0x00u), 0u);
    EXPECT_EQ_U16(rtc_dec_to_bcd(59u), 0x59u);
    EXPECT_EQ_U16(rtc_dec_to_bcd(0u), 0x00u);
}


static void test_stm32f767_board_copy_mutex_and_adc_paths(void)
{
    stm32f767_t board;
    ADC_HandleTypeDef adc;
    reset_mocks();

    memset(&board, 0, sizeof(board));
    stm32f767_init(&board);
    EXPECT_EQ_INT(board.hadc1.dummy, hadc1.dummy);
    EXPECT_EQ_INT(board.hadc2.dummy, hadc2.dummy);
    EXPECT_EQ_INT(board.hadc3.dummy, hadc3.dummy);
    EXPECT_EQ_INT(board.hcan1.dummy, hcan1.dummy);
    EXPECT_EQ_INT(board.hi2c2.dummy, hi2c2.dummy);
    EXPECT_EQ_INT(board.hrtc.dummy, hrtc.dummy);
    EXPECT_EQ_INT(board.hspi6.dummy, hspi6.dummy);
    EXPECT_EQ_INT(board.htim3.dummy, htim3.dummy);
    EXPECT_EQ_INT(board.htim4.dummy, htim4.dummy);
    EXPECT_EQ_INT(board.htim5.dummy, htim5.dummy);
    EXPECT_EQ_INT(board.huart7.dummy, huart7.dummy);
    EXPECT_EQ_INT(board.huart3.dummy, huart3.dummy);
    EXPECT_EQ_U32(mock_mutex_new_count, 5u);
    EXPECT_TRUE(board.can1_mutex != NULL);
    EXPECT_TRUE(board.i2c2_mutex != NULL);
    EXPECT_TRUE(board.spi6_mutex != NULL);
    EXPECT_TRUE(board.uart3_mutex != NULL);
    EXPECT_TRUE(board.uart7_mutex != NULL);
    EXPECT_TRUE(mock_mutex_attrs[0] != NULL);
    EXPECT_TRUE(mock_mutex_attrs[1] != NULL);
    EXPECT_TRUE(mock_mutex_attrs[2] != NULL);
    EXPECT_TRUE(mock_mutex_attrs[3] != NULL);
    EXPECT_TRUE(mock_mutex_attrs[4] != NULL);

    reset_mocks();
    EXPECT_EQ_U16(stm32f767_adc_read(NULL), 0u);
    EXPECT_EQ_U32(mock_adc_start_count, 0u);

    mock_adc_start_status = HAL_ERROR;
    EXPECT_EQ_U16(stm32f767_adc_read(&adc), 0u);
    EXPECT_EQ_U32(mock_adc_start_count, 1u);
    EXPECT_EQ_U32(mock_adc_poll_count, 0u);
    EXPECT_EQ_U32(mock_adc_stop_count, 0u);

    reset_mocks();
    mock_adc_poll_status = HAL_TIMEOUT;
    mock_adc_value = 1234u;
    EXPECT_EQ_U16(stm32f767_adc_read(&adc), 0u);
    EXPECT_EQ_U32(mock_adc_start_count, 1u);
    EXPECT_EQ_U32(mock_adc_poll_count, 1u);
    EXPECT_EQ_U32(mock_adc_last_timeout, 10u);
    EXPECT_EQ_U32(mock_adc_get_count, 0u);
    EXPECT_EQ_U32(mock_adc_stop_count, 1u);

    reset_mocks();
    mock_adc_value = 4095u;
    EXPECT_EQ_U16(stm32f767_adc_read(&adc), 4095u);
    EXPECT_EQ_U32(mock_adc_start_count, 1u);
    EXPECT_EQ_U32(mock_adc_poll_count, 1u);
    EXPECT_EQ_U32(mock_adc_get_count, 1u);
    EXPECT_EQ_U32(mock_adc_stop_count, 1u);

    reset_mocks();
    EXPECT_EQ_STATUS(stm32f767_adc_switch_channel(NULL, 7u), HAL_ERROR);
    EXPECT_EQ_U32(mock_adc_config_count, 0u);
    EXPECT_EQ_STATUS(stm32f767_adc_switch_channel(&adc, 7u), HAL_OK);
    EXPECT_EQ_U32(mock_adc_config_count, 1u);
    EXPECT_EQ_U32(mock_adc_last_config.Channel, 7u);
    EXPECT_EQ_U32(mock_adc_last_config.Rank, ADC_REGULAR_RANK_1);
    EXPECT_EQ_U32(mock_adc_last_config.SamplingTime, ADC_SAMPLETIME_3CYCLES);

    mock_adc_config_status = HAL_BUSY;
    EXPECT_EQ_STATUS(stm32f767_adc_switch_channel(&adc, 8u), HAL_BUSY);
}

static void test_pwm_all_channels_and_start_failure_visibility(void)
{
    pwm_t pwm;
    TIM_HandleTypeDef htim;
    TIM_TypeDef tim;
    uint32_t ccr = 0u;

    reset_mocks();
    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, &htim, 100u, &ccr, 1), 0);
    EXPECT_EQ_U32(mock_tim_pwm_last_channel, 0u);
    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, &htim, 100u, &ccr, 2), 0);
    EXPECT_EQ_U32(mock_tim_pwm_last_channel, 4u);
    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, &htim, 100u, &ccr, 3), 0);
    EXPECT_EQ_U32(mock_tim_pwm_last_channel, 8u);
    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, &htim, 100u, &ccr, 4), 0);
    EXPECT_EQ_U32(mock_tim_pwm_last_channel, 12u);

    mock_tim_pwm_start_status = HAL_ERROR;
    EXPECT_EQ_INT(pwm_device_init(&pwm, &tim, &htim, 100u, &ccr, 1), -1);
    EXPECT_EQ_U32(ccr, 0u);
}

static void test_canbus_transmit_zero_timeout_and_header_stability(void)
{
    canbus_t canbus;
    CAN_HandleTypeDef hcan;
    CAN_TxHeaderTypeDef header;
    canbus_packet_t packet = {0};
    reset_mocks();

    canbus_device_init(&canbus, &hcan, &header);
    packet.id = 0x7FFu;
    packet.data[0] = 0x5Au;

    mock_can_free_level = 0u;
    mock_tick_value = 0u;
    mock_tick_increment = 1u;
    EXPECT_EQ_STATUS(canbus_transmit(&canbus, &packet, 0u), HAL_TIMEOUT);
    EXPECT_EQ_U32(mock_can_add_count, 0u);

    mock_can_free_level = 1u;
    EXPECT_EQ_STATUS(canbus_transmit(&canbus, &packet, 0u), HAL_OK);
    EXPECT_EQ_U32(mock_can_last_header.StdId, 0x7FFu);
    EXPECT_EQ_U32(mock_can_last_header.IDE, CAN_ID_STD);
    EXPECT_EQ_U32(mock_can_last_header.RTR, CAN_RTR_DATA);
    EXPECT_EQ_U32(mock_can_last_header.DLC, DATALEN);
    EXPECT_EQ_U16(mock_can_last_data[0], 0x5Au);
}

static void test_cli_tokenize_limits_and_status_order(void)
{
    cli_t cli;
    UART_HandleTypeDef uart;
    char line[] = "one two three four";
    char *toks[3] = {0};
    reset_mocks();

    EXPECT_EQ_INT(tokenize(line, toks, 3, " "), 3);
    EXPECT_TRUE(strcmp(toks[0], "one") == 0);
    EXPECT_TRUE(strcmp(toks[1], "two") == 0);
    EXPECT_TRUE(strcmp(toks[2], "three") == 0);

    cli_device_init(&cli, &uart);
    mock_uart_tx_status[0] = HAL_ERROR;
    mock_uart_tx_status[1] = HAL_TIMEOUT;
    EXPECT_EQ_INT(cli_printline(&cli, "bad"), HAL_ERROR);

    reset_mocks();
    mock_inside_isr = 1;
    mock_uart_tx_it_status[0] = HAL_BUSY;
    mock_uart_tx_it_status[1] = HAL_TIMEOUT;
    EXPECT_EQ_INT(cli_printline(&cli, "irqbad"), HAL_BUSY);
}

static void test_dashboard_write_error_propagation(void)
{
    dashboard_t dash;
    UART_HandleTypeDef uart;
    reset_mocks();

    EXPECT_EQ_INT(dashboard_init(&dash, &uart), 0);
    mock_uart_tx_status[0] = HAL_TIMEOUT;
    EXPECT_EQ_STATUS(dashboard_write(&dash, "dashboard"), HAL_TIMEOUT);
    EXPECT_EQ_U32(mock_uart_tx_count, 1u);
}

static void run_test(const char *name, void (*fn)(void))
{
    int before = failures;
    reset_mocks();
    fn();
    if(failures == before)
    {
        printf("PASS %s\n", name);
    }
}

int main(void)
{
    run_test("map edge cases", test_map_edge_cases);
    run_test("poten filter clamp and fault helpers", test_poten_filter_clamp_and_fault_helpers);
    run_test("pressure sensor percent and plausibility", test_pressure_sensor_percent_and_plausibility);
    run_test("pwm init and clamp behavior", test_pwm_init_and_clamp_behavior);
    run_test("flow sensor read zero and nonzero capture", test_flow_sensor_read_zero_and_nonzero_capture);
    run_test("mpu6050 init validation and status propagation", test_mpu6050_init_validation_and_status_propagation);
    run_test("mpu6050 read signed scaling and failure no overwrite", test_mpu6050_read_signed_scaling_and_failure_no_overwrite);
    run_test("canbus init transmit and timeout", test_canbus_init_transmit_and_timeout);
    run_test("cli tokenize and UART paths", test_cli_tokenize_and_uart_paths);
    run_test("dashboard ntc and rtc helpers", test_dashboard_ntc_and_rtc_helpers);
    run_test("stm32f767 board copy mutex and ADC paths", test_stm32f767_board_copy_mutex_and_adc_paths);
    run_test("pwm all channels and start failure visibility", test_pwm_all_channels_and_start_failure_visibility);
    run_test("canbus zero timeout and header stability", test_canbus_transmit_zero_timeout_and_header_stability);
    run_test("cli tokenize limits and status order", test_cli_tokenize_limits_and_status_order);
    run_test("dashboard write error propagation", test_dashboard_write_error_propagation);

    if(failures != 0)
    {
        printf("ECU DRIVER TESTS FAILED: %d failure(s)\n", failures);
        return 1;
    }

    printf("ALL ECU DRIVER TESTS PASSED\n");
    return 0;
}

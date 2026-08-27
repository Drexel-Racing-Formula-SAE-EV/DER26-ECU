#include "support/fake_hal.h"
#include "board.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define EXPECT_TRUE(x) do { if(!(x)){printf("FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);failures++;}}while(0)
#define EXPECT_EQ_U32(a,e) do{uint32_t aa=(uint32_t)(a),ee=(uint32_t)(e);if(aa!=ee){printf("FAIL %s:%d: %s=%lu expected=%lu\n",__FILE__,__LINE__,#a,(unsigned long)aa,(unsigned long)ee);failures++;}}while(0)

static ADC_HandleTypeDef adc1, adc2, adc3;
static CAN_HandleTypeDef can1;
static I2C_HandleTypeDef i2c2;
static RTC_HandleTypeDef rtc;
static SPI_HandleTypeDef spi6;
static TIM_HandleTypeDef tim3 = {.Instance=&host_tim3};
static TIM_HandleTypeDef tim4 = {.Instance=&host_tim4};
static TIM_HandleTypeDef tim5 = {.Instance=&host_tim5};
static UART_HandleTypeDef uart3, uart7;

void stm32f767_init(stm32f767_t *dev)
{
    if(dev == NULL) return;
    memset(dev, 0, sizeof(*dev));
    dev->hadc1 = &adc1;
    dev->hadc2 = &adc2;
    dev->hadc3 = &adc3;
    dev->hcan1 = &can1;
    dev->hi2c2 = &i2c2;
    dev->hrtc = &rtc;
    dev->hspi6 = &spi6;
    dev->htim3 = &tim3;
    dev->htim4 = &tim4;
    dev->htim5 = &tim5;
    dev->huart3 = &uart3;
    dev->huart7 = &uart7;
    dev->initialized = true;
}

static void test_board_init_wires_every_device(void)
{
    board_t board;
    host_hal_reset();
    memset(&board, 0xA5, sizeof(board));
    board_init(&board);

    EXPECT_TRUE(board.stm32f767.initialized);
    EXPECT_TRUE(board.apps1.handle == &adc1);
    EXPECT_TRUE(board.apps2.handle == &adc2);
    EXPECT_EQ_U32(board.apps1.count, 0u);
    EXPECT_EQ_U32(board.apps2.count, 0u);
    EXPECT_EQ_U32(board.apps1.min, APPS1_0);
    EXPECT_EQ_U32(board.apps1.max, APPS1_100);
    EXPECT_EQ_U32(board.apps2.min, APPS2_0);
    EXPECT_EQ_U32(board.apps2.max, APPS2_100);

    EXPECT_TRUE(board.bse1.handle == &adc3);
    EXPECT_TRUE(board.bse2.handle == &adc3);
    EXPECT_TRUE(board.cool_pressure.handle == &adc3);
    EXPECT_EQ_U32(board.bse1.channel, BSE1_ADC_CH);
    EXPECT_EQ_U32(board.bse2.channel, BSE2_ADC_CH);
    EXPECT_EQ_U32(board.cool_pressure.channel, COOL_PRESS_ADC_CH);

    EXPECT_TRUE(board.cool_flow.htim == &tim5);
    EXPECT_EQ_U32(board.cool_flow.clock_freq, 108000000u);
    EXPECT_EQ_U32(board.cool_flow.high_channel, TIM_CHANNEL_2);
    EXPECT_EQ_U32(board.cool_flow.total_channel, TIM_CHANNEL_1);
    EXPECT_TRUE(board.cool_flow.stale);

    EXPECT_TRUE(board.cool_temp1.hadc == &adc3);
    EXPECT_TRUE(board.cool_temp2.hadc == &adc3);
    EXPECT_EQ_U32(board.cool_temp1.channel, COOL_TEMP1_CH);
    EXPECT_EQ_U32(board.cool_temp2.channel, COOL_TEMP2_CH);

    EXPECT_TRUE(board.cool_pump.htim == &tim4);
    EXPECT_TRUE(board.cool_pump.CCR == &host_tim4.CCR3);
    EXPECT_EQ_U32(board.cool_pump.channel, 3u);
    EXPECT_EQ_U32(host_tim4.CCR3, 0u);
    EXPECT_TRUE(board.ssa.htim == &tim3);
    EXPECT_TRUE(board.ssa.CCR == &host_tim3.CCR4);
    EXPECT_EQ_U32(board.ssa.channel, 4u);
    EXPECT_EQ_U32(host_tim3.CCR4, 0u);
    EXPECT_EQ_U32(host_hal.tim_pwm_calls, 2u);

    EXPECT_TRUE(board.canbus.hcan == &can1);
    EXPECT_TRUE(board.canbus.started);
    EXPECT_TRUE(board.canbus.filters_configured);
    EXPECT_TRUE(board.cli.huart == &uart3);
    EXPECT_TRUE(board.dashboard.huart == &uart7);
    EXPECT_TRUE(board.mpu6050.hi2c == &i2c2);
    EXPECT_EQ_U32(board.mpu6050.addr_7bit, MPU6050_ADDR1);
    EXPECT_EQ_U32(host_hal.i2c_write_count, 5u);

    EXPECT_EQ_U32(board.ams.rx_count, 0u);
    EXPECT_EQ_U32(board.cm200.rx_count, 0u);
    EXPECT_EQ_U32(board.cm200.bad_rx_count, 0u);
}

static void test_board_init_null_and_failed_can_start(void)
{
    board_init(NULL);
    board_t board;
    host_hal_reset();
    host_hal.can_start_status = HAL_ERROR;
    memset(&board, 0, sizeof(board));
    board_init(&board);
    EXPECT_TRUE(board.canbus.filters_configured);
    EXPECT_TRUE(!board.canbus.started);
}

int main(void)
{
    test_board_init_wires_every_device();
    if(failures == 0) printf("PASS board init wires every device\n");
    int before = failures;
    test_board_init_null_and_failed_can_start();
    if(failures == before) printf("PASS board init failure propagation\n");
    if(failures != 0)
    {
        printf("BOARD INTEGRATION TEST FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("ALL BOARD INTEGRATION TESTS PASSED\n");
    return 0;
}

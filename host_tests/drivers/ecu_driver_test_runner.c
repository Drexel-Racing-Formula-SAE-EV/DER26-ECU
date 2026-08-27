#include "support/fake_hal.h"

#include "ext_drivers/canbus.h"
#include "ext_drivers/cooling_control.h"
#include "ext_drivers/cli.h"
#include "ext_drivers/dashboard.h"
#include "ext_drivers/flow_sensor.h"
#include "ext_drivers/map.h"
#include "ext_drivers/mpu6050.h"
#include "ext_drivers/ntc.h"
#include "ext_drivers/poten.h"
#include "ext_drivers/pressure_sensor.h"
#include "ext_drivers/pwm.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

static canbus_t *g_delay_outcome_bus;
static uint32_t g_delay_outcome_mailbox;
static uint32_t g_delay_hook_calls;
static uint32_t g_delay_hook_fire_at;
static bool g_delay_hook_complete;

static void delayed_can_outcome_hook(void)
{
    g_delay_hook_calls++;
    if((g_delay_outcome_bus != NULL) &&
       (g_delay_hook_calls >= g_delay_hook_fire_at))
    {
        if(g_delay_hook_complete)
        {
            canbus_tx_complete_isr(g_delay_outcome_bus,
                                   g_delay_outcome_mailbox);
        }
        else
        {
            canbus_tx_abort_isr(g_delay_outcome_bus,
                                g_delay_outcome_mailbox);
        }
        host_hal.delay_hook = NULL;
    }
}

#define EXPECT_TRUE(x) do { if(!(x)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); failures++; } } while(0)
#define EXPECT_FALSE(x) EXPECT_TRUE(!(x))
#define EXPECT_EQ_U32(a,e) do { uint32_t aa=(uint32_t)(a), ee=(uint32_t)(e); if(aa!=ee){printf("FAIL %s:%d: %s=%lu expected=%lu\n",__FILE__,__LINE__,#a,(unsigned long)aa,(unsigned long)ee);failures++;}}while(0)
#define EXPECT_EQ_I32(a,e) do { int32_t aa=(int32_t)(a), ee=(int32_t)(e); if(aa!=ee){printf("FAIL %s:%d: %s=%ld expected=%ld\n",__FILE__,__LINE__,#a,(long)aa,(long)ee);failures++;}}while(0)
#define EXPECT_NEAR(a,e,t) do { double aa=(double)(a), ee=(double)(e); if(fabs(aa-ee)>(double)(t)){printf("FAIL %s:%d: %s=%.9f expected=%.9f tol=%.9f\n",__FILE__,__LINE__,#a,aa,ee,(double)(t));failures++;}}while(0)

static void run_test(const char *name, void (*fn)(void))
{
    int before = failures;
    host_hal_reset();
    fn();
    if(before == failures) printf("PASS %s\n", name);
}

static void test_map_contract(void)
{
    EXPECT_NEAR(map(0, 0, 100, 0, 1000), 0.0, 0.001);
    EXPECT_NEAR(map(50, 0, 100, 0, 1000), 500.0, 0.001);
    EXPECT_NEAR(map(100, 0, 100, 0, 1000), 1000.0, 0.001);
    EXPECT_NEAR(map(25, 0, 100, 1000, 0), 750.0, 1.0);
    EXPECT_NEAR(map(5, 10, 10, 20, 40), 30.0, 0.001);
}

static void test_potentiometer_contract(void)
{
    ADC_HandleTypeDef adc = {0};
    poten_t p;
    memset(&p, 0xA5, sizeof(p));
    poten_init(&p, 1000u, 2000u, &adc);
    EXPECT_EQ_U32(p.count, 0u);
    EXPECT_NEAR(p.percent, 0.0, 0.001);
    EXPECT_EQ_U32(p.hist_count, 0u);
    for(size_t i=0; i<HISTSZ; i++) EXPECT_NEAR(p.hist[i], 0.0, 0.001);

    p.count = 1000u;
    EXPECT_NEAR(poten_get_raw_percent(&p), 0.0, 0.01);
    p.count = 1500u;
    EXPECT_NEAR(poten_get_raw_percent(&p), 50.0, 0.01);
    p.count = 2500u;
    EXPECT_NEAR(poten_get_raw_percent(&p), 100.0, 0.01);

    p.count = 1000u;
    EXPECT_NEAR(poten_get_percent(&p), 0.0, 0.01);
    p.count = 2000u;
    EXPECT_NEAR(poten_get_percent(&p), 50.0, 0.01);
    for(int i=0; i<20; i++) (void)poten_get_percent(&p);
    EXPECT_TRUE(p.hist_count == HISTSZ);
    EXPECT_TRUE(poten_get_percent(NULL) == 0.0f);

    EXPECT_EQ_U32(poten_percent_to_hex(-1.0f), 0u);
    EXPECT_EQ_U32(poten_percent_to_hex(0.0f), 0u);
    EXPECT_EQ_U32(poten_percent_to_hex(50.0f), 32768u);
    EXPECT_EQ_U32(poten_percent_to_hex(100.0f), 65535u);
    EXPECT_EQ_U32(poten_percent_to_hex(NAN), 0u);

    (void)poten_check_plausibility(0.0f, 0.0f, 10, 2);
    EXPECT_TRUE(poten_check_plausibility(0.0f, 20.0f, 10, 2));
    EXPECT_TRUE(poten_check_plausibility(0.0f, 20.0f, 10, 2));
    EXPECT_FALSE(poten_check_plausibility(0.0f, 20.0f, 10, 2));
    EXPECT_TRUE(poten_check_plausibility(0.0f, 0.0f, 10, 2));
    EXPECT_TRUE(poten_check_failure(500.0f, 3000, 100));
    EXPECT_FALSE(poten_check_failure(50.0f, 3000, 100));
}

static void test_pressure_sensor_contract(void)
{
    ADC_HandleTypeDef adc = {0};
    pressure_sensor_t p;
    memset(&p, 0xA5, sizeof(p));
    pressure_sensor_init(&p, 100u, 1100u, &adc, 7u);
    EXPECT_EQ_U32(p.count, 0u);
    EXPECT_EQ_U32(p.channel, 7u);
    EXPECT_NEAR(p.percent, 0.0, 0.001);
    p.count = 600u;
    EXPECT_NEAR(pressure_sensor_get_percent(&p), 50.0, 0.01);
    p.count = 0u;
    EXPECT_NEAR(pressure_sensor_get_percent(&p), 0.0, 0.01);
    p.count = 2000u;
    EXPECT_NEAR(pressure_sensor_get_percent(&p), 100.0, 0.01);
    EXPECT_NEAR(pressure_sensor_get_percent(NULL), 0.0, 0.01);

    (void)pressure_sensor_check_implausibility(0.0f, 0.0f, 10, 1);
    EXPECT_TRUE(pressure_sensor_check_implausibility(0.0f, 20.0f, 10, 1));
    EXPECT_FALSE(pressure_sensor_check_implausibility(0.0f, 20.0f, 10, 1));
    EXPECT_TRUE(pressure_sensor_check_implausibility(0.0f, 0.0f, 10, 1));
    EXPECT_TRUE(pressure_sensor_in_range(500.0f, 3000, 100));
    EXPECT_FALSE(pressure_sensor_in_range(4000.0f, 3000, 100));
}

static void test_pwm_contract(void)
{
    TIM_HandleTypeDef htim = {.Instance=&host_tim4};
    pwm_t pwm;
    volatile uint32_t ccr = 0xFFFFFFFFu;

    EXPECT_EQ_I32(pwm_device_init(&pwm, &host_tim4, &htim, 65535u, &ccr, 3), 0);
    EXPECT_EQ_U32(host_hal.tim_pwm_calls, 1u);
    EXPECT_EQ_U32(host_hal.tim_pwm_channel, TIM_CHANNEL_3);
    EXPECT_EQ_U32(ccr, 0u);
    EXPECT_NEAR(pwm.duty_cycle, 0.0, 0.001);

    EXPECT_EQ_I32(pwm_set_percent(&pwm, 50.0f), 0);
    EXPECT_EQ_U32(ccr, 32767u);
    EXPECT_EQ_I32(pwm_set_percent(&pwm, 200.0f), 0);
    EXPECT_EQ_U32(ccr, 65535u);
    EXPECT_EQ_I32(pwm_set_percent(&pwm, -20.0f), 0);
    EXPECT_EQ_U32(ccr, 0u);
    EXPECT_EQ_I32(pwm_set_percent(&pwm, NAN), -1);

    EXPECT_EQ_I32(pwm_device_init(NULL, &host_tim4, &htim, 1u, &ccr, 1), -1);
    EXPECT_EQ_I32(pwm_device_init(&pwm, &host_tim4, &htim, 1u, &ccr, 0), -1);
    EXPECT_EQ_I32(pwm_device_init(&pwm, &host_tim4, &htim, 1u, &ccr, 5), -1);
    EXPECT_EQ_I32(pwm_device_init(&pwm, &host_tim4, &htim, UINT64_MAX, &ccr, 1), -1);
    host_hal.tim_pwm_status = HAL_ERROR;
    EXPECT_EQ_I32(pwm_device_init(&pwm, &host_tim4, &htim, 100u, &ccr, 1), -1);
}

static void test_flow_sensor_contract(void)
{
    TIM_HandleTypeDef htim = {.Instance=&host_tim5};
    flow_sensor_t flow;
    flow_sensor_init(&flow, 1000000u, &htim, &host_tim5,
                     TIM_CHANNEL_2, TIM_CHANNEL_1);
    EXPECT_EQ_U32(host_hal.tim_base_calls, 1u);
    EXPECT_EQ_U32(host_hal.tim_ic_it_calls, 1u);
    EXPECT_EQ_U32(host_hal.tim_ic_calls, 1u);
    EXPECT_TRUE(flow.stale);
    EXPECT_FALSE(flow.valid);
    EXPECT_EQ_I32(flow.ret, 0);

    host_hal.capture[0] = 1000u;
    host_hal.capture[1] = 250u;
    host_hal.tick = 100u;
    EXPECT_EQ_I32(flow_sensor_read(&flow), 0);
    EXPECT_TRUE(flow.valid);
    EXPECT_FALSE(flow.stale);
    EXPECT_NEAR(flow.duty, 25.0, 0.001);
    EXPECT_NEAR(flow.freq, 1000.0, 0.001);
    EXPECT_EQ_U32(flow.last_capture_tick, 100u);

    flow_sensor_update_stale(&flow, 200u, 100u);
    EXPECT_TRUE(flow.valid);
    flow_sensor_update_stale(&flow, 201u, 100u);
    EXPECT_FALSE(flow.valid);
    EXPECT_TRUE(flow.stale);

    host_hal.capture[0] = 100u;
    host_hal.capture[1] = 101u;
    EXPECT_EQ_I32(flow_sensor_read(&flow), -1);
    EXPECT_FALSE(flow.valid);
    EXPECT_TRUE(flow.stale);

    host_hal.capture[0] = 0u;
    EXPECT_EQ_I32(flow_sensor_read(&flow), 0);
    EXPECT_FALSE(flow.valid);

    flow_sensor_t bad;
    host_hal.tim_base_status = HAL_ERROR;
    flow_sensor_init(&bad, 100u, &htim, &host_tim5,
                     TIM_CHANNEL_2, TIM_CHANNEL_1);
    EXPECT_EQ_I32(bad.ret, -1);
    EXPECT_EQ_I32(flow_sensor_read(NULL), -1);
}

static void test_cli_contract(void)
{
    UART_HandleTypeDef uart = {0};
    cli_t cli;
    memset(&cli, 0xA5, sizeof(cli));
    cli_device_init(&cli, &uart);
    EXPECT_EQ_U32(cli.index, 0u);
    EXPECT_FALSE(cli.msg_pending);
    EXPECT_EQ_U32(cli.line[0], 0u);

    char line[] = "sdcard   soak 100";
    char *tokens[5] = {(char *)0x1, (char *)0x1, (char *)0x1, (char *)0x1, (char *)0x1};
    EXPECT_EQ_I32(tokenize(line, tokens, 5, " \t"), 3);
    EXPECT_TRUE(strcmp(tokens[0], "sdcard") == 0);
    EXPECT_TRUE(strcmp(tokens[1], "soak") == 0);
    EXPECT_TRUE(strcmp(tokens[2], "100") == 0);
    EXPECT_TRUE(tokens[3] == NULL);

    char one[] = "abc def";
    char *small[1] = {(char *)0x1};
    EXPECT_EQ_I32(tokenize(one, small, 1, " "), 0);
    EXPECT_TRUE(small[0] == NULL);
    EXPECT_EQ_I32(tokenize(NULL, tokens, 5, " "), 0);
    EXPECT_EQ_I32(tokenize(line, NULL, 5, " "), 0);
    EXPECT_EQ_I32(tokenize(line, tokens, 0, " "), 0);

    EXPECT_EQ_I32(cli_printline(&cli, "hello"), HAL_OK);
    EXPECT_EQ_U32(host_hal.uart_calls, 2u);
    EXPECT_TRUE(host_hal.uart_capture_len == 7u);
    EXPECT_TRUE(memcmp(host_hal.uart_capture, "hello\r\n", 7u) == 0);

    host_hal.inside_isr = true;
    EXPECT_EQ_I32(cli_printline(&cli, "blocked"), HAL_BUSY);
    EXPECT_EQ_U32(host_hal.uart_calls, 2u);
    EXPECT_EQ_I32(cli_printline(NULL, "x"), HAL_ERROR);
}

static void test_dashboard_contract(void)
{
    UART_HandleTypeDef uart = {0};
    dashboard_t dash;
    memset(&dash, 0xA5, sizeof(dash));
    EXPECT_EQ_I32(dashboard_init(&dash, &uart), 0);
    EXPECT_EQ_U32(dash.line[0], 0u);
    EXPECT_EQ_I32(dashboard_write(&dash, "abc"), HAL_OK);
    EXPECT_TRUE(host_hal.uart_capture_len == 3u);
    EXPECT_TRUE(memcmp(host_hal.uart_capture, "abc", 3u) == 0);
    EXPECT_EQ_I32(dashboard_init(NULL, &uart), -1);
    EXPECT_EQ_I32(dashboard_write(NULL, "abc"), HAL_ERROR);
    EXPECT_EQ_I32(dashboard_write(&dash, NULL), HAL_ERROR);
}

static void put_be16(uint8_t *dst, int16_t value)
{
    uint16_t v = (uint16_t)value;
    dst[0] = (uint8_t)(v >> 8u);
    dst[1] = (uint8_t)v;
}

static void test_mpu6050_contract(void)
{
    I2C_HandleTypeDef i2c = {0};
    mpu6050_t mpu;
    mpu6050_config_t cfg = {
        .addr_7bit = MPU6050_ADDR1,
        .sample_rate_divisor = 7u,
        .external_sync = EXT_SYNC_GYRO_XOUT_L0,
        .lowpass_filter = DLPF_44HZ_BW,
        .gyro_scale = FS_SEL_500,
        .acc_scale = AFS_SEL_4,
        .clock = CLKSEL_XGYRO,
    };
    EXPECT_EQ_I32(mpu6050_init(&mpu, &cfg, &i2c), HAL_OK);
    EXPECT_EQ_U32(host_hal.i2c_write_count, 5u);
    EXPECT_EQ_U32(host_hal.i2c_writes[0].mem_addr, REG_SMPLRT_DIV);
    EXPECT_EQ_U32(host_hal.i2c_writes[0].value, 7u);
    EXPECT_EQ_U32(host_hal.i2c_writes[1].mem_addr, REG_CONFIG);
    EXPECT_EQ_U32(host_hal.i2c_writes[1].value,
                  (uint8_t)(DLPF_44HZ_BW | (EXT_SYNC_GYRO_XOUT_L0 << 3)));
    EXPECT_NEAR(mpu.gyro_div, 65.5, 0.001);
    EXPECT_NEAR(mpu.acc_div, 8192.0, 0.001);

    put_be16(&host_hal.i2c_read_data[0], 8192);
    put_be16(&host_hal.i2c_read_data[2], -8192);
    put_be16(&host_hal.i2c_read_data[4], 4096);
    put_be16(&host_hal.i2c_read_data[6], 340);
    put_be16(&host_hal.i2c_read_data[8], 655);
    put_be16(&host_hal.i2c_read_data[10], -655);
    put_be16(&host_hal.i2c_read_data[12], 0);
    host_hal.i2c_read_len = 14u;
    EXPECT_EQ_I32(mpu6050_read(&mpu), HAL_OK);
    EXPECT_NEAR(mpu.x_acc, 1.0, 0.001);
    EXPECT_NEAR(mpu.y_acc, -1.0, 0.001);
    EXPECT_NEAR(mpu.z_acc, 0.5, 0.001);
    EXPECT_NEAR(mpu.temp, 37.53, 0.01);
    EXPECT_NEAR(mpu.x_gyro, 10.0, 0.01);
    EXPECT_NEAR(mpu.y_gyro, -10.0, 0.01);

    host_hal.i2c_read_status = HAL_TIMEOUT;
    EXPECT_EQ_I32(mpu6050_read(&mpu), HAL_TIMEOUT);
    EXPECT_EQ_I32(mpu6050_read(NULL), HAL_ERROR);
    cfg.gyro_scale = (FS_SEL)4;
    EXPECT_EQ_I32(mpu6050_init(&mpu, &cfg, &i2c), HAL_ERROR);
}

static void test_canbus_contract(void)
{
    CAN_HandleTypeDef hcan = {0};
    CAN_TxHeaderTypeDef header;
    canbus_t bus;
    memset(&bus, 0xA5, sizeof(bus));
    memset(&header, 0xA5, sizeof(header));
    canbus_device_init(&bus, &hcan, &header);
    EXPECT_TRUE(bus.started);
    EXPECT_TRUE(bus.filters_configured);
    EXPECT_EQ_U32(host_hal.can_filter_count, 8u);
    EXPECT_EQ_U32(host_hal.can_start_calls, 1u);
    EXPECT_EQ_U32(header.IDE, CAN_ID_STD);
    EXPECT_EQ_U32(header.RTR, CAN_RTR_DATA);
    EXPECT_EQ_U32(header.DLC, 8u);
    for(size_t i=0; i<host_hal.can_filter_count; i++)
    {
        const CAN_FilterTypeDef *f = &host_hal.can_filters[i];
        EXPECT_TRUE(f->FilterIdHigh != 0u);
        if(i < 7u)
        {
            EXPECT_EQ_U32(f->FilterMode, CAN_FILTERMODE_IDLIST);
            EXPECT_EQ_U32(f->FilterScale, CAN_FILTERSCALE_16BIT);
            EXPECT_TRUE(f->FilterIdLow != 0u);
            EXPECT_TRUE(f->FilterMaskIdHigh != 0u);
            EXPECT_TRUE(f->FilterMaskIdLow != 0u);
        }
        else
        {
            EXPECT_EQ_U32(f->FilterMode, CAN_FILTERMODE_IDMASK);
            EXPECT_EQ_U32(f->FilterScale, CAN_FILTERSCALE_32BIT);
            EXPECT_EQ_U32(f->FilterIdHigh, (0x680u << 5u));
            EXPECT_EQ_U32(f->FilterIdLow, 0u);
            EXPECT_EQ_U32(f->FilterMaskIdHigh, (0x780u << 5u));
            EXPECT_EQ_U32(f->FilterMaskIdLow, 0x0006u);
        }
        EXPECT_EQ_U32(f->FilterBank, i);
    }

    canbus_tx_request_t r1;
    canbus_tx_request_t r2;
    memset(&r1, 0, sizeof(r1));
    memset(&r2, 0, sizeof(r2));
    r1.packet.id = 0x111u;
    r2.packet.id = 0x222u;
    EXPECT_EQ_I32(canbus_queue_tx(&bus, &r1), HAL_OK);
    EXPECT_EQ_I32(canbus_queue_tx(&bus, &r2), HAL_OK);
    EXPECT_EQ_U32(bus.tx_replaced_count, 1u);
    canbus_tx_request_t stored;
    memcpy(&stored, bus.tx_queue_storage, sizeof(stored));
    EXPECT_EQ_U32(stored.packet.id, 0x222u);
    host_queue_force_fail(bus.tx_queue, true);
    EXPECT_EQ_I32(canbus_queue_tx(&bus, &r1), HAL_BUSY);
    EXPECT_EQ_U32(bus.tx_dropped_count, 1u);

    host_hal.can_free_level = 1u;
    EXPECT_EQ_I32(canbus_wait_tx_ready(&bus, 10u), HAL_OK);
    host_hal.can_free_level = 0u;
    host_hal.tick = UINT32_MAX - 2u;
    host_hal.tick_step = 1u;
    EXPECT_EQ_I32(canbus_wait_tx_ready(&bus, 4u), HAL_TIMEOUT);
    EXPECT_TRUE(host_hal.yield_calls > 0u);

    canbus_packet_t p = {.id=0x456u, .data={0,1,2,3,4,5,6,7}};
    host_hal.can_free_level = 0u;
    EXPECT_EQ_I32(canbus_transmit_ready(&bus, &p), HAL_BUSY);
    host_hal.can_free_level = 1u;
    EXPECT_EQ_I32(canbus_transmit_ready(&bus, &p), HAL_OK);
    EXPECT_EQ_U32(host_hal.can_add_calls, 1u);
    EXPECT_EQ_U32(host_hal.can_last_header.StdId, 0x456u);
    EXPECT_TRUE(memcmp(host_hal.can_last_payload, p.data, 8u) == 0);
    canbus_tx_complete_isr(&bus, CAN_TX_MAILBOX0);
    EXPECT_EQ_I32(canbus_wait_tx_complete(&bus, CAN_TX_MAILBOX0, 2u), HAL_OK);
    EXPECT_EQ_U32(bus.tx_complete_count, 1u);

    /* A software-owned mailbox must block *before* HAL enqueue.  Otherwise
     * bxCAN can become free while the completion IRQ is masked and HAL may
     * reuse that mailbox, putting an ambiguously-owned frame on the wire. */
    bus.tx_pending_mailbox_mask = CAN_TX_MAILBOX1;
    uint32_t add_calls_before = host_hal.can_add_calls;
    uint32_t abort_calls_before = host_hal.can_abort_calls;
    uint32_t mailbox = 0u;
    EXPECT_EQ_I32(canbus_transmit_ready_tracked(&bus, &p, &mailbox), HAL_BUSY);
    EXPECT_EQ_U32(host_hal.can_add_calls, add_calls_before);
    EXPECT_EQ_U32(host_hal.can_abort_calls, abort_calls_before);
    EXPECT_EQ_U32(bus.tx_pending_mailbox_mask, CAN_TX_MAILBOX1);
    canbus_tx_complete_isr(&bus, CAN_TX_MAILBOX1);
    EXPECT_EQ_I32(canbus_wait_tx_complete(&bus, CAN_TX_MAILBOX1, 1u), HAL_OK);

    mailbox = 0u;
    EXPECT_EQ_I32(canbus_transmit_ready_tracked(&bus, &p, &mailbox), HAL_OK);
    EXPECT_EQ_U32(mailbox, CAN_TX_MAILBOX0);
    canbus_tx_abort_isr(&bus, mailbox);
    EXPECT_EQ_I32(canbus_wait_tx_complete(&bus, mailbox, 2u), HAL_ERROR);
    EXPECT_EQ_U32(bus.tx_abort_count, 1u);

    host_hal_reset();
    host_hal.can_filter_fail_index = 3;
    memset(&bus, 0, sizeof(bus));
    canbus_device_init(&bus, &hcan, &header);
    EXPECT_FALSE(bus.started);
    EXPECT_FALSE(bus.filters_configured);
}

static void test_can_tx_error_ownership_contract(void)
{
    CAN_HandleTypeDef hcan = {0};
    CAN_TxHeaderTypeDef header = {0};
    canbus_t bus;
    memset(&bus, 0, sizeof(bus));
    canbus_device_init(&bus, &hcan, &header);
    EXPECT_TRUE(bus.started);

    /* RX-only and protocol-warning errors must not retire a live TX token. */
    bus.tx_pending_mailbox_mask = CAN_TX_MAILBOX0;
    canbus_tx_error_isr(&bus, HAL_CAN_ERROR_RX_FOV0 | HAL_CAN_ERROR_EWG |
                              HAL_CAN_ERROR_ACK);
    EXPECT_EQ_U32(bus.tx_pending_mailbox_mask, CAN_TX_MAILBOX0);
    EXPECT_EQ_U32(bus.tx_abort_mailbox_mask, 0u);
    EXPECT_EQ_U32(bus.tx_complete_mailbox_mask, 0u);
    EXPECT_EQ_U32(bus.tx_abort_count, 0u);
    EXPECT_EQ_U32(host_hal.can_abort_calls, 0u);

    /* HAL does not emit AbortCallback for ALST/TERR; that error callback is
     * the terminal outcome for exactly the reported mailbox. */
    canbus_tx_error_isr(&bus, HAL_CAN_ERROR_TX_TERR0);
    EXPECT_EQ_U32(bus.tx_pending_mailbox_mask, 0u);
    EXPECT_EQ_U32(bus.tx_abort_mailbox_mask, CAN_TX_MAILBOX0);
    EXPECT_EQ_U32(bus.tx_abort_count, 1u);
    EXPECT_EQ_I32(canbus_wait_tx_complete(&bus, CAN_TX_MAILBOX0, 1u), HAL_ERROR);
    EXPECT_EQ_U32(bus.tx_abort_mailbox_mask, 0u);

    /* Fatal controller errors request an exact hardware abort while keeping
     * software ownership until hardware reports the real outcome. */
    bus.tx_pending_mailbox_mask = CAN_TX_MAILBOX0 | CAN_TX_MAILBOX2;
    host_hal.can_abort_calls = 0u;
    host_hal.can_last_abort_mailbox = 0u;
    canbus_tx_error_isr(&bus, HAL_CAN_ERROR_BOF);
    EXPECT_EQ_U32(host_hal.can_abort_calls, 1u);
    EXPECT_EQ_U32(host_hal.can_last_abort_mailbox,
                  CAN_TX_MAILBOX0 | CAN_TX_MAILBOX2);
    EXPECT_EQ_U32(bus.tx_pending_mailbox_mask,
                  CAN_TX_MAILBOX0 | CAN_TX_MAILBOX2);
    EXPECT_EQ_U32(bus.tx_abort_mailbox_mask, 0u);

    /* A late TXOK after the abort request is authoritative. */
    canbus_tx_complete_isr(&bus, CAN_TX_MAILBOX0);
    canbus_tx_abort_isr(&bus, CAN_TX_MAILBOX2);
    EXPECT_EQ_U32(bus.tx_pending_mailbox_mask, 0u);
    EXPECT_EQ_U32(bus.tx_complete_mailbox_mask, CAN_TX_MAILBOX0);
    EXPECT_EQ_U32(bus.tx_abort_mailbox_mask, CAN_TX_MAILBOX2);
    EXPECT_EQ_I32(canbus_wait_tx_complete(&bus, CAN_TX_MAILBOX0, 1u), HAL_OK);
    EXPECT_EQ_I32(canbus_wait_tx_complete(&bus, CAN_TX_MAILBOX2, 1u), HAL_ERROR);

    /* If a mailbox-terminal error and BOF share one IRQ, retire the failed
     * mailbox first and abort only the requests that remain pending. */
    bus.tx_pending_mailbox_mask = CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 |
                                  CAN_TX_MAILBOX2;
    host_hal.can_abort_calls = 0u;
    host_hal.can_last_abort_mailbox = 0u;
    canbus_tx_error_isr(&bus, HAL_CAN_ERROR_TX_ALST1 | HAL_CAN_ERROR_BOF);
    EXPECT_EQ_U32(bus.tx_pending_mailbox_mask,
                  CAN_TX_MAILBOX0 | CAN_TX_MAILBOX2);
    EXPECT_TRUE((bus.tx_abort_mailbox_mask & CAN_TX_MAILBOX1) != 0u);
    EXPECT_EQ_U32(host_hal.can_abort_calls, 1u);
    EXPECT_EQ_U32(host_hal.can_last_abort_mailbox,
                  CAN_TX_MAILBOX0 | CAN_TX_MAILBOX2);

    /* A stale error bit for another mailbox cannot steal current ownership. */
    bus.tx_pending_mailbox_mask = CAN_TX_MAILBOX0;
    bus.tx_abort_mailbox_mask = 0u;
    uint32_t unexpected_before = bus.tx_unexpected_callback_count;
    canbus_tx_error_isr(&bus, HAL_CAN_ERROR_TX_TERR2);
    EXPECT_EQ_U32(bus.tx_pending_mailbox_mask, CAN_TX_MAILBOX0);
    EXPECT_EQ_U32(bus.tx_abort_mailbox_mask, 0u);
    EXPECT_EQ_U32(bus.tx_unexpected_callback_count, unexpected_before + 1u);

    /* An abort API failure must not fabricate completion/abort ownership. */
    host_hal.can_abort_status = HAL_ERROR;
    host_hal.can_abort_calls = 0u;
    canbus_tx_error_isr(&bus, HAL_CAN_ERROR_INTERNAL);
    EXPECT_EQ_U32(host_hal.can_abort_calls, 1u);
    EXPECT_EQ_U32(bus.tx_pending_mailbox_mask, CAN_TX_MAILBOX0);

    /* Timeout is the highest-risk ownership window.  First prove that a TXOK
     * arriving just after the primary deadline is reconciled as transmitted,
     * so the caller can advance the inverter rolling counter correctly. */
    host_hal_reset();
    memset(&bus, 0, sizeof(bus));
    canbus_device_init(&bus, &hcan, &header);
    canbus_packet_t p = {.id = 0x0C0u, .data = {1,2,3,4,5,6,7,8}};
    uint32_t mailbox = 0u;
    EXPECT_EQ_I32(canbus_transmit_ready_tracked(&bus, &p, &mailbox), HAL_OK);
    EXPECT_EQ_U32(mailbox, CAN_TX_MAILBOX0);
    g_delay_outcome_bus = &bus;
    g_delay_outcome_mailbox = mailbox;
    g_delay_hook_calls = 0u;
    g_delay_hook_fire_at = 3u; /* first delay inside abort-reconcile window */
    g_delay_hook_complete = true;
    host_hal.tick_step = 0u; /* advance only through osDelay for deterministic hook timing */
    host_hal.delay_hook = delayed_can_outcome_hook;
    EXPECT_EQ_I32(canbus_wait_tx_complete(&bus, mailbox, 2u), HAL_OK);
    EXPECT_EQ_U32(bus.tx_complete_timeout_count, 1u);
    EXPECT_EQ_U32(bus.tx_pending_mailbox_mask, 0u);
    EXPECT_EQ_U32(host_hal.can_abort_calls, 1u);

    /* With no terminal callback, the wait stays bounded but the physical
     * outcome is unknowable.  This is a one-way safety latch: even a terminal
     * callback that arrives after the caller returned cannot safely recover,
     * because the CM200 rolling counter was deliberately not committed. */
    host_hal_reset();
    memset(&bus, 0, sizeof(bus));
    canbus_device_init(&bus, &hcan, &header);
    mailbox = 0u;
    EXPECT_EQ_I32(canbus_transmit_ready_tracked(&bus, &p, &mailbox), HAL_OK);
    EXPECT_EQ_I32(canbus_wait_tx_complete(&bus, mailbox, 2u), HAL_TIMEOUT);
    EXPECT_TRUE((bus.tx_pending_mailbox_mask & mailbox) != 0u);
    EXPECT_TRUE(bus.tx_outcome_uncertain_latched);
    uint32_t add_before_retry = host_hal.can_add_calls;
    EXPECT_EQ_I32(canbus_transmit_ready_tracked(&bus, &p, NULL), HAL_ERROR);
    EXPECT_EQ_U32(host_hal.can_add_calls, add_before_retry);

    /* A late abort proves the hardware token is gone, but it is intentionally
     * too late to undo the caller-level uncertainty decision. */
    canbus_tx_abort_isr(&bus, mailbox);
    EXPECT_EQ_I32(canbus_wait_tx_complete(&bus, mailbox, 1u), HAL_ERROR);
    EXPECT_TRUE(bus.tx_outcome_uncertain_latched);
    EXPECT_EQ_I32(canbus_transmit_ready_tracked(&bus, &p, NULL), HAL_ERROR);
    EXPECT_EQ_U32(host_hal.can_add_calls, add_before_retry);

    /* Explicit device reinitialization is the only recovery boundary. */
    canbus_device_init(&bus, &hcan, &header);
    EXPECT_FALSE(bus.tx_outcome_uncertain_latched);
    EXPECT_EQ_I32(canbus_transmit_ready_tracked(&bus, &p, NULL), HAL_OK);
}

static void test_cooling_control_contract(void)
{
    bool valid = false;
    EXPECT_NEAR(ecu_coolant_temp_sen04_5_from_adc(1365u, &valid),
                37.78, 0.25);
    EXPECT_TRUE(valid);
    EXPECT_NEAR(ecu_coolant_temp_sen04_5_from_adc(286u, &valid),
                93.33, 0.5);
    EXPECT_TRUE(valid);
    (void)ecu_coolant_temp_sen04_5_from_adc(0u, &valid);
    EXPECT_FALSE(valid);
    (void)ecu_coolant_temp_sen04_5_from_adc(4095u, &valid);
    EXPECT_FALSE(valid);

    EXPECT_NEAR(ecu_coolant_pressure_100psi_from_adc(413u, &valid),
                0.0, 0.2);
    EXPECT_TRUE(valid);
    EXPECT_NEAR(ecu_coolant_pressure_100psi_from_adc(3723u, &valid),
                100.0, 0.2);
    EXPECT_TRUE(valid);
    EXPECT_NEAR(ecu_coolant_flow_bv2000_from_hz(62.5f, true, &valid),
                5.0, 0.001);
    EXPECT_TRUE(valid);
    (void)ecu_coolant_flow_bv2000_from_hz(0.0f, false, &valid);
    EXPECT_FALSE(valid);

    ecu_coolant_pump_command_t pump =
        ecu_coolant_pump_command(0.0f, false, false);
    EXPECT_NEAR(pump.pump_s_duty_pct, 12.0, 0.001);
    EXPECT_NEAR(pump.mcu_gate_duty_pct, 88.0, 0.001);
    pump = ecu_coolant_pump_command(50.0f, false, true);
    EXPECT_NEAR(pump.pump_s_duty_pct, 52.5, 0.001);
    EXPECT_NEAR(pump.mcu_gate_duty_pct, 47.5, 0.001);
    EXPECT_TRUE((pump.flags & ECU_COOLING_PUMP_FLAG_MANUAL) != 0u);
    pump = ecu_coolant_pump_command(100.0f, false, false);
    EXPECT_NEAR(pump.pump_s_duty_pct, 93.0, 0.001);
    EXPECT_NEAR(pump.mcu_gate_duty_pct, 7.0, 0.001);
    pump = ecu_coolant_pump_command(NAN, false, false);
    EXPECT_NEAR(pump.mcu_gate_duty_pct, 0.0, 0.001);
    EXPECT_TRUE((pump.flags & ECU_COOLING_PUMP_FLAG_FAILSAFE_MAX) != 0u);

    ecu_cooling_monitor_t monitor;
    ecu_cooling_monitor_init(&monitor);
    ecu_cooling_sample_t sample = {
        .temp_in_c = 30.0f, .temp_out_c = 32.0f,
        .pressure_psi = 10.0f, .flow_lpm = 8.0f,
        .pump_command_pct = 100.0f,
        .temp_in_valid = true, .temp_out_valid = true,
        .pressure_valid = true, .flow_valid = true,
    };
    for(unsigned i = 0u; i < 25u; i++)
        EXPECT_FALSE(ecu_cooling_monitor_update(&monitor, &sample));
    sample.flow_lpm = 3.0f;
    for(unsigned i = 0u; i < 2u; i++)
        EXPECT_FALSE(ecu_cooling_monitor_update(&monitor, &sample));
    EXPECT_TRUE(ecu_cooling_monitor_update(&monitor, &sample));
    EXPECT_TRUE((monitor.fault_flags & ECU_COOLING_FAULT_LOW_FLOW) != 0u);
    sample.flow_lpm = 8.0f;
    for(unsigned i = 0u; i < 9u; i++)
        EXPECT_TRUE(ecu_cooling_monitor_update(&monitor, &sample));
    EXPECT_FALSE(ecu_cooling_monitor_update(&monitor, &sample));
}

static void test_ntc_contract(void)
{
    ADC_HandleTypeDef adc = {0};
    ntc_t ntc;
    memset(&ntc, 0xA5, sizeof(ntc));
    ntc_init(&ntc, &adc, 15u);
    EXPECT_TRUE(ntc.hadc == &adc);
    EXPECT_EQ_U32(ntc.channel, 15u);
    EXPECT_EQ_U32(ntc.count, 0u);
    EXPECT_NEAR(ntc.temp, 0.0, 0.001);
    ntc_init(NULL, &adc, 0u);
}

int main(void)
{
    run_test("map conversion contract", test_map_contract);
    run_test("potentiometer contract", test_potentiometer_contract);
    run_test("pressure sensor contract", test_pressure_sensor_contract);
    run_test("PWM contract", test_pwm_contract);
    run_test("flow sensor contract", test_flow_sensor_contract);
    run_test("CLI contract", test_cli_contract);
    run_test("dashboard contract", test_dashboard_contract);
    run_test("MPU6050 contract", test_mpu6050_contract);
    run_test("CAN bus contract", test_canbus_contract);
    run_test("CAN TX error ownership", test_can_tx_error_ownership_contract);
    run_test("cooling conversions/pump/fault contract", test_cooling_control_contract);
    run_test("NTC initialization contract", test_ntc_contract);

    if(failures != 0)
    {
        printf("ECU DRIVER TESTS FAILED: %d failure(s)\n", failures);
        return 1;
    }
    printf("ALL ECU DRIVER TESTS PASSED\n");
    return 0;
}

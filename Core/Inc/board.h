/**
 * @file board.h
 * @author Cole Bardin (cab572@drexel.edu)
 * @brief
 * @version 0.1
 * @date 2023-04-24
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef ECU_BOARD_H_
#define ECU_BOARD_H_

#include <stdbool.h>

#include "ext_drivers/stm32f767.h"
#include "ext_drivers/poten.h"
#include "ext_drivers/pressure_sensor.h"
#include "ext_drivers/canbus.h"
#include "ext_drivers/cli.h"
#include "ext_drivers/mpu6050.h"
#include "ext_drivers/dashboard.h"
#include "ext_drivers/flow_sensor.h"
#include "ext_drivers/ntc.h"
#include "ext_drivers/pwm.h"
#include "ext_drivers/ams.h"

#define APPS1_0 1650u
#define APPS1_100 2350u
#define APPS2_0 400u
#define APPS2_100 1500u
#define APPS_IMPLAUSIBILITY_MAX 3000u
#define APPS_IMPLAUSIBILITY_MIN 100u

// 0.12V - 1.8V * (3/2) resistor divider => 0.18V - 2.7V
#define BSE1_MIN 280u //Brake emulator min: 155 //Theoretical value (ADC max): 339
#define BSE1_MAX 1600u //Brake emulator max: 2240 //Theoretical value (ADC max): 1900
// 0.14V -1.8V *(3/2) => 0.21V - 2.7V
#define BSE2_MIN 280u //Brake emulator min: 175 //Theoretical value (ADC max): 810
#define BSE2_MAX 1200u//Brake emulator max: 2250 //Theoretical value (ADC max): 2158
#define BSE_IMPLAUSIBILITY_MAX 3000u
#define BSE_IMPLAUSIBILITY_MIN 100u

// TODO: Calibrate
// 0.5V-4.5V Sensor output * 2/3 VDiv = 0.33V-3V * 4095 / 3.3V = 413Ct-3723Ct
#define COOL_PRESS_MIN 413u
#define COOL_PRESS_MAX 3723u

#define BSE1_ADC_CH 13u
#define BSE2_ADC_CH 9u
#define COOL_PRESS_ADC_CH 7u
#define COOL_TEMP1_CH 15u
#define COOL_TEMP2_CH 14u

#define CANBUS_ISR 	0x2u	// Notification bit value for ISR messages
#define CANBUS_APPS	0x1u // Notification bit value for APPS messages

#define ECU_CANBUS_ID 0x69u
#define CM_CANBUS_ID 0x0C0u

typedef struct {
	stm32f767_t stm32f767;
	poten_t apps1;
	poten_t apps2;
	pressure_sensor_t bse1;
	pressure_sensor_t bse2;
	pressure_sensor_t cool_pressure;
 	flow_sensor_t cool_flow;
 	ntc_t cool_temp1;
 	ntc_t cool_temp2;
 	pwm_t cool_pump;
 	canbus_t canbus;
	cli_t cli;
	mpu6050_t mpu6050;
	dashboard_t dashboard;
	pwm_t ssa;
	ams_t ams;
} board_t;

void board_init(board_t *dev);

#endif

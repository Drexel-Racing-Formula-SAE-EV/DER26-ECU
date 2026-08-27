/* DER26 ECU firmware pin contract. Verify against the built .ioc and schematic. */

/* Shutdown and safety */
IMD_Fail       PF14  active high; internal pull-up (disconnected = fault)
BMS_Fail       PF15  active high; internal pull-up (disconnected = fault)
BSPD_OK        PE13  active high at protected 3.3 V interface; internal pull-down
Firmware_Ok    PA7   active high output; initializes low
TSAL_HV_SIG    PC5   active high; internal pull-down
RTD_Go         PE4   active-low button; internal pull-up

/* Motor controller */
MTR_Fault      PB2   active high; internal pull-up
MTR_Ok         PF12  active low; internal pull-up
MTR_EN         PF10  output; initializes low
Cascadia_ON    PA5   output; initializes low
CAN_RX         PD0
CAN_TX         PD1

/* Driver and cooling outputs */
Buzzer         PF13
Brake_Light    PD14
SSA_LED        PB1   TIM3_CH4
CoolPump       PB8   TIM4_CH3

/* Analog and captured inputs */
APPS1          PA3   ADC1_IN3
APPS2          PC0   ADC2_IN10
BSE1           PC3   ADC3_IN13
BSE2           PF3   ADC3_IN9
CoolTemp1      PF5   ADC3_IN15
CoolTemp2      PF4   ADC3_IN14
CoolPress      PF9   ADC3_IN7
CoolFlow       PA0   TIM5_CH1/CH2 capture path

/* CLI USB UART */
USART3_TX      PD8
USART3_RX      PD9

/* Dashboard */
UART7_RX       PF6
UART7_TX       PF7

/* SD / SPI6 */
SPI6_MISO      PG12
SPI6_MOSI      PG14
SPI6_SCK       PG13
SPI6_NSS       PG8

/* MPU6050 */
I2C2_SDA       PF0
I2C2_SCL       PF1

/* Programming and board management */
TCK_SWD        PA14
TMS_SWD        PA13
SWO_SWD        PB3


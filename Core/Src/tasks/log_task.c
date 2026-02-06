/**
* @file log_task.c
* @author Alex Pylaras (ap3782@drexel.edu)
* @brief
* @version 0.1
* @date 2026-01-08
*
* @copyright Copyright (c) 2026
*
*/

#include "tasks/log_task.h"
#include "main.h"
#include "fatfs.h"
#include <stdio.h>

typedef struct {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;
} MPU6050_RawData_t;

typedef struct {
  float ax_g, ay_g, az_g;
  float gx_dps, gy_dps, gz_dps;
} MPU6050_ScaledData_t;

/* DEFINES FOR MPU 6050 (ACCELEROMETER) */
#define MPU6050_ADDR        (0x69)
#define MPU6050_WHO_AM_I    (0x75)
#define MPU6050_PWR_MGMT_1  (0x6B)
#define MPU6050_SMPLRT_DIV  (0x19)
#define MPU6050_CONFIG      (0x1A)
#define MPU6050_GYRO_CONFIG (0x1B)
#define MPU6050_ACCEL_CONFIG (0x1C)
#define MPU6050_ACCEL_XOUT_H  (0x3B)   // start of burst read (accel/temp/gyro)\

/* MPU-6050 interface functions. These functions handle I2C communication with the IMU */
uint8_t MPU6050_Init(void);
HAL_StatusTypeDef MPU6050_WriteReg(uint8_t reg, uint8_t val);
HAL_StatusTypeDef MPU6050_ReadRegs(uint8_t reg, uint8_t *buf, uint16_t len);
void MPU6050_ReadRaw(MPU6050_RawData_t *data);
void MPU6050_Scale(const MPU6050_RawData_t *raw, MPU6050_ScaledData_t *out);

/* MPU-6050 interface functions. These functions handle I2C communication with the IMU */
extern I2C_HandleTypeDef hi2c2;

HAL_StatusTypeDef MPU6050_WriteReg(uint8_t reg, uint8_t val)
{
    return HAL_I2C_Mem_Write(&hi2c2,
                             MPU6050_ADDR << 1,
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             &val,
                             1,
                             100);
}

HAL_StatusTypeDef MPU6050_ReadRegs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c2,
                            MPU6050_ADDR << 1,
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            buf,
                            len,
                            100);
}

uint8_t MPU6050_Init(void)
{
    uint8_t who = 0;

    // 1) Verify device
    if (MPU6050_ReadRegs(MPU6050_WHO_AM_I, &who, 1) != HAL_OK)
        return 0;

    if (who != 0x68)
        return 0;   // not responding / wrong address / wrong wiring

    // 2) Wake up (clear sleep bit)
    if (MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x00) != HAL_OK)
        return 0;
    HAL_Delay(10);

    // 3) Sample rate: 1kHz / (1 + SMPLRT_DIV)
    // 0x04 -> 200 Hz
    MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x04);

    // 4) DLPF config: 0x03 -> ~44 Hz accel, ~42 Hz gyro (good default)
    MPU6050_WriteReg(MPU6050_CONFIG, 0x03);

    // 5) Gyro range: 0x00 -> ±250 dps
    MPU6050_WriteReg(MPU6050_GYRO_CONFIG, 0x00);

    // 6) Accel range: 0x00 -> ±2g
    MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, 0x00);

    return 1;
}

void MPU6050_ReadRaw(MPU6050_RawData_t *data)
{
  uint8_t buf[14];

  if (MPU6050_ReadRegs(MPU6050_ACCEL_XOUT_H, buf, 14) != HAL_OK) {
    // If read fails, you can choose to leave old values or zero them
    return;
  }

  data->ax = (int16_t)((buf[0]  << 8) | buf[1]);
  data->ay = (int16_t)((buf[2]  << 8) | buf[3]);
  data->az = (int16_t)((buf[4]  << 8) | buf[5]);

  // buf[6], buf[7] = temperature (ignored for now)

  data->gx = (int16_t)((buf[8]  << 8) | buf[9]);
  data->gy = (int16_t)((buf[10] << 8) | buf[11]);
  data->gz = (int16_t)((buf[12] << 8) | buf[13]);
}

void MPU6050_Scale(const MPU6050_RawData_t *raw, MPU6050_ScaledData_t *out)
{
  // For ±2g: 16384 LSB/g ; for ±250 dps: 131 LSB/(deg/s)
  out->ax_g   = raw->ax / 16384.0f;
  out->ay_g   = raw->ay / 16384.0f;
  out->az_g   = raw->az / 16384.0f;

  out->gx_dps = raw->gx / 131.0f;
  out->gy_dps = raw->gy / 131.0f;
  out->gz_dps = raw->gz / 131.0f;
}


/**
* @brief Actual Log task function
*
* @param arg App_data struct pointer converted to void pointer
*/
void log_task_fn(void *arg);

TaskHandle_t log_task_start(app_data_t *data)
{
   TaskHandle_t handle;
   xTaskCreate(log_task_fn, "Log task", 1024, (void *)data, LOG_PRIO, &handle);
   return handle;
}

void log_task_fn(void *arg)
{
    app_data_t *data = (app_data_t *)arg;
    uint32_t entry;

    static FATFS FatFs;
    static bool fs_mounted = false;

    MPU6050_RawData_t imu_raw;
    MPU6050_ScaledData_t imu_scaled;

    // Need to give the SD card a moment to settle, otherwise you'll get FR_NOT_READY when mounting
    entry = osKernelGetTickCount();
    osDelayUntil(entry + (1000 / LOG_FREQ));

	// Initial mount
    if (f_mount(&FatFs, "", 1) == FR_OK)
    {
        fs_mounted = true;
    }
    else
    {
        data->log_fault = true;
    }

    /* Initialize MPU-6050 IMU */
    if (!MPU6050_Init())
    {
    	// MPU init failed
    	data->log_fault = true;
    }

    for (;;)
    {
        entry = osKernelGetTickCount();

        MPU6050_ReadRaw(&imu_raw);                 // store raw counts
        MPU6050_Scale(&imu_raw, &imu_scaled);      // optional: store in g and deg/s

        osDelay(100);		// 100 ms


        if (fs_mounted)
        {
            FIL fil;
            FRESULT fres;

            fres = f_open(&fil, "der26.log", FA_WRITE | FA_OPEN_APPEND);
            if (fres == FR_OK)
            {
                char line[256];
                read_time();
                int len = snprintf(
                    line,
                    sizeof(line),
                    "%04u-%02u-%02u %02u:%02u:%02u | hard_fault=%s | soft_fault=%s | "
                    "ax=%.3f ay=%.3f az=%.3f | gx=%.3f gy=%.3f gz=%.3f\r\n",

                    data->datetime.year,
                    data->datetime.month,
                    data->datetime.day,
                    data->datetime.hour,
                    data->datetime.minute,
                    data->datetime.second,
                    data->hard_fault ? "true" : "false",
                    data->soft_fault ? "true" : "false",

                    imu_scaled.ax_g,
                    imu_scaled.ay_g,
                    imu_scaled.az_g,
                    imu_scaled.gx_dps,
                    imu_scaled.gy_dps,
                    imu_scaled.gz_dps
                );


                if (len > 0 && len < sizeof(line))
                {
                	UINT bw;
                    f_write(&fil, line, len, &bw);
                    if (bw < len) {  // Make sure the the SD card didn't run out of space
                    	data->log_fault = true;
                    }
                    f_sync(&fil);
                }


                f_close(&fil);
            }
        }

        osDelayUntil(entry + (1000 / LOG_FREQ));
    }
}

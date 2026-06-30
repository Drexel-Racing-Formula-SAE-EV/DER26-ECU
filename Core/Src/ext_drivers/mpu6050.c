/*
 * mpu6050.c
 *
 *  Created on: Feb 19, 2024
 *      Author: Cole Bardin & Azmain Yousuf
 */

#include "ext_drivers/mpu6050.h"

static const float GYRO_DIVS[4] = {131.0f, 65.5f, 32.8f, 16.4f};
static const float ACC_DIVS[4] = {16384.0f, 8192.0f, 4096.0f, 2048.0f};

static HAL_StatusTypeDef merge_hal_status(HAL_StatusTypeDef current, HAL_StatusTypeDef next)
{
	HAL_StatusTypeDef result = current;

	if((result == HAL_OK) && (next != HAL_OK))
	{
		result = next;
	}

	return result;
}

static int16_t mpu6050_i16_from_be(uint8_t msb, uint8_t lsb)
{
	return (int16_t)(((uint16_t)msb << 8u) | (uint16_t)lsb);
}

HAL_StatusTypeDef mpu6050_init(mpu6050_t *dev, const mpu6050_config_t *conf, I2C_HandleTypeDef *hi2c)
{
	uint8_t temp_data;
	uint16_t dev_addr;
	HAL_StatusTypeDef ret = HAL_OK;

	if((dev == NULL) || (conf == NULL) || (hi2c == NULL))
	{
		return HAL_ERROR;
	}

	if(((uint32_t)conf->gyro_scale >= 4u) ||
	   ((uint32_t)conf->acc_scale >= 4u) ||
	   ((uint32_t)conf->external_sync >= 8u) ||
	   ((uint32_t)conf->lowpass_filter >= 7u) ||
	   ((uint32_t)conf->clock >= 7u))
	{
		return HAL_ERROR;
	}

	dev->addr_7bit = conf->addr_7bit;
	dev->hi2c = hi2c;
	dev->temp = 0.0f;
	dev->x_acc = 0.0f;
	dev->y_acc = 0.0f;
	dev->z_acc = 0.0f;
	dev->x_gyro = 0.0f;
	dev->y_gyro = 0.0f;
	dev->z_gyro = 0.0f;
	dev->gyro_div = GYRO_DIVS[(uint32_t)conf->gyro_scale];
	dev->acc_div = ACC_DIVS[(uint32_t)conf->acc_scale];
	dev->error = HAL_OK;
	dev_addr = (uint16_t)((uint16_t)dev->addr_7bit << 1u);

	ret = merge_hal_status(ret, HAL_I2C_IsDeviceReady(dev->hi2c, dev_addr, 100u, 100u));

	temp_data = conf->sample_rate_divisor;
	ret = merge_hal_status(ret, HAL_I2C_Mem_Write(dev->hi2c, dev_addr, REG_SMPLRT_DIV, I2C_MEMADD_SIZE_8BIT, &temp_data, 1u, 200u));

	temp_data = (uint8_t)((uint8_t)conf->lowpass_filter | ((uint8_t)conf->external_sync << 3u));
	ret = merge_hal_status(ret, HAL_I2C_Mem_Write(dev->hi2c, dev_addr, REG_CONFIG, I2C_MEMADD_SIZE_8BIT, &temp_data, 1u, 200u));

	temp_data = (uint8_t)((uint8_t)conf->gyro_scale << 3u);
	ret = merge_hal_status(ret, HAL_I2C_Mem_Write(dev->hi2c, dev_addr, REG_CONFIG_GYRO, I2C_MEMADD_SIZE_8BIT, &temp_data, 1u, 200u));

	temp_data = (uint8_t)((uint8_t)conf->acc_scale << 3u);
	ret = merge_hal_status(ret, HAL_I2C_Mem_Write(dev->hi2c, dev_addr, REG_CONFIG_ACC, I2C_MEMADD_SIZE_8BIT, &temp_data, 1u, 200u));

	temp_data = (uint8_t)conf->clock;
	ret = merge_hal_status(ret, HAL_I2C_Mem_Write(dev->hi2c, dev_addr, REG_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT, &temp_data, 1u, 200u));

	dev->error = ret;
	return ret;
}

HAL_StatusTypeDef mpu6050_read(mpu6050_t *dev)
{
	HAL_StatusTypeDef ret;
	uint8_t data[14] = {0u};
	int16_t x_acc_raw;
	int16_t y_acc_raw;
	int16_t z_acc_raw;
	int16_t temp_raw;
	int16_t x_gyro_raw;
	int16_t y_gyro_raw;
	int16_t z_gyro_raw;
	uint16_t dev_addr;

	if((dev == NULL) || (dev->hi2c == NULL) || (dev->acc_div <= 0.0f) || (dev->gyro_div <= 0.0f))
	{
		if(dev != NULL)
		{
			dev->error = HAL_ERROR;
		}
		return HAL_ERROR;
	}

	dev_addr = (uint16_t)((uint16_t)dev->addr_7bit << 1u);
	ret = HAL_I2C_Mem_Read(dev->hi2c, dev_addr, ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT, data, 14u, 200u);
	dev->error = ret;

	if(ret != HAL_OK)
	{
		return ret;
	}

	x_acc_raw = mpu6050_i16_from_be(data[0], data[1]);
	y_acc_raw = mpu6050_i16_from_be(data[2], data[3]);
	z_acc_raw = mpu6050_i16_from_be(data[4], data[5]);
	temp_raw = mpu6050_i16_from_be(data[6], data[7]);
	x_gyro_raw = mpu6050_i16_from_be(data[8], data[9]);
	y_gyro_raw = mpu6050_i16_from_be(data[10], data[11]);
	z_gyro_raw = mpu6050_i16_from_be(data[12], data[13]);

	dev->x_acc = (float)x_acc_raw / dev->acc_div;
	dev->y_acc = (float)y_acc_raw / dev->acc_div;
	dev->z_acc = (float)z_acc_raw / dev->acc_div;
	dev->x_gyro = (float)x_gyro_raw / dev->gyro_div;
	dev->y_gyro = (float)y_gyro_raw / dev->gyro_div;
	dev->z_gyro = (float)z_gyro_raw / dev->gyro_div;
	dev->temp = ((float)temp_raw / 340.0f) + 36.53f;

	return HAL_OK;
}

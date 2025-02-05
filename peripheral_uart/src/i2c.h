#ifndef I2C_H
#define I2C_H

#include <zephyr/device.h>
#include <stdint.h>
#include <stddef.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/i2c.h>

#define REPORT_SUCCESS false

// General I2C read/write functions
int i2c_write_register(const struct device *i2c_dev, uint8_t dev_addr, uint8_t reg_addr, uint8_t data);
int i2c_read_register(const struct device *i2c_dev, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data);
int i2c_read_registers(const struct device *i2c_dev, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len);
void i2c_init(void);
void i2c_read_data(void);

bool d_i2c_is_ready(const struct i2c_dt_spec *i2c_dev);
bool d_i2c_write_byte(const struct i2c_dt_spec *i2c_dev, uint8_t address, uint8_t data);
bool d_i2c_read_register(const struct i2c_dt_spec *i2c_dev, uint8_t reg_addr, uint8_t *data);
bool d_i2c_read_registers(const struct i2c_dt_spec *i2c_dev, uint8_t reg_addr, uint8_t *data, size_t len);

#endif

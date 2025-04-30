#include "i2c.h"
#include "BMP280.h"
#include "MPU6050.h"
#include "MLX90614.h"
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>

const struct device *i2c_dev0 = DEVICE_DT_GET(DT_NODELABEL(i2c0));
const struct device *i2c_dev1 = DEVICE_DT_GET(DT_NODELABEL(i2c1));

int i2c_write_register(const struct device *i2c_dev, uint8_t dev_addr, uint8_t reg_addr, uint8_t data) {
    uint8_t buffer[2] = {reg_addr, data};
    return i2c_write(i2c_dev, buffer, sizeof(buffer), dev_addr);
}

int i2c_read_register(const struct device *i2c_dev, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data) {
    return i2c_write_read(i2c_dev, dev_addr, &reg_addr, 1, data, 1);
}

int i2c_read_registers(const struct device *i2c_dev, uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_write_read(i2c_dev, dev_addr, &reg_addr, 1, data, len);
}

void i2c_init(void) {
    if (!device_is_ready(i2c_dev0)) {
        printk("I2C0 device not ready\n");
        return;
    }

    if (!device_is_ready(i2c_dev1)) {
        printk("I2C1 device not ready\n");
        return;
    }

    mpu6050_init(i2c_dev0);
    // mlx90614_init(i2c_dev0);
    // bmp280_init(i2c_dev0);
}

void i2c_read_data(void) {
    read_mpu6050_data(i2c_dev0);
    // read_mlx90614_data(i2c_dev0);
    // read_bmp280_data(i2c_dev0);
}

bool d_i2c_is_ready(const struct i2c_dt_spec *i2c_dev) {
    if (!i2c_is_ready_dt(i2c_dev)){
        printk("I2C bus is not ready\n");
        return false;
    } else {
        return true;
    }
}

bool d_i2c_write_to_reg(const struct i2c_dt_spec *i2c_dev, uint8_t address, uint8_t data)
{
	int ret;
	ret = i2c_reg_write_byte_dt(i2c_dev, address, data);
	if (ret < 0) {
		printk("Failed to write byte %x to register %x\n", data, address);
		return false;
	} else {
		if(REPORT_SUCCESS) printk("Wrote byte %2x to register %2x\n", data, address);
		return true;
	}
}

bool d_i2c_read_register(const struct i2c_dt_spec *i2c_dev, uint8_t reg_addr, uint8_t *data) {
    int ret = i2c_write_read_dt(i2c_dev, &reg_addr, 1, data, 1);
    if (ret < 0) {
        //printk("Failed to read register %x\n", reg_addr);
        return false;
    } else {
        if(REPORT_SUCCESS) printk("Read register %x\n", reg_addr);
        return true;
    }
}

bool d_i2c_read_registers(const struct i2c_dt_spec *i2c_dev, uint8_t reg_addr, uint8_t *data, size_t len) {
    int ret = i2c_write_read_dt(i2c_dev, &reg_addr, 1, data, len);
    if (ret < 0) { 
        //printk("Failed to read %d registers starting from %x\n", len, reg_addr);
        return false;
    } else {
        if(REPORT_SUCCESS) printk("Read %d registers starting from %x\n", len, reg_addr);
        return true;
    }
}

bool d_i2c_read(const struct i2c_dt_spec *i2c_dev, uint8_t *data, size_t len) {
    int ret = i2c_read_dt(i2c_dev, data, len);
    if (ret < 0) {
        printk("Failed to read %d bytes\n", len);
        return false;
    } else {
        if(REPORT_SUCCESS) printk("Read %d bytes\n", len);
        return true;
    }
}

bool d_i2c_write(const struct i2c_dt_spec *i2c_dev, const uint8_t *data, size_t len) {
    int ret = i2c_write_dt(i2c_dev, data, len);
    if (ret < 0) {
        printk("Failed to write %d bytes\n", len);
        return false;
    } else {
        if(REPORT_SUCCESS) printk("Wrote %d bytes\n", len);
        return true;
    }
}
    
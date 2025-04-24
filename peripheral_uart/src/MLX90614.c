#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdio.h>

#include "i2c.h"
#include "mlx90614.h"
#include "aggregator.h"

/**
 * @brief Read a 16-bit register from the MLX90614 sensor.
 *
 * @param i2c_dev   I2C device handle
 * @param reg_addr  Register address
 * @param data      Output pointer for raw register value
 * @return 0 on success, negative error code on failure
 */
int read_mlx90614_register(const struct device *i2c_dev,
                           uint8_t reg_addr,
                           uint16_t *data)
{
    uint8_t buffer[3];
    int ret = i2c_write_read(i2c_dev,
                             MLX90614_ADDR,
                             &reg_addr, 1,
                             buffer, 3);
    if (ret < 0) {
        return ret;
    }

    /* Combine low and high byte */
    *data = buffer[0] | (buffer[1] << 8);
    return 0;
}

/**
 * @brief Probe and wake the MLX90614 on the I2C bus.
 *
 * @param i2c_dev  I2C device handle
 */
void mlx90614_init(const struct device *i2c_dev)
{
    uint8_t device_id;

    if (i2c_read_register(i2c_dev,
                          MLX90614_ADDR,
                          MLX_DEVICE_ID,
                          &device_id) != 0) {
        printk("MLX90614 not detected! (ID=0x%02X)\n", device_id);
        return;
    }

    printk("MLX90614 detected (ID=0x%02X)\n", device_id);
    k_msleep(10);
}

/**
 * @brief Read ambient and object temperatures, convert to °C, and enqueue as JSON.
 *
 * @param i2c_dev  I2C device handle
 */
void read_mlx90614_data(const struct device *i2c_dev)
{
    uint16_t ambient_raw = 0;
    uint16_t object_raw  = 0;
    float    ambient_c   = 0.0f;
    float    object_c    = 0.0f;
    char     message[128];
    int      offset      = 0;

    /* Start JSON */
    memset(message, 0, sizeof(message));
    offset += snprintf(message + offset,
                       sizeof(message) - offset,
                       "{");

    /* Ambient temperature */
    if (read_mlx90614_register(i2c_dev, MLX90614_TA, &ambient_raw) == 0) {
        ambient_c = ambient_raw * 0.02f - 273.15f;  // Convert to °C
        offset += snprintf(message + offset,
                           sizeof(message) - offset,
                           "\"MLX_AmbientTemperature\": {\"Celsius\": %.2f},",
                           (double)ambient_c);
    } else {
        printk("Failed to read MLX90614 ambient temperature\n");
        return;
    }

    /* Object temperature */
    if (read_mlx90614_register(i2c_dev, MLX90614_TOBJ1, &object_raw) == 0) {
        object_c = object_raw * 0.02f - 273.15f;    // Convert to °C
        offset += snprintf(message + offset,
                           sizeof(message) - offset,
                           "\"MLX_ObjectTemperature\": {\"Celsius\": %.2f}",
                           (double)object_c);
    } else {
        printk("Failed to read MLX90614 object temperature\n");
        return;
    }

    /* Close JSON and enqueue */
    strncat(message, "}", sizeof(message) - strlen(message) - 1);
    aggregator_add_data(message);
}

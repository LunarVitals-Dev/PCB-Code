#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include "mlx90614.h"
#include "i2c.h"
#include "aggregator.h"

int read_mlx90614_register(const struct device *i2c_dev, uint8_t reg_addr, uint16_t *data) {
    uint8_t buffer[3];
    int ret = i2c_write_read(i2c_dev, MLX90614_ADDR, &reg_addr, 1, buffer, 3);
    if (ret < 0) {
        return ret;
    }
    *data = (buffer[0] | (buffer[1] << 8)); // Combine high and low byte
    return 0;
}

void mlx90614_init(const struct device *i2c_dev) {
    uint8_t device_id;
    if (i2c_read_register(i2c_dev, MLX90614_ADDR, MLX_DEVICE_ID, &device_id) != 0) {
        printk("MLX90614 not detected! (device_id: 0x%02X)\n", device_id);
        return;
    }

    printk("MLX90614 detected (device_id: 0x%02X)\n", device_id);
    k_msleep(10);
}

void read_mlx90614_data(const struct device *i2c_dev) {
    uint16_t ambient_temp_raw, object_temp_raw;
    float ambient_temp = 0.0, object_temp = 0.0;
    char message[200];
    int message_offset = 0;

    memset(message, 0, sizeof(message));
    
    // Start JSON object
    message_offset += snprintf(message + message_offset, sizeof(message) - message_offset, "{");

    // Read ambient temperature
    if (read_mlx90614_register(i2c_dev, MLX90614_TA, &ambient_temp_raw) == 0) {
        ambient_temp = ambient_temp_raw * 0.02 - 273.15; // Convert to Celsius
        message_offset += snprintf(
            message + message_offset, sizeof(message) - message_offset,
            "\"MLX_AmbientTemperature\": {\"Celsius\": %.2f},",
            ambient_temp
        );
    } else {
        printk("Failed to read MLX90614 Data\n");
        return;
    }

    // Read object temperature
    if (read_mlx90614_register(i2c_dev, MLX90614_TOBJ1, &object_temp_raw) == 0) {
        object_temp = object_temp_raw * 0.02 - 273.15; // Convert to Celsius
        message_offset += snprintf(
            message + message_offset, sizeof(message) - message_offset,
            "\"MLX_ObjectTemperature\": {\"Celsius\": %.2f}",
            object_temp
        );
    } else {
        printk("Failed to read object temperature\n");
        return;
    }

    // Close the JSON object.
    strncat(message, "}", sizeof(message) - strlen(message) - 1);
  
    // Instead of sending immediately, add this sensor's message to the aggregator.
    aggregator_add_data(message);

    // Print or send JSON message
    // printf("%s\n", message);

}


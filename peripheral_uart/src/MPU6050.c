#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include "mpu6050.h"
#include "i2c.h"

void mpu6050_init(const struct device *i2c_dev) {
    uint8_t device_id;

    // Read device_id register
    if (i2c_read_register(i2c_dev, MPU6050_ADDR, DEVICE_ID, &device_id) != 0) {
        printk("MPU6050 not detected! (device_id: 0x%02X)\n", device_id);
        return;
    }

    printk("MPU6050 detected (device_id: 0x%02X)\n", device_id);
    k_msleep(100);

    // Wake up MPU6050 by writing 0x00 to PWR_MGMT_1
    if (i2c_write_register(i2c_dev, MPU6050_ADDR, PWR_MGMT_1, 0x00) != 0) {
        printk("Failed to wake up MPU6050\n");
    } else {
        printk("MPU6050 initialized successfully\n");
    }
}

// // Function to read and print MPU6050 data with string conversion for float
// void read_mpu6050_data(const struct device *i2c_dev) {
//     int16_t accel_x, accel_y, accel_z;
//     int16_t gyro_x, gyro_y, gyro_z;
//     uint8_t data[6];

//     // Read accelerometer data (6 bytes)
//     if (i2c_read_registers(i2c_dev, MPU6050_ADDR, ACCEL_XOUT_H, data, 6) == 0) {
//         accel_x = (int16_t)((data[0] << 8) | data[1]); // Combine high and low byte
//         accel_y = (int16_t)((data[2] << 8) | data[3]);
//         accel_z = (int16_t)((data[4] << 8) | data[5]);

//         // Print raw accelerometer values for debugging
//         //printk("Raw Accelerometer (int16_t): X=%d, Y=%d, Z=%d\n", accel_x, accel_y, accel_z);

//         float accel_x_float = (float)accel_x / 16384.0f; // Convert to float
//         float accel_y_float = (float)accel_y / 16384.0f;
//         float accel_z_float = (float)accel_z / 16384.0f;

//         printk("Accelerometer (g): X=%.4f, Y=%.4f, Z=%.4f\n", accel_x_float, accel_y_float, accel_z_float);

//     } else {
//         printk("Failed to read accelerometer data\n");
//     }

//     // Read gyroscope data (6 bytes)
//     if (i2c_read_registers(i2c_dev, MPU6050_ADDR, GYRO_XOUT_H, data, 6) == 0) {
//         gyro_x = (int16_t)((data[0] << 8) | data[1]);
//         gyro_y = (int16_t)((data[2] << 8) | data[3]);
//         gyro_z = (int16_t)((data[4] << 8) | data[5]);

//         // Print raw gyroscope values for debugging
//         //printk("Raw Gyroscope (int16_t): X=%d, Y=%d, Z=%d\n", gyro_x, gyro_y, gyro_z);
//         float gyro_x_float = (float)gyro_x / 131.0f; // Convert to float
//         float gyro_y_float = (float)gyro_y / 131.0f;
//         float gyro_z_float = (float)gyro_z / 131.0f;

//         printk("Gyroscope (°/s): X=%.4f, Y=%.4f, Z=%.4f\n", gyro_x_float, gyro_y_float, gyro_z_float);

//     } else {
//         printk("Failed to read gyroscope data\n");
//     }
// }
void read_mpu6050_data(const struct device *i2c_dev) {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    uint8_t data[6];
    char message[300];
    int message_offset = 0;

    // Start JSON object
    message_offset += snprintf(message + message_offset, sizeof(message) - message_offset, "[{");

    // Read accelerometer data
    if (i2c_read_registers(i2c_dev, MPU6050_ADDR, ACCEL_XOUT_H, data, 6) == 0) {
        accel_x = (int16_t)((data[0] << 8) | data[1]); // Combine high and low byte
        accel_y = (int16_t)((data[2] << 8) | data[3]);
        accel_z = (int16_t)((data[4] << 8) | data[5]);

        float accel_x_float = (float)accel_x / 16384.0f; // Convert to g
        float accel_y_float = (float)accel_y / 16384.0f;
        float accel_z_float = (float)accel_z / 16384.0f;

        message_offset += snprintf(
            message + message_offset, sizeof(message) - message_offset,
            "\"Accelerometer\": {\"X_g\": %.4f, \"Y_g\": %.4f, \"Z_g\": %.4f},",
            accel_x_float, accel_y_float, accel_z_float
        );
    } else {
        printk("Failed to read accelerometer data\n");
    }

    // Read gyroscope data
    if (i2c_read_registers(i2c_dev, MPU6050_ADDR, GYRO_XOUT_H, data, 6) == 0) {
        gyro_x = (int16_t)((data[0] << 8) | data[1]);
        gyro_y = (int16_t)((data[2] << 8) | data[3]);
        gyro_z = (int16_t)((data[4] << 8) | data[5]);

        float gyro_x_float = (float)gyro_x / 131.0f; // Convert to °/s
        float gyro_y_float = (float)gyro_y / 131.0f;
        float gyro_z_float = (float)gyro_z / 131.0f;

        message_offset += snprintf(
            message + message_offset, sizeof(message) - message_offset,
            "\"Gyroscope\": {\"X_deg_per_s\": %.4f, \"Y_deg_per_s\": %.4f, \"Z_deg_per_s\": %.4f},",
            gyro_x_float, gyro_y_float, gyro_z_float
        );
    } else {
        //printk("Failed to read gyroscope data\n");
    }

    // Remove trailing comma if necessary and close JSON object
    if (message_offset > 1 && message[message_offset - 1] == ',') {
        message[message_offset - 1] = '\0'; // Replace last comma with null terminator
    }
    strcat(message, "}]");
    printf("%s\n", message);
    // Print or send JSON message
    //printf("%s\n", message);
    send_message_to_bluetooth(message);
    // send_message_to_bluetooth(message);  // Uncomment if Bluetooth transmission is implemented
}

// void read_mpu6050_data(const struct device *i2c_dev, char *buffer, size_t size) {
//     int16_t accel_x, accel_y, accel_z;
//     int16_t gyro_x, gyro_y, gyro_z;
//     uint8_t data[6];

//     // Read accelerometer data (6 bytes)
//     if (i2c_read_registers(i2c_dev, MPU6050_ADDR, ACCEL_XOUT_H, data, 6) == 0) {
//         accel_x = (int16_t)((data[0] << 8) | data[1]);
//         accel_y = (int16_t)((data[2] << 8) | data[3]);
//         accel_z = (int16_t)((data[4] << 8) | data[5]);

//         // Convert to G
//         float accel_x_float = (float)accel_x / 16384.0f;
//         float accel_y_float = (float)accel_y / 16384.0f;
//         float accel_z_float = (float)accel_z / 16384.0f;

//         // Read gyroscope data (6 bytes)
//         if (i2c_read_registers(i2c_dev, MPU6050_ADDR, GYRO_XOUT_H, data, 6) == 0) {
//             gyro_x = (int16_t)((data[0] << 8) | data[1]);
//             gyro_y = (int16_t)((data[2] << 8) | data[3]);
//             gyro_z = (int16_t)((data[4] << 8) | data[5]);

//             // Convert to degrees/second
//             float gyro_x_float = (float)gyro_x / 131.0f;
//             float gyro_y_float = (float)gyro_y / 131.0f;
//             float gyro_z_float = (float)gyro_z / 131.0f;

//             // Format the JSON object
//             snprintf(buffer, size,
//                      "\"MPU6050\": {"
//                      "\"Accelerometer\": {\"X\": %.4f, \"Y\": %.4f, \"Z\": %.4f}, "
//                      "\"Gyroscope\": {\"X\": %.4f, \"Y\": %.4f, \"Z\": %.4f}"
//                      "}",
//                      accel_x_float, accel_y_float, accel_z_float,
//                      gyro_x_float, gyro_y_float, gyro_z_float);
//         } else {
//             snprintf(buffer, size, "\"MPU6050\": {\"Error\": \"Failed to read gyroscope data\"}");
//         }
//     } else {
//         snprintf(buffer, size, "\"MPU6050\": {\"Error\": \"Failed to read accelerometer data\"}");
//     }
// }

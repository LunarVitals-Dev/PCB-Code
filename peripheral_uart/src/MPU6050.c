#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/printk.h>
#include <zephyr/kernel.h>
#include <math.h>
#include "mpu6050.h"
#include "i2c.h"

// Step detection configuration
#define WINDOW_SIZE         10   // Moving average window size
#define STEP_THRESHOLD     0.10f  // Minimum acceleration magnitude to register as step
#define STEP_COOLDOWN_MS   600  // Minimum time between steps
#define GYRO_STEP_THRESHOLD 3.0f // Gyroscope threshold for step detection

// Step counter structure
typedef struct {
    float magnitude_buffer[WINDOW_SIZE];
    uint8_t buffer_index;
    uint32_t last_step_time;
    uint32_t step_count;
    float moving_average;
    float accel_x;
    float accel_y;
    float accel_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
} StepCounter;

static StepCounter step_counter;

// Initialize the step counter
static void init_step_counter(void) {
    step_counter.buffer_index = 0;
    step_counter.last_step_time = 0;
    step_counter.step_count = 0;
    step_counter.moving_average = 0;
    
    for(int i = 0; i < WINDOW_SIZE; i++) {
        step_counter.magnitude_buffer[i] = 0;
    }
}

// Process acceleration data for step counting
static int process_step_detection(float accel_x, float accel_y, float accel_z, float gyro_y, float gyro_z) {
    // Remove gravity using a high-pass filter approximation
    float magnitude = fabsf(sqrtf(
        accel_x * accel_x + 
        accel_y * accel_y + 
        accel_z * accel_z
    ) - 1.0f);
    
    // Update moving average
    step_counter.moving_average -= step_counter.magnitude_buffer[step_counter.buffer_index] / WINDOW_SIZE;
    step_counter.magnitude_buffer[step_counter.buffer_index] = magnitude;
    step_counter.moving_average += magnitude / WINDOW_SIZE;
    
    // Get current time in milliseconds
    uint32_t current_time = k_uptime_get_32();

    // printk("Magnitude: %f\n", magnitude);
    // printk("Moving Average: %f\n", step_counter.moving_average);
    
    // Detect step using peak detection and gyroscope data
    if (magnitude > STEP_THRESHOLD && 
        magnitude > step_counter.moving_average &&
        fabs(gyro_y) > GYRO_STEP_THRESHOLD &&
        fabs(gyro_z) > GYRO_STEP_THRESHOLD &&
        (current_time - step_counter.last_step_time) > STEP_COOLDOWN_MS) {
        step_counter.step_count++;
        step_counter.last_step_time = current_time;
        //printk("Total steps: %d\n", step_counter.step_count);
       
    }
    
    step_counter.buffer_index = (step_counter.buffer_index + 1) % WINDOW_SIZE;
    return step_counter.step_count;
}

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

    // Initialize step counter
    init_step_counter();
}

void read_mpu6050_data(const struct device *i2c_dev) {
    int16_t accel_x, accel_y, accel_z;
    int16_t gyro_x, gyro_y, gyro_z;
    uint8_t data[6];
    char message[300];
    int message_offset = 0;
    // Start JSON object
    message_offset += snprintf(message + message_offset, sizeof(message) - message_offset, "[{");
    // Read accelerometer data (6 bytes)
    if (i2c_read_registers(i2c_dev, MPU6050_ADDR, ACCEL_XOUT_H, data, 6) == 0) {
        accel_x = (int16_t)((data[0] << 8) | data[1]);
        accel_y = (int16_t)((data[2] << 8) | data[3]);
        accel_z = (int16_t)((data[4] << 8) | data[5]);

        step_counter.accel_x = (float)accel_x / 16384.0f;
        step_counter.accel_y = (float)accel_y / 16384.0f;
        step_counter.accel_z = (float)accel_z / 16384.0f;

        //printk("Accelerometer (g): X=%.2f, Y=%.2f, Z=%.2f\n", step_counter.accel_x, step_counter.accel_y, step_counter.accel_z);
        message_offset += snprintf(
            message + message_offset, sizeof(message) - message_offset,
            "\"MPU_Accelerometer\": {\"X_g\": %.4f, \"Y_g\": %.4f, \"Z_g\": %.4f},",
            step_counter.accel_x, step_counter.accel_y, step_counter.accel_z
        );
    } else {
        printk("Failed to read accelerometer data\n");
         return;
    }

    // Read gyroscope data (6 bytes)
    if (i2c_read_registers(i2c_dev, MPU6050_ADDR, GYRO_XOUT_H, data, 6) == 0) {
        gyro_x = (int16_t)((data[0] << 8) | data[1]);
        gyro_y = (int16_t)((data[2] << 8) | data[3]);
        gyro_z = (int16_t)((data[4] << 8) | data[5]);

        step_counter.gyro_x = (float)gyro_x / 131.0f;
        step_counter.gyro_y = (float)gyro_y / 131.0f;
        step_counter.gyro_z = (float)gyro_z / 131.0f;
        int steps = process_step_detection(step_counter.accel_x, step_counter.accel_y, step_counter.accel_z, step_counter.gyro_y, step_counter.gyro_z);

        // printk("Gyroscope (°/s): X=%.2f, Y=%.2f, Z=%.2f\n", 
        //        step_counter.gyro_x, step_counter.gyro_y, step_counter.gyro_z);

        message_offset += snprintf(
            message + message_offset, sizeof(message) - message_offset,
            "\"MPU_Gyroscope\": {\"X_deg_per_s\": %.2f, \"Y_deg_per_s\": %.2f, \"Z_deg_per_s\": %.2f, \"steps\": %d},",
            step_counter.gyro_x, step_counter.gyro_y,step_counter.gyro_z, steps
        );
       
        
    } else {
        printk("Failed to read gyroscope data\n");
         return;
    }

    // Remove trailing comma if necessary and close JSON object
    if (message_offset > 1 && message[message_offset - 1] == ',') {
        message[message_offset - 1] = '\0'; // Replace last comma with null terminator
    }
    strcat(message, "}]");
    // Print or send JSON message
    printf("%s\n", message);
    send_message_to_bluetooth(message);
    // Process acceleration and gyroscope data for step detection
}

// Function to get current step count
uint32_t get_step_count(void) {
    return step_counter.step_count;
}

// Function to reset step counter
void reset_step_counter(void) {
    step_counter.step_count = 0;
    printk("Step counter reset to 0\n");
}

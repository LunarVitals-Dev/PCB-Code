#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "mpu6050.h"
#include "i2c.h"
#include "aggregator.h"


/* ACCELEROMETER */
#define MAX_STEP_HISTORY 50       /* Number of recent step timestamps to keep */
#define STEP_WINDOW_MS   10000    /* Time window (ms) for rate calculation */
#define STEP_DEBOUNCE_MS   500    /* Minimum interval (ms) between steps */

/* GYROSCOPE */
#define MAX_GYRO_HISTORY   50        /* how many rotation events to remember */
#define GYRO_WINDOW_MS     10000     /* same window as steps */
#define GYRO_DEBOUNCE_MS   500       /* ignore bounces under 100 ms */
#define GYRO_THRESHOLD     26.0f     /* deg/s change needed to count a “swing” */

/* Step counter state */
typedef struct {
    uint32_t last_step_time;                   /* Timestamp of last detected step */
    float    vector_previous;                  /* Previous acceleration magnitude */
    float    accel_x, accel_y, accel_z;        /* Latest accel readings (g) */
    float    gyro_x, gyro_y, gyro_z;           /* Latest gyro readings (°/s) */
    uint32_t    rate;                             /* Steps per STEP_WINDOW_MS */
    uint32_t step_timestamps[MAX_STEP_HISTORY];/* Circular buffer of step times */
    int      step_history_index;               /* Next write index in buffer */

    float    prev_gyro_mag;                  /* last gyro‐vector magnitude */
    uint32_t last_gyro_time;                 /* last time we counted a rotation */
    uint32_t gyro_timestamps[MAX_GYRO_HISTORY];
    int      gyro_history_index;
    uint32_t rotation_rate;                  /* “swings” in past window */
} StepCounter;

static StepCounter step_counter;

static void init_step_counter(void)
{
    memset(&step_counter, 0, sizeof(step_counter));
}

/**
 * @brief Calculate step rate over the last STEP_WINDOW_MS.
 *
 * @return Number of steps detected in that window.
 */
float calculate_step_rate(void)
{
    uint32_t now = k_uptime_get_32();
    int count = 0;

    for (int i = 0; i < MAX_STEP_HISTORY; i++) {
        uint32_t ts = step_counter.step_timestamps[i];
        if (ts != 0 && (now - ts) <= STEP_WINDOW_MS) {
            count++;
        }
    }

    return count;
}

static float calculate_rotation_rate(void)
{
    uint32_t now = k_uptime_get_32();
    int count = 0;

    for (int i = 0; i < MAX_GYRO_HISTORY; i++) {
        uint32_t ts = step_counter.gyro_timestamps[i];
        if (ts != 0 && (now - ts) <= GYRO_WINDOW_MS) {
            count++;
        }
    }
    return count;
}

/**
 * @brief Detect a step from acceleration data.
 *
 * Uses change in vector magnitude and a debounce interval.
 */
static void process_step_detection(float ax, float ay, float az)
{
    uint32_t now = k_uptime_get_32();
    float vector = sqrtf(ax*ax + ay*ay + az*az);
    float delta  = fabsf(vector - step_counter.vector_previous);

    /* If change exceeds threshold and enough time has passed, count a step */
    if (delta > 0.2f && (now - step_counter.last_step_time) > STEP_DEBOUNCE_MS) {
        step_counter.last_step_time = now;

        /* Store timestamp in circular buffer */
        step_counter.step_timestamps[step_counter.step_history_index] = now;
        step_counter.step_history_index =
            (step_counter.step_history_index + 1) % MAX_STEP_HISTORY;

        /* Update rate and log */
        step_counter.rate = calculate_step_rate();
        // printk("Steps: %u, Rate: %.0f per %u ms\n",
        //        step_counter.step_count, step_counter.rate, STEP_WINDOW_MS);
    }

    step_counter.vector_previous = vector;
}

static void process_rotation_detection(float gx, float gy, float gz)
{
    uint32_t now = k_uptime_get_32();
    float mag   = sqrtf(gx*gx + gy*gy + gz*gz);
    float delta = fabsf(mag - step_counter.prev_gyro_mag);

    if (delta > GYRO_THRESHOLD &&
        (now - step_counter.last_gyro_time) > GYRO_DEBOUNCE_MS) {

        step_counter.last_gyro_time =
            step_counter.gyro_timestamps[
                step_counter.gyro_history_index
            ] = now;

        step_counter.gyro_history_index =
            (step_counter.gyro_history_index + 1) % MAX_GYRO_HISTORY;

        step_counter.rotation_rate = calculate_rotation_rate();
    }

    step_counter.prev_gyro_mag = mag;
}

/**
 * @brief Initialize the MPU6050 sensor and step counter.
 */
void mpu6050_init(const struct device *i2c_dev)
{
    uint8_t device_id;

    /* Verify sensor presence */
    if (i2c_read_register(i2c_dev, MPU6050_ADDR, MPU_DEVICE_ID, &device_id) != 0) {
        printk("MPU6050 not detected! (ID=0x%02X)\n", device_id);
        return;
    }

    printk("MPU6050 detected (ID=0x%02X)\n", device_id);
    k_msleep(10);

    /* Wake sensor */
    if (i2c_write_register(i2c_dev, MPU6050_ADDR, PWR_MGMT_1, 0x00) != 0) {
        printk("Failed to wake up MPU6050\n");
    } else {
        printk("MPU6050 initialized successfully\n");
    }
    /* initialize step_counter state */
    init_step_counter();
}

/**
 * @brief Read accelerometer and gyroscope data, detect steps, and aggregate JSON.
 */
void read_mpu6050_data(const struct device *i2c_dev)
{
    int16_t accel_raw[3], gyro_raw[3];
    uint8_t buf[6];
    char message[256];
    int  offset = 0;

    /* Start JSON */
    memset(message, 0, sizeof(message));
    offset += snprintf(message + offset,
                        sizeof(message) - offset,
                        "{");

    /* Read accelerometer registers */
    if (i2c_read_registers(i2c_dev, MPU6050_ADDR, ACCEL_XOUT_H, buf, 6) != 0) {
        printk("Failed to read accelerometer data\n");
        return;
    }
    accel_raw[0] = (int16_t)((buf[0] << 8) | buf[1]);
    accel_raw[1] = (int16_t)((buf[2] << 8) | buf[3]);
    accel_raw[2] = (int16_t)((buf[4] << 8) | buf[5]);
    step_counter.accel_x = accel_raw[0] / 16384.0f;
    step_counter.accel_y = accel_raw[1] / 16384.0f;
    step_counter.accel_z = accel_raw[2] / 16384.0f;

    process_step_detection(
        step_counter.accel_x,
        step_counter.accel_y,
        step_counter.accel_z
    );

    offset += snprintf(message + offset, sizeof(message) - offset,
                    "\"MPU_Accelerometer\": {"
                    "\"X_g\": %.2f, \"Y_g\": %.2f, \"Z_g\": %.2f, \"step_rate\": %u},",
                    (double)step_counter.accel_x,
                    (double)step_counter.accel_y,
                    (double)step_counter.accel_z,
                    step_counter.rate
    );

    /* Read gyroscope registers */
    if (i2c_read_registers(i2c_dev, MPU6050_ADDR, GYRO_XOUT_H, buf, 6) != 0) {
        printk("Failed to read gyroscope data\n");
        return;
    }
    gyro_raw[0] = (int16_t)((buf[0] << 8) | buf[1]);
    gyro_raw[1] = (int16_t)((buf[2] << 8) | buf[3]);
    gyro_raw[2] = (int16_t)((buf[4] << 8) | buf[5]);
    step_counter.gyro_x = gyro_raw[0] / 131.0f;
    step_counter.gyro_y = gyro_raw[1] / 131.0f;
    step_counter.gyro_z = gyro_raw[2] / 131.0f;

    /* process a rotation “swing” */
    process_rotation_detection(
        step_counter.gyro_x,
        step_counter.gyro_y,
        step_counter.gyro_z
    );

    offset += snprintf(message + offset, sizeof(message) - offset,
                    "\"MPU_Gyroscope\": {"
                    "\"X_deg_per_s\": %.2f, \"Y_deg_per_s\": %.2f, \"Z_deg_per_s\": %.2f, \"rotation_rate\": %u}"
                    ,
                    (double)step_counter.gyro_x,
                    (double)step_counter.gyro_y,
                    (double)step_counter.gyro_z,
                    step_counter.rotation_rate
    );

    /* Close JSON and send */
    strncat(message, "}", sizeof(message) - strlen(message) - 1);
    aggregator_add_data(message);
}


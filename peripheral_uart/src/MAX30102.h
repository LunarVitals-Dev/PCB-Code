/*
    * MAX30102.h
    *
    *  Created on: 2020. 12. 10.
    * 
    * This file was created to interface with the MAX30102 pulse oximeter sensor
    * It uses algorithms developed by Nathan Seidle of Sparkfun Electronics as 
    * adapted in i2c_de.
    * 
    * TODO: need to clean this up and add more functionality
*/

#ifndef MAX30102_H_
#define MAX30102_H_

#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>

#define LED0_NODE DT_ALIAS(led0)
#define LED0_DT_SPEC GPIO_DT_SPEC_GET(LED0_NODE, gpios)

#define MAX30102_NODE DT_NODELABEL(max30102)
#define MAX30102_DT_SPEC I2C_DT_SPEC_GET(MAX30102_NODE)

#define DATA_BUFFER_SIZE 32
typedef struct buffer {
	    uint32_t red[DATA_BUFFER_SIZE];
	    uint32_t ir[DATA_BUFFER_SIZE];
	    uint8_t head_ptr;
	    uint8_t tail_ptr;
} sensor_struct;

typedef enum{
	HEART_RATE = 2,
	SPO2 = 3,
	MULTI_LED = 7
} MAX30102_mode_t;

void max30102_default_setup(const struct i2c_dt_spec *dev_max30102);
void max30102_pulse_oximeter_setup(const struct i2c_dt_spec *dev_max30102, uint8_t sample_avg, bool fifo_rollover, uint8_t fifo_int_threshold, MAX30102_mode_t mode, int sample_rate, int pulse_width, int adc_range);
int max30102_check(const struct i2c_dt_spec *dev_max30102);

void max30102_read_data_hr(const struct i2c_dt_spec *dev_max30102);
// int max30102_read_data_spo2(const struct i2c_dt_spec *dev_max30102, const struct gpio_dt_spec *led0);
void max30102_read_data_spo2(const struct i2c_dt_spec *dev_max30102);

int max30102_available(void);
void max30102_next_sample(void);



int gpio_led_setup(const struct gpio_dt_spec *led0);


// // ------ Global variables used by MAx30102 Functions such as max30102_read_data_hr() ------
// static sensor_struct sensor_data;
// #define BUFFERLENGTH 100

// extern int bufferLength; //buffer length of 100 stores 4 seconds of samples running at 25sps
// extern int spo2;
// extern int heartRate;
// extern int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
// extern int8_t validHeartRate; //indicator to show if the heart rate calculation is valid

// extern uint32_t irBuffer[BUFFERLENGTH]; // infrared LED sensor data
// extern uint32_t redBuffer[BUFFERLENGTH]; // red LED sensor data
// // ------ Global variables used by MAx30102 Functions such as max30102_read_data_hr() ------




#endif 
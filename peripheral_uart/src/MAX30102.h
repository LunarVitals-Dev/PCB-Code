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

#define MAX30102_NODE DT_NODELABEL(max30102)
#define MAX30102_DT_SPEC I2C_DT_SPEC_GET(MAX30102_NODE)

#define MAX30102_FIFO_CONFIG        0x08
#define MAX30102_MODE_CONFIG        0x09
#define MAX30102_SPO2_CONFIG        0x0A
#define MAX30102_LED1_PA            0x0C
#define MAX30102_LED2_PA            0x0D
#define MAX30102_MULTI_LED_CONFIG1  0x11
#define MAX30102_MULTI_LED_CONFIG2  0x12

#define MAX30102_FIFO_WR_PTR        0x04
#define MAX30102_FIFO_O_CNTR        0x05
#define MAX30102_FIFO_RD_PTR        0x06
#define MAX30102_FIFO_DATA          0x07

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
int max30102_get_data(const struct i2c_dt_spec *dev_max30102);
int max30102_read_data(const struct i2c_dt_spec *dev_max30102);
void max30102_print_data(void);

#endif 
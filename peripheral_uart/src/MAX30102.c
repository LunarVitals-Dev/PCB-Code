/*
    * MAX30102.h
    *
    *  Created on: 2020. 12. 10.
    * 
    * This file was created to interface with the MAX30102 pulse oximeter sensor
    * It uses algorithms developed by Nathan Seidle of Sparkfun Electronics as 
    * adapted in `heart_rate.c/h` and `spo2_algorithm.c/h`.
    * 
    * TODO: need to clean this up and add more functionality
*/

#include "MAX30102.h"
#include "i2c.h"
#include "heart_rate.h"
#include "spo2_algorithm.h"

static const uint8_t MAX30102_FIFO_CONFIG        = 0x08;
static const uint8_t MAX30102_MODE_CONFIG        = 0x09;
static const uint8_t MAX30102_SPO2_CONFIG        = 0x0A;
static const uint8_t MAX30102_LED1_PA            = 0x0C;
static const uint8_t MAX30102_LED2_PA            = 0x0D;
static const uint8_t MAX30102_MULTI_LED_CONFIG1  = 0x11;
static const uint8_t MAX30102_MULTI_LED_CONFIG2  = 0x12;

static const uint8_t MAX30102_FIFO_WR_PTR        = 0x04;
static const uint8_t MAX30102_FIFO_O_CNTR        = 0x05;
static const uint8_t MAX30102_FIFO_RD_PTR        = 0x06;
static const uint8_t MAX30102_FIFO_DATA          = 0x07;
#define RATE_SIZE 4
uint8_t rates[RATE_SIZE]; //Array of heart rates
uint8_t rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute = 0;
int beatAvg = 0;
//static const struct i2c_dt_spec max30102_dev = MAX30102_DT_SPEC;
static sensor_struct sensor_data; // I COMMMENTED THIS OUT TO ADD TO .h FILE - ALLAN Feb17

void max30102_default_setup(const struct i2c_dt_spec *dev_max30102)
{
    max30102_pulse_oximeter_setup(dev_max30102, 1, false, 15, SPO2, 100, 441, 4096);
}

/*
 * @brief Setup the pulse oximeter
 * @param sample_avg The number of samples to average. Must be a multiple of 2 between 1 and 32 - default 4
 * @param fifo_rollover Whether the FIFO should rollover. Must be true or false - default true
 * @param fifo_int_threshold The FIFO interrupt threshold. Must be between 0 and 15 - default 4
 * @param mode The mode of the pulse oximeter. Must be one of HEART_RATE, SPO2, or MULTI_LED - default MULTI_LED
 * @param sample_rate The sample rate of the ADC. Must be one of 50, 100, 200, 400, 800, 1000, 1600, 3200 - default 50
 * @param pulse_width The pulse width of the LED. Must be one of 69, 118, 215, 411 - default 69
 * @param adc_range The range of the ADC. Must be one of 2048, 4096, 8192, 16384 - default 16384
 */
void max30102_pulse_oximeter_setup(const struct i2c_dt_spec *dev_max30102, uint8_t sample_avg, bool fifo_rollover, 
		uint8_t fifo_int_threshold, MAX30102_mode_t mode, int sample_rate, int pulse_width, int adc_range)
{
    uint8_t address, data;

    // check i2c_device is ready
    if (!d_i2c_is_ready(dev_max30102)) {
        return;
    }

    // reset the device
    address = MAX30102_MODE_CONFIG;
    data = 0x40; // x_1_xxx_xxx
    if (d_i2c_write_to_reg(dev_max30102, address, data) == false) {
		printk("Failed to reset device\n");
		return;
	}

	// set the sample average (SMP_AVE) and FIFO rollover (FIFO_ROLLOVER_EN) and FIFO interrupt threshold (FIFO_A_FULL)
	address = MAX30102_FIFO_CONFIG;
	data = 0x00;
    switch(sample_avg){
		case 1:
			data |= 0x00; // 000_x_xxxx
			break;
		case 2:
			data |= 0x20; // 001_x_xxxx
			break;
		case 4:
			data |= 0x40; // 010_x_xxxx
			break;
		case 8:
			data |= 0x60; // 011_x_xxxx
			break;
		case 16:
			data |= 0x80; // 100_x_xxxx
			break;
		case 32:
			data |= 0xA0; // 101_x_xxxx
			break;
		default:
			data |= 0x40; // 010_x_xxxx
			break;
	}
	switch(fifo_rollover){
		case true:
			data |= 0x10; // xxx_1_xxxx
			break;
		case false:
			data |= 0x00; // xxx_0_xxxx
			break;
	}
	data |= (fifo_int_threshold & 0x0F); // xxx_x_DDDD
	d_i2c_write_to_reg(dev_max30102, address, data);

	//mode = heart rate // 0_0_xxx_010
	//mode = multi led // 0_0_xxx_111
	//mode = sp02 // 0_0_xxx_011
	address = MAX30102_MODE_CONFIG;
	data = mode & 0x07; // 0_0_000_111 
	d_i2c_write_to_reg(dev_max30102, address, data);

	//adc range = 16384, sample rate = 50, pulse width = 69 // x11_000_00
	//SP02 ADC = 4096, sample rate = 100, pulse width = 411 // x01_001_11

	address = MAX30102_SPO2_CONFIG;
	data = 0x00;
	switch(adc_range){
		case 2048:
			data |= 0x00; // x_00_xxx_xx
			break;
		case 4096:
			data |= 0x20; // x_01_xxx_xx
			break;
		case 8192:
			data |= 0x40; // x_10_xxx_xx
			break;
		case 16384:
			data |= 0x60; // x_11_xxx_xx
			break;
		default:
			data |= 0x60; // x_11_xxx_xx
			break;
	}

	switch(sample_rate){
		case 50:
			data |= 0x00; // x_xx_000_xx
			break;
		case 100:
			data |= 0x04; // x_xx_001_xx
			break;
		case 200:
			data |= 0x08; // x_xx_010_xx
			break;
		case 400:
			data |= 0x0C; // x_xx_011_xx
			break;
		case 800:
			data |= 0x10; // x_xx_100_xx
			break;
		case 1000:
			data |= 0x14; // x_xx_101_xx
			break;
		case 1600:
			data |= 0x18; // x_xx_110_xx
			break;
		case 3200:
			data |= 0x1C; // x_xx_111_xx
			break;
		default:
			data |= 0x00; // x_xx_000_xx
			break;
	}
	switch(pulse_width){
		case 69:
			data |= 0x00; // x_xx_xxx_00
			break;
		case 118:
			data |= 0x01; // x_xx_xxx_01
			break;
		case 215:
			data |= 0x02; // x_xx_xxx_10
			break;
		case 411:
			data |= 0x03; // x_xx_xxx_11
			break;
		default:
			data |= 0x00; // x_xx_xxx_00
			break;

	}
	d_i2c_write_to_reg(dev_max30102, address, data);
	
	// set power level to 6.2mA (0x1F)
	address = MAX30102_LED1_PA;
	data = 0x1F;
	d_i2c_write_to_reg(dev_max30102, address, data);

	address = MAX30102_LED2_PA;
	data = 0x1F;
	d_i2c_write_to_reg(dev_max30102, address, data);

	// // multi_led control registers
	// address = 0x11; 
	// data = 0x07; // x_001_x_010 (RED and IR LED)
	// write_byte(address, data);

	// address = 0x12;
	// data = 0x07; // x_000_x_000 (NO LED)
	// write_byte(address, data);

	// clear fifo
	d_i2c_write_to_reg(dev_max30102, MAX30102_FIFO_WR_PTR, 0x00);
	d_i2c_write_to_reg(dev_max30102, MAX30102_FIFO_O_CNTR, 0x00);
	d_i2c_write_to_reg(dev_max30102, MAX30102_FIFO_RD_PTR, 0x00);
}

/*
 * @brief Get the data from the pulse oximeter
 * @return The number of samples read
 * TODO: need to implement this better - it currently only reads one sample
 */
int max30102_check(const struct i2c_dt_spec *dev_max30102)
{
	bool ret;

	uint8_t data[6];
	ret = d_i2c_read_registers(dev_max30102, 0x07, data, 6);
	if (ret == true){ 
		sensor_data.head_ptr ++;
		sensor_data.head_ptr %= DATA_BUFFER_SIZE;	
		sensor_data.red[sensor_data.head_ptr] = ((data[0] << 16) | (data[1] << 8) | data[2]) & 0x3FFFF;
		sensor_data.ir[sensor_data.head_ptr] = ((data[3] << 16) | (data[4] << 8) | data[5]) & 0x3FFFF;
	}
	return 6;
}

int gpio_led_setup(const struct gpio_dt_spec *led0) {
	if(!gpio_is_ready_dt(led0)) printk("GPIO is not ready\n");
	int ret = gpio_pin_configure_dt(led0, GPIO_OUTPUT);
	if (ret < 0) {
		printk("Failed to configure LED0\n");
		return -1;
	}
	return 0;
}

int gpio_led_toggle(const struct gpio_dt_spec *led0) {
	int ret = gpio_pin_toggle_dt(led0);
	if (ret < 0) {
		printk("Failed to toggle LED0\n");
		return -1;
	}
	return 0;
}

int max30102_available(void)
{
	int numSamples = sensor_data.head_ptr - sensor_data.tail_ptr;
	if (numSamples < 0) numSamples += DATA_BUFFER_SIZE;
	return numSamples;
}

void max30102_next_sample(void)
{
	if(max30102_available() > 0){
		sensor_data.tail_ptr++;
		sensor_data.tail_ptr %= DATA_BUFFER_SIZE;
	}
}



int max_print_counter = 0;
void max30102_read_data_hr(const struct i2c_dt_spec *dev_max30102){ // gets looped

	if (max30102_check(dev_max30102) == 0){
		printk("No data\n");
	} else {

		long irVal = sensor_data.ir[sensor_data.head_ptr];

		// Check if the finger is on the sensor by looking at the IR value.
		if (irVal < 100000) {
			beatAvg = 0;
		} else {
			// Finger is detected on the sensor - proceed with beat detection
			if (checkForBeat(irVal) == true) { // only uses IR value to calculate HR
				// We sensed a beat!
				long delta = k_uptime_get() - lastBeat;
				lastBeat = k_uptime_get();
				beatsPerMinute = 60 / (delta / 1000.0);
			
				if (beatsPerMinute < 200 && beatsPerMinute > 30) {
					rates[rateSpot++] = (uint8_t)beatsPerMinute; // Store this reading in the array
					rateSpot %= RATE_SIZE; // Wrap around
					
					// Take the average of readings
					beatAvg = 0;
					for (uint8_t x = 0; x < RATE_SIZE; x++) {
						beatAvg += rates[x];
					}
					beatAvg /= RATE_SIZE;
				}
			}
		}
		// ---------- Original HR Calculation ------------
		if (max_print_counter == 0){

			char message[200];
			int message_offset = 0;  

			// Add start marker for the JSON object
			message_offset += snprintf(message + message_offset, sizeof(message) - message_offset, "[{");

			// printk(">Red:%d,IR:%d", 
			// sensor_data.red[sensor_data.head_ptr], 
			// sensor_data.ir[sensor_data.head_ptr]);
			
			message_offset += snprintf(
				message + message_offset, sizeof(message) - message_offset,
				"\"MAX_Heart Rate Sensor\": {\"AvgBPM\": %d}",	
				beatAvg
			);
				if (message_offset > 1) {
				message[message_offset - 1] = '\0'; // Remove last comma
			}

			strcat(message, "}]");

			// Print final JSON message
			printf("%s\n", message); // CAN COMMENT THIS OUT AFTER TESTING

			// Send the message over Bluetooth
			send_message_to_bluetooth(message);
		}
		max_print_counter++;
		max_print_counter %= 100; 
	}
}



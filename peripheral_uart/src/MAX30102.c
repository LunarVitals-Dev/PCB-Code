#include "MAX30102.h"
#include "i2c.h"
#include "heart_rate.h"

//static const struct i2c_dt_spec max30102_dev = MAX30102_DT_SPEC;
static sensor_struct sensor_data;

void max30102_default_setup(const struct i2c_dt_spec *dev_max30102)
{
    max30102_pulse_oximeter_setup(dev_max30102, 4, false, 4, MULTI_LED, 50, 69, 16384);
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
void max30102_pulse_oximeter_setup(const struct i2c_dt_spec *dev_max30102, uint8_t sample_avg, bool fifo_rollover, uint8_t fifo_int_threshold, MAX30102_mode_t mode, int sample_rate, int pulse_width, int adc_range)
{
    uint8_t address, data;

    // check i2c_device is ready
    if (!d_i2c_is_ready(dev_max30102)) {
        return;
    }

    // reset the device
    address = MAX30102_MODE_CONFIG;
    data = 0x40; // x_1_xxx_xxx
    if (d_i2c_write_byte(dev_max30102, address, data) == false) {
		printk("Failed to reset device\n");
		return;
	}

    // switch(sample_avg){
	// 	case 1:
	// 		data = 0x00; // 000_x_0000
	// 		break;
	// }

	// // sample avg = 4, rollover disabled, almost full = 17 // 010_0_1111
	address = MAX30102_FIFO_CONFIG;
	data = 0x0F; // 001_0_1111
	d_i2c_write_byte(dev_max30102, address, data);

	//mode = multi led // 0_0_xxx_111
	//mode = sp02 // 0_0_xxx_011
	address = MAX30102_MODE_CONFIG;
	data = 0x03; 
	d_i2c_write_byte(dev_max30102, address, data);

	//adc range = 16384, sample rate = 50, pulse width = 69 // x11_000_00
	//SP02 ADC = 4096, sample rate = 100, pulse width = 411 // x01_001_11
	address = MAX30102_SPO2_CONFIG;
	data = 0x27; 
	d_i2c_write_byte(dev_max30102, address, data);
	
	// set power level to 6.2mA (0x1F)
	address = MAX30102_LED1_PA;
	data = 0x1F;
	d_i2c_write_byte(dev_max30102, address, data);

	address = MAX30102_LED2_PA;
	data = 0x1F;
	d_i2c_write_byte(dev_max30102, address, data);

	// // multi_led control registers
	// address = 0x11; 
	// data = 0x07; // x_001_x_010 (RED and IR LED)
	// write_byte(address, data);

	// address = 0x12;
	// data = 0x07; // x_000_x_000 (NO LED)
	// write_byte(address, data);

	// clear fifo
	d_i2c_write_byte(dev_max30102, MAX30102_FIFO_WR_PTR, 0x00);
	d_i2c_write_byte(dev_max30102, MAX30102_FIFO_O_CNTR, 0x00);
	d_i2c_write_byte(dev_max30102, MAX30102_FIFO_RD_PTR, 0x00);
}

int max30102_get_data(const struct i2c_dt_spec *dev_max30102)
{
    // uint8_t read_ptr, write_ptr, ovf_ctr;
	// read_register(&dev_i2c0, 0x04, &read_ptr);
	// read_register(&dev_i2c0, 0x05, &ovf_ctr);
	// read_register(&dev_i2c0, 0x06, &write_ptr);

	// if (read_ptr == write_ptr) {
	// 	printk("FIFO is empty\n");
	// 	printk("Read pointer: %d, Write pointer: %d, Overflow counter: %d\n", read_ptr, write_ptr, ovf_ctr);
	// 	return 0;
	// } else {
	// 	printk("FIFO is not empty\n");
	// 	printk("Read pointer: %d, Write pointer: %d, Overflow counter: %d\n", read_ptr, write_ptr, ovf_ctr);
	// 	int num_samples_to_read = (write_ptr - read_ptr) & 0x1F; // mask to get the lower 5 bits of the difference

	// 	// know the number of samples we need to read
	// 	int num_bytes_to_read = num_samples_to_read * 6; // 3 bytes for red and 3 bytes for ir for each sample

	// 	// prepare to read data from the fifo buffer address many times


	// 	uint8_t data[num_bytes_to_read];
	// 	read_registers(&dev_i2c0, 0x07, data, num_bytes_to_read);

	// 	for (int i = 0; i < num_bytes_to_read; i += 6) {
	// 		sensor_data.head_ptr ++;
	// 		sensor_data.head_ptr %= DATA_BUFFER_SIZE;
			
	// 		sensor_data.red[sensor_data.head_ptr] = ((data[i] << 16) | (data[i + 1] << 8) | data[i + 2]) & 0x3FFFF;
	// 		sensor_data.ir[sensor_data.head_ptr] = ((data[i + 3] << 16) | (data[i + 4] << 8) | data[i + 5]) & 0x3FFFF;
	// 	}
	// 	return num_samples_to_read;
	// }

	uint8_t data[6];
	bool ret = d_i2c_read_registers(dev_max30102, 0x07, data, 6);
	if (ret == true){
		sensor_data.head_ptr ++;
		sensor_data.head_ptr %= DATA_BUFFER_SIZE;	
		sensor_data.red[sensor_data.head_ptr] = ((data[0] << 16) | (data[1] << 8) | data[2]) & 0x3FFFF;
		sensor_data.ir[sensor_data.head_ptr] = ((data[3] << 16) | (data[4] << 8) | data[5]) & 0x3FFFF;
	}
	return 6;
}

// Global Variables
double beatsPerMinute = 0;
double beatAvg = 0;

int max30102_read_data(const struct i2c_dt_spec *dev_max30102)
{
    const uint8_t RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
	uint8_t rates[RATE_SIZE]; //Array of heart rates
	uint8_t rateSpot = 0;
	long lastBeat = 0; //Time at which the last beat occurred
	

	if (max30102_get_data(dev_max30102) == 0){
		printk("No data\n");
		return -1;  // Return error code when no data
	} else {
		long irVal = sensor_data.ir[sensor_data.head_ptr];

		if (checkForBeat(irVal) == true) {
			//We sensed a beat!
			long delta = k_uptime_get() - lastBeat;
			lastBeat = k_uptime_get();
		
			beatsPerMinute = 60 / (delta / 1000.0);
		
			if (beatsPerMinute < 255 && beatsPerMinute > 20) {
				rates[rateSpot++] = (uint8_t)beatsPerMinute; //Store this reading in the array
				rateSpot %= RATE_SIZE; //Wrap variable
		
				//Take average of readings
				beatAvg = 0;
				for (uint8_t x = 0 ; x < RATE_SIZE ; x++)
				beatAvg += rates[x];
				beatAvg /= RATE_SIZE;
			}
		}

		//printk(">Red:%d,IR:%d,BPM:%.2f,AvgBPM:%.2f\n", sensor_data.red[sensor_data.head_ptr], sensor_data.ir[sensor_data.head_ptr], beatsPerMinute, beatAvg);
	}
	return 0;
}

void max30102_print_data(void)
{
    printk(">Red:%d, IR:%d, BPM:%.2f, AvgBPM:%.2f\n",
           sensor_data.red[sensor_data.head_ptr],
           sensor_data.ir[sensor_data.head_ptr],
           beatsPerMinute,
           beatAvg);
}
/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic UART Bridge Service (NUS) sample
 */
#include "adc.h"

#define ADC_REF_VOLTAGE_MV 600 // Internal reference in mV
#define ADC_GAIN (1.0 / 6.0)    // Gain of 1/6
#define ADC_RESOLUTION 4096     // 12-bit resolution
#define NUMOFADCCHANNELS 2
/* ADC channel configuration via Device Tree */


/* ADC buffer and sequence configuration */
static int16_t buf;
static struct adc_sequence sequence = {
    .buffer = &buf,
    .buffer_size = sizeof(buf), 
    /* Optional calibration */
    //.calibrate = true,
};


// -----------------Filtering ------------------
#include <math.h>  // Include for exponential calculations if needed
    // Filter Variables
int32_t prev_sample = 0; // Derivative filter variable

// Heart Rate Variables
bool rising = false;
uint32_t last_peak_time = 0; // Timestamp of the last valid peak
int32_t previous_val = 0;
int32_t previous_bpm= 0;

// Derivative filter
int32_t derivative_filter(int32_t new_sample, int32_t prev_sample) {
    return new_sample - prev_sample;
}
// -----------------Filtering ------------------

// ------- Heart Rate Calculation ------------------------------
#define BEAT_THRESHOLD 100   // Minimum change to detect a breath
#define MAX_PEAKS 15            // Circular buffer for peak timestamps
#define MIN_PEAK_INTERVAL_MS 600 // Minimum interval between peaks for valid BPM calculation (at least 60 BPM)

// Detect peaks
bool detect_peak(int32_t current_value, int32_t prev_value, bool *rising) {
    if (*rising && current_value < prev_value) {
        *rising = false;
        return true; // Peak detected
    } else if (!*rising && current_value > prev_value + BEAT_THRESHOLD) {
        *rising = true;
        // printf("Rising True\n"); // for debugging
    }
    return false;
}

// Circular buffer to store timestamps of peaks
uint32_t peak_timestamps[MAX_PEAKS];
int peak_index = 0;

void add_peak_timestamp(uint32_t timestamp) {
    peak_timestamps[peak_index] = timestamp;
    peak_index = (peak_index + 1) % MAX_PEAKS;
}

float calculate_BPM(void) {
    int valid_peaks = 0; // Number of valid peaks
    if (peak_index < MAX_PEAKS) {
        valid_peaks = peak_index;
    } else {
        valid_peaks = MAX_PEAKS;
    }

    if (valid_peaks < 2) {
        return 0.0; // Not enough peaks to calculate rate
    }

    // Calculate time intervals between consecutive peaks
    int interval_count = valid_peaks - 1;
    uint32_t intervals[interval_count];
    for (int i = 0; i < interval_count; i++) {
        int current = (peak_index - i - 1 + MAX_PEAKS) % MAX_PEAKS;
        int previous = (current - 1 + MAX_PEAKS) % MAX_PEAKS;
        intervals[i] = peak_timestamps[current] - peak_timestamps[previous];
    }

    // Compute the average interval
    uint32_t sum_intervals = 0;
    for (int i = 0; i < interval_count; i++) {
        sum_intervals += intervals[i];
    }
    float avg_period = (float)sum_intervals / interval_count;

    // Convert average period to beats per minute
    return 60000.0 / avg_period; // 60000 ms per minute
}
// ------- Heart Rate Calculation ------------------------------



int32_t convert_to_mv(int16_t raw_value) {
    return (int32_t)((raw_value * ADC_REF_VOLTAGE_MV * (1.0 / ADC_GAIN)) / ADC_RESOLUTION);
}

void adc_init(){


	for(int i = 0; i < NUMOFADCCHANNELS; i++){
		int err;
		const struct adc_dt_spec *adc_channel = &adc_channels[i];
		if (!adc_is_ready_dt(adc_channel)) {
			printf("ADC controller device %s not ready\n", adc_channel->dev->name);
			return;
		}

		/* Configure ADC channel */
		int err1 = adc_channel_setup_dt(adc_channel);
		if (err1 < 0) {
			printf("Could not setup ADC channel (%d)\n", err);
			return;
		}

		/* Initialize ADC sequence */
		err1 = adc_sequence_init_dt(adc_channel, &sequence);
		if (err1 < 0) {
			printf("Could not initialize ADC sequence (%d)\n", err);
			return;
		}
	}
}

void get_adc_data() {
    char message[200];
    int message_offset = 0;  

    // Add start marker for the JSON object
    message_offset += snprintf(message + message_offset, sizeof(message) - message_offset, "[{");

    for (int i = 0; i < NUMOFADCCHANNELS; i++) {
        const struct adc_dt_spec *adc_channel = &adc_channels[i];

                // Update the ADC sequence to select only this channel
        sequence.channels = BIT(adc_channel->channel_id); 
        // Read ADC data for the current channel
        int err1 = adc_read(adc_channel->dev, &sequence);
        if (err1 < 0) {
            printf("ADC read failed for channel %d (%d)\n", i, err1);
        } else {
            // Convert the raw value to millivolts
            int32_t val_mv = convert_to_mv(buf);
            
            // Get the current uptime in milliseconds
            uint32_t timestamp = k_uptime_get();
            
            // Append to JSON message
            if (i == 0) {//Respiratory Senosr
                message_offset += snprintf(
                    message + message_offset, sizeof(message) - message_offset,
                    "\"RespiratoryRate\": {\"Value_mV\": %d},",
                    val_mv
                );
            } else if (i == 1) {// Pulse Sensor

                int final_bpm = previous_bpm;
                    // Filtering code
                int32_t edge_enhanced = derivative_filter(val_mv, prev_sample);
                prev_sample = val_mv;

                                // BPM calculation
                if (detect_peak(val_mv, previous_val, &rising)) {
                    uint32_t current_time = k_uptime_get(); // Get current uptime in milliseconds

                    // Check if enough time has passed since the last peak
                    if (current_time - last_peak_time >= MIN_PEAK_INTERVAL_MS) {
                        last_peak_time = current_time; // Update the last peak time
                        add_peak_timestamp(current_time); // Add the peak timestamp

                        // Calculate and display the BPM
                        float bpm = calculate_BPM();
                        if (bpm > 0) {
                            final_bpm = bpm;
                            previous_bpm = final_bpm;
                          // bpm =  printf(">b:%.2f\n", bpm); // beats per minute  
                        } 
                    }
                }
                previous_val = val_mv;


                message_offset += snprintf(
                    message + message_offset, sizeof(message) - message_offset,
                    "\"PulseSensor\": {{\"Value_mV\": %d}, {\"BPM\": %d}},",
                    val_mv, final_bpm
                );
            }
        }
    }
     // Remove trailing comma and close JSON object
    if (message_offset > 1) {
        message[message_offset - 1] = '\0'; // Remove last comma
    }

    strcat(message, "}]");

    // Print final JSON message
    printf("%s\n", message);

    // Send the message over Bluetooth
    send_message_to_bluetooth(message);
}
/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @file
 *  @brief Nordic UART Bridge Service (NUS) sample
 */
#include "adc.h"
#include "aggregator.h"

#define ADC_REF_VOLTAGE_MV 600 // Internal reference in mV
#define ADC_GAIN (1.0 / 6.0)    // Gain of 1/6
#define ADC_RESOLUTION 4096     // 12-bit resolution
#define NUMOFADCCHANNELS 2
/* ADC channel configuration via Device Tree */

extern void send_message_to_bluetooth(const char *message);

/* ADC buffer and sequence configuration */
static int16_t buf;
static struct adc_sequence sequence = {
    .buffer = &buf,
    .buffer_size = sizeof(buf), 
    /* Optional calibration */
    //.calibrate = true,
};


// -----------------Filtering Pulse ------------------
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
// -----------------Filtering  Pulse------------------

// -------- Filtering Respiratory---------------------------------------------

#include <math.h>  // Include for exponential calculations if needed
#define MOVING_AVERAGE_WINDOW 5

// Filter Variables
int32_t prev_val_moving_avg_breath = 0;
float prev_BRPM = 0;
// Breathing rate variables 
bool rising_breath = false;
uint32_t last_peak_time_breath = 0; // Timestamp of the last valid peak

/* Moving Average Filter */
int32_t moving_average_filter_breath(int32_t *buffer, int32_t new_sample) {
    static int32_t sum = 0;
    static int index = 0;
    static int32_t data_buffer[MOVING_AVERAGE_WINDOW] = {0};

    sum -= data_buffer[index];
    data_buffer[index] = new_sample;
    sum += new_sample;

    index = (index + 1) % MOVING_AVERAGE_WINDOW;
    return sum / MOVING_AVERAGE_WINDOW;
}
// -------- Filtering  Respiratory---------------------------------------------



// ------- Breathing Rate Calculation ------------------------------
#define BREATHING_THRESHOLD 10   // Minimum change to detect a breath
#define MAX_PEAKS_BREATH 10            // Circular buffer for peak timestamps
#define MIN_PEAK_INTERVAL_MS_BREATH 1500 // ~40 breaths per minute

// Detect peaks
bool detect_peak_breath(int32_t current_value, int32_t prev_value, bool *rising) {
    if (*rising && current_value < prev_value) {
        *rising = false;
        return true; // Peak detected
    } else if (!*rising && current_value > prev_value + BREATHING_THRESHOLD) {
        *rising = true;
        // printf("Rising True\n"); // for debugging
    }
    return false;
}

// Circular buffer to store timestamps of peaks
uint32_t peak_timestamps_breath[MAX_PEAKS_BREATH];
int peak_index_breath = 0;

void add_peak_timestamp_breath(uint32_t timestamp) {
    peak_timestamps_breath[peak_index_breath] = timestamp;
    peak_index_breath = (peak_index_breath + 1) % MAX_PEAKS_BREATH;
}

float calculate_breathing_rate_new(void) {

    int valid_peaks = 0; // Number of valid peaks
    if (peak_index_breath < MAX_PEAKS_BREATH) {
        valid_peaks = peak_index_breath;
    } else {
        valid_peaks = MAX_PEAKS_BREATH;
    }

    if (valid_peaks < 2) {
        return 0.0; // Not enough peaks to calculate rate
    }

    // Calculate time intervals between consecutive peaks
    int interval_count = valid_peaks - 1;
    uint32_t intervals[interval_count];
    for (int i = 0; i < interval_count; i++) {
        int current = (peak_index_breath - i - 1 + MAX_PEAKS_BREATH) % MAX_PEAKS_BREATH;
        int previous = (current - 1 + MAX_PEAKS_BREATH) % MAX_PEAKS_BREATH;
        intervals[i] = peak_timestamps_breath[current] - peak_timestamps_breath[previous];
    }

    // Compute the average interval
    uint32_t sum_intervals = 0;
    for (int i = 0; i < interval_count; i++) {
        sum_intervals += intervals[i];
    }
    float avg_period = (float)sum_intervals / interval_count;

    // Convert average period to breaths per minute
    return 60000.0f / avg_period; // 60000 ms per minute
}
// ------- Breathing Rate Calculation ------------------------------

// ------- Heart Rate Calculation ------------------------------
#define BEAT_THRESHOLD 80   // Minimum change to detect a beat
#define MAX_PEAKS 20            // Circular buffer for peak timestamps
#define MIN_PEAK_INTERVAL_MS 600 // Minimum interval between peaks for valid BPM calculation 

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
    return 60000.0f / avg_period; // 60000 ms per minute
}
// ------- Heart Rate Calculation ------------------------------



int32_t convert_to_mv(int16_t raw_value) {
    return (int32_t)((raw_value * ADC_REF_VOLTAGE_MV * (1.0 / ADC_GAIN)) / ADC_RESOLUTION);
}

void adc_init(){


	for(int i = 0; i < NUMOFADCCHANNELS; i++){
		int err = 0;
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
    char message[400];
    int message_offset = 0;  

    memset(message, 0, sizeof(message));

    // Add start marker for the JSON object
    message_offset += snprintf(message + message_offset, sizeof(message) - message_offset, "{");

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
            
            // Append to JSON message
            if (i == 0) {//Respiratory Senosr

                 // Apply Moving Average Filter
                int32_t moving_avg_breath = moving_average_filter_breath(&prev_val_moving_avg_breath, val_mv);
                int32_t BRPM = prev_BRPM;
                //printf(">m:%d\n", moving_avg); // Moving average value

                // Breathing rate calculation
                if (detect_peak_breath(moving_avg_breath, prev_val_moving_avg_breath, &rising)) {
                    uint32_t current_time_breath = k_uptime_get(); // Get current uptime in milliseconds

                    // Check if enough time has passed since the last peak
                    if (current_time_breath - last_peak_time_breath >= MIN_PEAK_INTERVAL_MS) {
                        last_peak_time_breath = current_time_breath; // Update the last peak time
                        add_peak_timestamp_breath(current_time_breath); // Add the peak timestamp

                        // Calcuulate and display the breathing rate
                        float breathing_rate = calculate_breathing_rate_new();
                        if (breathing_rate > 0) {
                            BRPM = breathing_rate;
                            //printf(">b:%.2f\n", breathing_rate); // breathing rate in breaths per minute  
                        } 
                    }
                }
                prev_val_moving_avg_breath = moving_avg_breath;

                message_offset += snprintf(
                    message + message_offset, sizeof(message) - message_offset,
                    "\"RespiratoryRate\": {\"avg_mV\": %d, \"BRPM\": %d},",
                    moving_avg_breath, BRPM
                );

                prev_BRPM = BRPM;

            } else if (i == 1) {// Pulse Sensor

                int32_t final_bpm = previous_bpm;
                    // Filtering code
                int32_t edge_enhanced = derivative_filter(val_mv, prev_sample);
                prev_sample = val_mv;

                                // BPM calculation
                if (detect_peak(edge_enhanced, previous_val, &rising)) {
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
                previous_val = edge_enhanced;

                // Append to JSON message
                message_offset += snprintf(
                    message + message_offset, sizeof(message) - message_offset,
                    "\"PulseSensor\": {\"Value_mV\": %d, \"pulse_BPM\": %d}",
                    val_mv, final_bpm
                );
            }
        }
    }

    // Close the JSON object.
    strncat(message, "}", sizeof(message) - strlen(message) - 1);
  
    // Instead of sending immediately, add this sensor's message to the aggregator.
    aggregator_add_data(message);

    // Print final JSON message
    // printf("%s\n", message);
}
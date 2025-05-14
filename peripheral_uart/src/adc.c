/** @file
 *  @brief Nordic UART Bridge Service (NUS) sample
 */
#include "adc.h"
#include "aggregator.h"
#include <math.h>  // Include for exponential calculations if needed

#define ADC_REF_VOLTAGE_MV 600 // Internal reference in mV
#define ADC_GAIN (1.0 / 6.0)    // Gain of 1/6
#define ADC_RESOLUTION 4096     // 12-bit resolution
#define NUMOFADCCHANNELS 2

extern void send_message_to_bluetooth(const char *message);

/* ADC buffer and sequence configuration */
static int16_t buf;
static struct adc_sequence sequence = {
    .buffer = &buf,
    .buffer_size = sizeof(buf), 
};


// -----------------Filtering Pulse ------------------
#define TIME_WINDOW_MS 20000

bool peak_detected = false;
int32_t threshold = 1610; 
uint16_t buffer_index = 0;
uint16_t beat_count = 0;
static int32_t prev_val_heart = 0;
int32_t bpm_exp = 0;
uint64_t last_bpm_calc_time = 0;

// -------- Filtering Respiratory---------------------------------------------

// -------- Filtering Respiratory---------------------------------------------   
#define MAX_PEAKS_BREATH               15     
#define MIN_PEAK_INTERVAL_MS_BREATH    1000   
#define BREATH_WINDOW_MS               20000  
#define MOVING_AVERAGE_WINDOW          5

int32_t prev_val_moving_avg_breath = 0;
bool rising_breath = false;
uint32_t last_peak_time_breath = 0;
uint32_t peak_timestamps_breath[MAX_PEAKS_BREATH];
int peak_index_breath = 0;

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

bool detect_peak_breath(int32_t current_value, int32_t prev_value, bool *rising) {
    if (*rising && current_value < prev_value) {
        *rising = false;
    } else if (!*rising && current_value > prev_value + 7) {
        *rising = true;
        return true;
    }
    return false;
}

void add_peak_timestamp_breath(uint32_t timestamp) {
    peak_timestamps_breath[peak_index_breath] = timestamp;
    peak_index_breath = (peak_index_breath + 1) % MAX_PEAKS_BREATH;
}

uint32_t calculate_breathing_rate_windowed(void) {
    uint32_t now = k_uptime_get();
    int count = 0;
    for (int i = 0; i < MAX_PEAKS_BREATH; i++) {
        uint32_t t = peak_timestamps_breath[i];
        if ((now - t) <= BREATH_WINDOW_MS) {
            count++;
        }
    }
    return (uint32_t)(count * 3);
}

int32_t convert_to_mv(int16_t raw_value) {
    return (int32_t)((raw_value * ADC_REF_VOLTAGE_MV * (1.0 / ADC_GAIN)) / ADC_RESOLUTION);
}

void adc_init(){
	for(int i = 0; i < NUMOFADCCHANNELS; i++){
		int err = 0;
		const struct adc_dt_spec *adc_channel = &adc_channels[i];
		if (!adc_is_ready_dt(adc_channel)) {
			printk("ADC controller device %s not ready\n", adc_channel->dev->name);
			return;
		}

		/* Configure ADC channel */
		int err1 = adc_channel_setup_dt(adc_channel);
		if (err1 < 0) {
			printk("Could not setup ADC channel (%d)\n", err);
			return;
		}

		/* Initialize ADC sequence */
		err1 = adc_sequence_init_dt(adc_channel, &sequence);
		if (err1 < 0) {
			printk("Could not initialize ADC sequence (%d)\n", err);
			return;
		}
	}
}

void get_adc_data() {
    for (int i = 0; i < NUMOFADCCHANNELS; i++) {
        const struct adc_dt_spec *adc_channel = &adc_channels[i];

        // Update the ADC sequence to select only this channel
        sequence.channels = BIT(adc_channel->channel_id); 
        // Read ADC data for the current channel
        int err1 = adc_read(adc_channel->dev, &sequence);
        if (err1 < 0) {
            printk("ADC read failed for channel %d (%d)\n", i, err1);
        } else {
            // Convert the raw value to millivolts
            int32_t BRPM = 0;
            int32_t val_mv = convert_to_mv(buf);
            
            if (i == 0) { // Respiratory sensor
                int32_t moving_avg_breath = moving_average_filter_breath(&prev_val_moving_avg_breath, val_mv);

                if (detect_peak_breath(moving_avg_breath, prev_val_moving_avg_breath, &rising_breath)) {
                    uint32_t now = k_uptime_get();
                    if (now - last_peak_time_breath >= MIN_PEAK_INTERVAL_MS_BREATH) {
                        last_peak_time_breath = now;
                        add_peak_timestamp_breath(now);
                    }
                }
                prev_val_moving_avg_breath = moving_avg_breath;

                BRPM = calculate_breathing_rate_windowed();

                aggregator_add_int(moving_avg_breath);
                aggregator_add_int(BRPM);

            } else if (i == 1) {// Pulse Sensor
                uint64_t now = k_uptime_get(); 

                // Peak detection
                if (val_mv > threshold && prev_val_heart <= threshold && !peak_detected) {
                    beat_count++;
                    peak_detected = true;
                } else if (val_mv < threshold) {
                    peak_detected = false;
                }

                prev_val_heart = val_mv;

                uint64_t elapsed_ms = now - last_bpm_calc_time;
                if (elapsed_ms >= TIME_WINDOW_MS) {
                    if (beat_count > 0) {
                        bpm_exp = beat_count * 3;
                    } else {
                        bpm_exp = 0;
                    }

                    beat_count = 0;
                    last_bpm_calc_time = now;
                }

                aggregator_add_int(val_mv);
                aggregator_add_int(bpm_exp);
            }
        }
    }
}
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
static int16_t buf[NUMOFADCCHANNELS];  // Store multiple readings

static struct adc_sequence sequence = {
    .buffer = buf,  // No need for `&buf[i]`, just use `buf`
    .buffer_size = sizeof(buf),
};


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
        printf("🔍 Channel %d -> Dev: %s, Pin: %d, Resolution: %d\n",
               i,
               adc_channel->dev->name,
               adc_channel->channel_id, 
               adc_channel->resolution);
        // Configure sequence for single-channel read
        sequence.buffer = &buf[i];  
        sequence.buffer_size = sizeof(buf[i]);  

        printf("Reading ADC channel %d (Pin: %s)...\n", i, adc_channel->dev->name);

        // Read ADC data
        int err1 = adc_read(adc_channel->dev, &sequence);
        if (err1 < 0) {
            printf("❌ ADC read failed for channel %d (Error %d)\n", i, err1);
        } else {
            // Print raw ADC value
            printf("✅ ADC raw value for channel %d: %d\n", i, buf[i]);

            // Convert raw ADC value to millivolts
            int32_t val_mv = convert_to_mv(buf[i]);  
            printf("🔹 Converted value (mV) for channel %d: %d\n", i, val_mv);

            // Get the current uptime in milliseconds
            uint32_t timestamp = k_uptime_get();
            printf("⏱️ Timestamp: %d ms\n", timestamp);

            // Append to JSON message
            if (i == 0) {
                message_offset += snprintf(
                    message + message_offset, sizeof(message) - message_offset,
                    "\"RespiratoryRate\": {\"Value_mV\": %d},",
                    val_mv
                );
            } else if (i == 1) {
                message_offset += snprintf(
                    message + message_offset, sizeof(message) - message_offset,
                    "\"PulseSensor\": {\"Value_mV\": %d},",
                    val_mv
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
    printf("📡 Final JSON message: %s\n", message);

    // Send the message over Bluetooth
    send_message_to_bluetooth(message);
}


// void get_adc_data() {

//     char message[200];
//     int message_offset = 0;   // Track the position in the message string

//     message_offset += snprintf(message + message_offset, sizeof(message) - message_offset, "{");
//     for (int i = 0; i < NUMOFADCCHANNELS; i++) {
//         const struct adc_dt_spec *adc_channel = &adc_channels[i];
        
//         // Read ADC data for the current channel
//         int err1 = adc_read(adc_channel->dev, &sequence);
//         if (err1 < 0) {
//             printf("ADC read failed for channel %d (%d)\n", i, err1);
//         } else {
//             // Convert the raw value to millivolts
//             int32_t val_mv = convert_to_mv(buf);
            
//             // Get the current uptime in milliseconds
//             uint32_t timestamp = k_uptime_get();
            
//             // Format the message to send via Bluetooth
//             //char message[50];
//             //snprintf(message, sizeof(message), "Channel %d: %d mV, Timestamp: %u\r\n", i, val_mv, timestamp);
//             //printf(message);
//             // Send the message over Bluetooth
//             //send_message_to_bluetooth(message);
            
//             if (i == 0) {
//                 message_offset += snprintf(
//                     message + message_offset, sizeof(message) - message_offset,
//                     "\"RespiratoryRate\": {\"Value_mV\": %d},",
//                     val_mv
//                 );
//             } else if (i == 1) {
//                 message_offset += snprintf(
//                     message + message_offset, sizeof(message) - message_offset,
//                     "\"PulseSensor\": {\"Value_mV\": %d},",
//                     val_mv
//                 );
//             }
//         }
//     }
//       // Remove the trailing comma and close the JSON object
//     if (message_offset > 1) {
//         message[message_offset - 1] = '\0'; // Replace last comma with null terminator
//     }
//     strcat(message, "}");
//     //printf("%s\n", message); // Print JSON string
//     send_message_to_bluetooth(message);
// }

// void get_adc_data(char *buffer, size_t size) {
//     for (int i = 0; i < NUMOFADCCHANNELS; i++) {
//         const struct adc_dt_spec *adc_channel = &adc_channels[i];

//         // Read ADC data for the current channel
//         int err = adc_read(adc_channel->dev, &sequence);
//         if (err < 0) {
//             snprintf(buffer, size, "\"Channel%d\": {\"Error\": \"ADC read failed (%d)\"}", i, err);
//         } else {
//             // Convert the raw value to millivolts
//             int32_t val_mv = convert_to_mv(buf);

//             // Get the current uptime in milliseconds
//             uint32_t timestamp = k_uptime_get();

//             // Format the JSON output for this channel
//             snprintf(buffer, size,
//                      "\"Channel%d\": {\"Value\": %d, \"Timestamp\": %u}",
//                      i, val_mv, timestamp);
//         }

//         // Move the buffer pointer forward for the next channel
//         size_t written = strlen(buffer);
//         buffer += written;
//         size -= written;

//         // Add a comma for separation, except after the last entry
//         if (i < NUMOFADCCHANNELS - 1) {
//             strncat(buffer, ",", size - 1);
//         }
//     }
// }

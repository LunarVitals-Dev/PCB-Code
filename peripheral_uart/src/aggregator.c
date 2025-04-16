#include "aggregator.h"
#include <stdio.h>
#include <string.h>

// Adjust these definitions as necessary.
#define TOTAL_SENSORS 3         // Total number of sensor messages expected per cycle.
#define AGG_BUFFER_SIZE 2048    // The size of the aggregation buffer.

// Global buffer for aggregation and a counter for received messages.
static char aggregated_sensor_data[AGG_BUFFER_SIZE];
static int sensors_received = 0;

// Declare external function. This is your existing Bluetooth send function.
extern void send_message_to_bluetooth(const char *msg);

// Initialize the aggregator. This starts (or resets) the JSON aggregation.
void aggregator_init(void) {
    // Start a JSON array. (Alternatively, if you prefer an object, you could use "{" and "}")
    strcpy(aggregated_sensor_data, "[");
    sensors_received = 0;
}

// This function appends sensor data to the aggregated buffer.
void aggregator_add_data(const char *sensor_msg) {
    // If not the first sensor, add a comma separator.
    if (sensors_received > 0) {
        strncat(aggregated_sensor_data, ",", AGG_BUFFER_SIZE - strlen(aggregated_sensor_data) - 1);
    }
    
    // Append the sensor's JSON snippet.
    strncat(aggregated_sensor_data, sensor_msg, AGG_BUFFER_SIZE - strlen(aggregated_sensor_data) - 1);
    sensors_received++;
    
    // If all sensor data has been collected...
    if (sensors_received == TOTAL_SENSORS) {
        // Close the JSON array.
        strncat(aggregated_sensor_data, "]", AGG_BUFFER_SIZE - strlen(aggregated_sensor_data) - 1);
        // Send the aggregated data via your core Bluetooth function.
        send_message_to_bluetooth(aggregated_sensor_data);
        // Reset for the next cycle.
        aggregator_init();
    }
}

// Optionally, if you need to send out data even when you haven't received all messages.
void aggregator_finalize_and_send(void) {
    // Ensure the JSON array is closed.
    if (aggregated_sensor_data[strlen(aggregated_sensor_data) - 1] != ']') {
        strncat(aggregated_sensor_data, "]", AGG_BUFFER_SIZE - strlen(aggregated_sensor_data) - 1);
    }
    send_message_to_bluetooth(aggregated_sensor_data);
    aggregator_init();
}

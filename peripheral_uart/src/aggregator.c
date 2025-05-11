#include "aggregator.h"
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>

// Total number of sensor messages per JSON array
#define TOTAL_SENSORS  5
// Maximum size of the aggregation buffer
#define AGG_BUFFER_SIZE 1024

// Buffer to build the JSON array, and counter for accumulated messages
static char aggregated_sensor_data[AGG_BUFFER_SIZE];
static int sensors_received = 0;

// External BLE send function (implemented elsewhere)
extern void send_message_to_bluetooth(const char *msg);

// Initialize or reset the JSON aggregation
void aggregator_init(void) {
    memset(aggregated_sensor_data, 0, AGG_BUFFER_SIZE);
    strcpy(aggregated_sensor_data, "[");
    sensors_received = 0;
}

// Append a sensor's JSON snippet. Only accumulates up to TOTAL_SENSORS items
void aggregator_add_data(const char *sensor_msg) {
    if (sensors_received >= TOTAL_SENSORS) {
        // Already have all expected sensors for this cycle
        return;
    }

    if (sensors_received > 0) {
        strncat(
            aggregated_sensor_data,
            ",",
            AGG_BUFFER_SIZE - strlen(aggregated_sensor_data) - 1
        );
    }

    strncat(
        aggregated_sensor_data,
        sensor_msg,
        AGG_BUFFER_SIZE - strlen(aggregated_sensor_data) - 1
    );
    sensors_received++;
}

// Finalize the JSON array, send over BLE, then reset for next cycle
void aggregator_finalize_and_send(void) {
    if (sensors_received == 0) {
        // Nothing to send
        return;
    }

    size_t len = strlen(aggregated_sensor_data);

    // Ensure it ends with a closing bracket
    if (aggregated_sensor_data[len - 1] != ']') {
        strncat(
            aggregated_sensor_data,
            "]",
            AGG_BUFFER_SIZE - strlen(aggregated_sensor_data) - 1
        );
    }

    // Send the complete JSON array
    send_message_to_bluetooth(aggregated_sensor_data);
    // printk("Sending: %s\n", aggregated_sensor_data);

    // Reset for the next cycle
    aggregator_init();
}

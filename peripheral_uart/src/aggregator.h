#ifndef AGGREGATOR_H
#define AGGREGATOR_H

// Call this at startup (or before the sensor data collection cycle)
void aggregator_init(void);

// Called by each sensor module to add its data
void aggregator_add_data(const char *sensor_msg);

// Optionally, a function to force sending the aggregation, even if not all sensors reported
void aggregator_finalize_and_send(void);

#endif // AGGREGATOR_H
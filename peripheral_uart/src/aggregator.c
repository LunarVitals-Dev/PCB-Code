#include <stdio.h>
#include <string.h>

#define AGG_BUFFER_SIZE 128
static char agg_buffer[AGG_BUFFER_SIZE];
static int  values_received;

extern void send_message_to_bluetooth(const char *msg);

void aggregator_init(void) {
    values_received = 0;
    agg_buffer[0]  = '\0';
}

void aggregator_add_int(int v) {
    char tmp[32];
    // no decimals
    snprintf(tmp, sizeof(tmp), "%d", v);
    if (values_received > 0) {
        strncat(agg_buffer, ",", AGG_BUFFER_SIZE - strlen(agg_buffer) - 1);
    }
    strncat(agg_buffer, tmp, AGG_BUFFER_SIZE - strlen(agg_buffer) - 1);
    values_received++;
}

void aggregator_add_float(double v) {
    char tmp[32];
    // exactly one decimal
    snprintf(tmp, sizeof(tmp), "%.1f", v);
    if (values_received > 0) {
        strncat(agg_buffer, ",", AGG_BUFFER_SIZE - strlen(agg_buffer) - 1);
    }
    strncat(agg_buffer, tmp, AGG_BUFFER_SIZE - strlen(agg_buffer) - 1);
    values_received++;
}

void aggregator_finalize_and_send(void) {
    if (values_received == 0) {
        return;
    }
    strncat(agg_buffer, "\n", AGG_BUFFER_SIZE - strlen(agg_buffer) - 1);
    send_message_to_bluetooth(agg_buffer);
    aggregator_init();
}

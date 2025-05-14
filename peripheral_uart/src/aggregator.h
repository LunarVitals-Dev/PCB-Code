#ifndef AGGREGATOR_H
#define AGGREGATOR_H

void aggregator_init(void);
void aggregator_add_int(int v);
void aggregator_add_float(double v);
void aggregator_finalize_and_send(void);

#endif // AGGREGATOR_H
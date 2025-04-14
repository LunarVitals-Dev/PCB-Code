#include "heart_rate.h"

// Global variables for signal tracking and filtering
int16_t IR_AC_Max = 20;
int16_t IR_AC_Min = -20;

int16_t IR_AC_Signal_Current = 0;
int16_t IR_AC_Signal_Previous = 0;
int16_t IR_AC_Signal_min = 0;
int16_t IR_AC_Signal_max = 0;
int16_t IR_Average_Estimated = 0;

bool positiveEdge = false;
bool negativeEdge = false;

int32_t ir_avg_reg = 0;

int16_t cbuf[32];
uint8_t offset = 0;

static const uint16_t FIRCoeffs[12] = {172, 321, 579, 927, 1360, 1858, 2390, 2916, 3391, 3768, 4012, 4096};

// Define a refractory period (in number of samples)
// Adjust REFRACTORY_PERIOD_SAMPLES based on your sampling rate (e.g., 25 samples for 250ms delay)
#define REFRACTORY_PERIOD_SAMPLES 25
static uint16_t refractoryCounter = 0;

// Function prototypes
int16_t averageDCEstimator(int32_t *p, uint16_t x);
int16_t lowPassFIRFilter(int16_t din);
int32_t mul16(int16_t x, int16_t y);

// Heart Rate Monitor function takes a sample value.
// Returns true if a beat is detected.
bool checkForBeat(int32_t sample)
{
  bool beatDetected = false;

  // Save previous filtered signal
  IR_AC_Signal_Previous = IR_AC_Signal_Current;
  
  // Process new data sample: remove the DC component and low-pass filter
  IR_Average_Estimated = averageDCEstimator(&ir_avg_reg, sample);
  IR_AC_Signal_Current = lowPassFIRFilter(sample - IR_Average_Estimated);

  // Decrement the refractory counter if it's active
  if (refractoryCounter > 0) {
    refractoryCounter--;
  }
  
  // Detect positive zero crossing (rising edge)
  if ((IR_AC_Signal_Previous < 0) && (IR_AC_Signal_Current >= 0))
  {
    // Update peak values encountered during the cycle
    IR_AC_Max = IR_AC_Signal_max;
    IR_AC_Min = IR_AC_Signal_min;

    // Reset edge and max/min trackers for the new cycle
    positiveEdge = true;
    negativeEdge = false;
    IR_AC_Signal_max = 0;

    // Only register a beat if the peak-to-peak difference exceeds the threshold
    // and if the refractory period has elapsed.
    if ((IR_AC_Max - IR_AC_Signal_min) > 20 &&
        (IR_AC_Max - IR_AC_Signal_min) < 1000 &&
        (refractoryCounter == 0))
    {
      beatDetected = true;
      // Set the refractory counter to avoid re-triggering too soon
      refractoryCounter = REFRACTORY_PERIOD_SAMPLES;
    }
  }

  // Detect negative zero crossing (falling edge)
  if ((IR_AC_Signal_Previous > 0) && (IR_AC_Signal_Current <= 0))
  {
    positiveEdge = false;
    negativeEdge = true;
    IR_AC_Signal_min = 0;
  }

  // Find maximum value during the positive cycle
  if (positiveEdge && (IR_AC_Signal_Current > IR_AC_Signal_Previous))
  {
    IR_AC_Signal_max = IR_AC_Signal_Current;
  }

  // Find minimum value during the negative cycle
  if (negativeEdge && (IR_AC_Signal_Current < IR_AC_Signal_Previous))
  {
    IR_AC_Signal_min = IR_AC_Signal_Current;
  }
  
  return beatDetected;
}

// Average DC Estimator using a running exponential average
int16_t averageDCEstimator(int32_t *p, uint16_t x)
{
  *p += ((((long)x << 15) - *p) >> 4);
  return (*p >> 15);
}

// Low Pass FIR Filter implementation
int16_t lowPassFIRFilter(int16_t din)
{  
  cbuf[offset] = din;

  // Initial term using the last coefficient
  int32_t z = mul16(FIRCoeffs[11], cbuf[(offset - 11) & 0x1F]);
  
  // Sum symmetric filter contributions
  for (uint8_t i = 0; i < 11; i++)
  {
    z += mul16(FIRCoeffs[i],
               cbuf[(offset - i) & 0x1F] + cbuf[(offset - 22 + i) & 0x1F]);
  }

  offset = (offset + 1) % 32;  // Circular buffer wrap

  return (z >> 15);
}

// Multiplies two 16-bit values returning a 32-bit result
int32_t mul16(int16_t x, int16_t y)
{
  return ((long)x * (long)y);
}

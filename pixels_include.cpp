
#include <stdint.h>
#include <stdbool.h>
#include "pixels_include.h"

// Average two values together by an 8 bit "percent" as a fraction of 255
int hexWeightedMean(int32_t num_a, int32_t num_b, uint8_t percent_b) {
  //return fast255Divide((num_a * (255 - percent_b) + (num_b * percent_b)));
   return (num_a * (255 - percent_b) + (num_b * percent_b)) / 255;
}

float hexWeightedMean(float num_a, float num_b, uint8_t percent_b) {
  return (num_a * (255 - percent_b) + (num_b * percent_b)) / 255;
}

// Fast divide-by-255.
int fast255Divide(int val) {
  return (val + 1 + (val >> 8)) >> 8;
}

// Double exponential weighted moving average.
float smooth2(uint16_t newValue, uint8_t binAlpha, uint8_t binBeta, float *previousValue, float* previousDelta, bool started) {
  int delta = newValue - (*previousValue);
  (*previousDelta) = exponentialAverage(delta, *previousDelta, binBeta);
  return (*previousDelta) + smooth(newValue, binAlpha, previousValue, started);
}

float smooth(int newValue, uint8_t binAlpha, float* previousValue, bool started) {
  if (started) {
  (*previousValue) = exponentialAverage(newValue, *previousValue, binAlpha);
  } else {
  (*previousValue) = newValue;
    
  }
  return *previousValue;
}

// Exponential moving average.
// It should be possible to refactor this into pure integer ops.
float exponentialAverage(int newValue, float oldValue, uint8_t binAlpha) {
  return (((1 << binAlpha) - 1) * oldValue + newValue) / (1 << binAlpha);
}


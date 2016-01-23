#include <stdint.h>
#include <stdbool.h>

float hexWeightedMean(float num_a, float num_b, uint8_t percent_b);
int hexWeightedMean(int32_t num_a, int32_t num_b, uint8_t percent_b);

int fast255Divide(int val);

float smooth2(uint16_t newValue, uint8_t binAlpha, uint8_t binBeta, float *previousValue, float* previousDelta, bool started);

float smooth(int newValue, uint8_t binAlpha, float* previousValue, bool started);

float exponentialAverage(int newValue, float oldValue, uint8_t binAlpha);

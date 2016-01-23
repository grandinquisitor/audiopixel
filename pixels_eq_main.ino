#include <FastLED.h>
#include "pixels_include.h"


// Setup.

// Number of pixels.
static const uint8_t N_PIXELS = 16;

// Pin setup.
static const int NEO_PIXEL_PIN = 2;
static const int EQ_OUTPUT_PIN = A0;
static const int EQ_RESET_PIN = 6;
static const int EQ_STROBE_PIN = 7;


// Parameters.

// 8bit fraction. Absolute brightness level of pixels. Max = 255, which is absolutely blinding.
static const uint8_t MAX_BRIGHTNESS = 40;

// log2 values. Alpha and beta decay periods of the short period moving average.
static const uint8_t SHORT_PERIOD_DECAY_ALPHA = 2;
static const uint8_t SHORT_PERIOD_DECAY_BETA = 2;

// log2 value. Decay the long period moving average by this many bits each cycle.
static const uint8_t LONG_PERIOD_DECAY_ALPHA = 9;

// log2 value. If the difference between the short and long period averages is less than 1 over 2 ^ this amount, fade out the pixel.
static const uint8_t THRESHOLD = 3;

// 8bit fraction. When pixels are off or thresholded, fade them out by this fraction of 255 per cycle.
static const uint8_t FADE_RATE = 24;

// 8bit fraction. Flip pixels on at this brightness at least, as a percent of 255, where 255 has already been projected to MAX_BRIGHTNESS.
static const uint8_t MIN_BRIGHTNESS_ON = 0;

// 8bit fraction. Color saturation level.
static const uint8_t SATURATION = 255;


// Constants.

// Number of bands the spectrum analyzer has.
static const uint8_t NUM_FREQS = 7;

// Amount of fixed precision bits to use for the moving averages.
static const uint8_t FIXED_BITS = 4;

// Memoize the interpolation values from NUM_FREQS <-> N_PIXELS here.
static uint8_t BAND_WEIGHTS[N_PIXELS][2];

// State variables
static CRGB leds[N_PIXELS];
static float longReadMovingAverage[N_PIXELS];
static float shortReadMovingAverage[N_PIXELS];
static float shortReadMovingAverageDelta[N_PIXELS];

void setup() {
  pinMode(EQ_OUTPUT_PIN, INPUT);
  pinMode(EQ_STROBE_PIN, OUTPUT);
  pinMode(EQ_RESET_PIN, OUTPUT);
  pinMode(NEO_PIXEL_PIN, OUTPUT);

  FastLED.addLeds<NEOPIXEL, NEO_PIXEL_PIN>(leds, N_PIXELS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(MAX_BRIGHTNESS);

  /*
  Serial.begin(9600);             // we agree to talk fast!
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }
  Serial.println("started");
  */

  digitalWrite(EQ_RESET_PIN, LOW);
  digitalWrite(EQ_STROBE_PIN, HIGH);

  setup_bins();
}

void loop(void) {
  readEq();

  for (uint8_t i = 0; i < N_PIXELS; i++) {
    setColor(i);
  }

  FastLED.show();
}

// Read the spectrum analyzer and populate the moving average globals.
void readEq(void) {
  digitalWrite(EQ_RESET_PIN, HIGH);
  digitalWrite(EQ_RESET_PIN, LOW);
  delayMicroseconds(70);

  for (uint8_t i = 0; i < NUM_FREQS; i++) {
    digitalWrite(EQ_STROBE_PIN, LOW);
    delayMicroseconds(40);

    // This will be a 10 bit value.
    uint16_t newValue = analogRead(EQ_OUTPUT_PIN);

    // Double exponential smoothing for the short period.
    smooth2(
      newValue,
      SHORT_PERIOD_DECAY_ALPHA,
      SHORT_PERIOD_DECAY_BETA,
      &shortReadMovingAverage[i],
      &shortReadMovingAverageDelta[i],
      longReadMovingAverage[i] != 0);

    // Single exponential smoothing for the long period. Not double because we actually want this to lag.
    smooth(
      newValue,
      LONG_PERIOD_DECAY_ALPHA,
      &longReadMovingAverage[i],
      longReadMovingAverage[i] != 0);

    digitalWrite(EQ_STROBE_PIN, HIGH);
    delayMicroseconds(40);
  }
}

// Set the new color value of an individial pixel.
void setColor(uint8_t i) {
  // Algorithm:
  // Take the difference between the long-period and short-period averages of each frequency band.
  // Interpolate pixel space to frequency space. If short period > long period of a given band, light up its pixels.

  int value, baseline;

  uint8_t band1, band2, percentBand2;

  // Project the 7 frequency bands onto N_PIXELS.
  band1 = BAND_WEIGHTS[i][0];
  band2 = band1 + 1;
  percentBand2 = BAND_WEIGHTS[i][1];

  // Short period value.
  value = hexWeightedMean((int32_t) shortReadMovingAverage[band1], (int32_t) shortReadMovingAverage[band2], percentBand2);

  // Long period "baseline".
  baseline = hexWeightedMean((int32_t) longReadMovingAverage[band1], (int32_t) longReadMovingAverage[band2], percentBand2);

  // Set brightness according to the long vs short distance.
  uint8_t brightness = map(max(value - baseline, 0), 0, baseline >> 1, 20, 255);

  // Hue = ring topology around the set of pixels.
  uint8_t hue = map(i, 0, N_PIXELS - 1, 0, 255);

  // Rotate hue over time.
  hue += (millis() >> 4) & 0xff;

  CRGB color = CHSV(hue, SATURATION, brightness);
  CRGB oldColor = leds[i];

  if (value <= baseline + (baseline >> THRESHOLD)) {
    // Threshold smoothing. If the difference between value and baseline is small, leave the pixel off.
    // This reduces flickering when things are quiet.
    leds[i].fadeToBlackBy(FADE_RATE);

  } else if (oldColor.getLuma() > color.getLuma()) {
    // Fade out smoothing. If the new color is darker, fade it out conservatively. This reduces flicker.
    leds[i] = blend(leds[i], color, FADE_RATE);

  } else {
    leds[i] = color;
  }
}

// Initialize the BAND_WEIGHTS array.
// Trade off some memory for clock cycles since these values never change.
// This could be rewritten as a macro.
// I was too lazy to optimize out the division and floating point ops here.
void setup_bins(void) {

  // Smooth the end buckets by averaging them with their neighbors by this amount. Without this the edges are a little erratic.
  static const float EDGE_PERCENT = .15;

  for (uint8_t i = 0; i < N_PIXELS; i++) {
    uint8_t frequencyBin1;
    float percent;
    if (i == 0) {
      frequencyBin1 = 0;
      percent = EDGE_PERCENT;

    } else if (i == N_PIXELS - 1) {
      frequencyBin1 = NUM_FREQS - 2;
      percent = 1 - EDGE_PERCENT;

    } else {
      float newBin = (i * NUM_FREQS) / ((float) (N_PIXELS - 1));
      frequencyBin1 = (int) newBin;
      percent = fmod(newBin, 1);
    }

    // The pixel at location i should be between frequencyBin1 and frequencyBin1 + 1.
    BAND_WEIGHTS[i][0] = frequencyBin1;

    // The pixel at location i should be weighted towards frequencyBin1 + 1 at this percent.
    BAND_WEIGHTS[i][1] = (uint8_t) round(percent * 255);
  }
}




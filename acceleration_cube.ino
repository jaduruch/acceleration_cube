#include <Adafruit_NeoPixel.h>
#include <math.h>

constexpr uint8_t LED_PIN = 6;
constexpr uint8_t CUBE_SIZE = 4;
constexpr uint16_t LED_COUNT = CUBE_SIZE * CUBE_SIZE * CUBE_SIZE;

constexpr uint8_t ACCEL_X_PIN = A0;
constexpr uint8_t ACCEL_Y_PIN = A1;
constexpr uint8_t ACCEL_Z_PIN = A2;

constexpr int ADC_CENTER = 512;
constexpr int ADC_RANGE = 300;

Adafruit_NeoPixel cube(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

static int constrainInt(const int value, const int minValue, const int maxValue) {
  if (value < minValue) {
    return minValue;
  }
  if (value > maxValue) {
    return maxValue;
  }
  return value;
}

static float normalizeAxis(const int reading) {
  const int centered = reading - ADC_CENTER;
  const int limited = constrainInt(centered, -ADC_RANGE, ADC_RANGE);
  return static_cast<float>(limited) / static_cast<float>(ADC_RANGE);
}

static uint8_t axisToIndex(const float value) {
  const float shifted = (value + 1.0f) * 0.5f;
  const float scaled = shifted * static_cast<float>(CUBE_SIZE - 1);
  int idx = static_cast<int>(scaled + 0.5f);
  idx = constrainInt(idx, 0, CUBE_SIZE - 1);
  return static_cast<uint8_t>(idx);
}

static uint16_t voxelIndex(const uint8_t x, const uint8_t y, const uint8_t z) {
  return static_cast<uint16_t>(x) +
         (static_cast<uint16_t>(y) * CUBE_SIZE) +
         (static_cast<uint16_t>(z) * CUBE_SIZE * CUBE_SIZE);
}

void setup() {
  cube.begin();
  cube.clear();
  cube.show();
}

void loop() {
  const float xNorm = normalizeAxis(analogRead(ACCEL_X_PIN));
  const float yNorm = normalizeAxis(analogRead(ACCEL_Y_PIN));
  const float zNorm = normalizeAxis(analogRead(ACCEL_Z_PIN));

  const uint8_t x = axisToIndex(xNorm);
  const uint8_t y = axisToIndex(yNorm);
  const uint8_t z = axisToIndex(zNorm);

  const uint8_t red = static_cast<uint8_t>(constrainInt(static_cast<int>(fabs(xNorm) * 255.0f), 0, 255));
  const uint8_t green = static_cast<uint8_t>(constrainInt(static_cast<int>(fabs(yNorm) * 255.0f), 0, 255));
  const uint8_t blue = static_cast<uint8_t>(constrainInt(static_cast<int>(fabs(zNorm) * 255.0f), 0, 255));

  cube.clear();
  cube.setPixelColor(voxelIndex(x, y, z), cube.Color(red, green, blue));
  cube.show();

  delay(50);
}

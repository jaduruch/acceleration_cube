
/************************************************
@ PROJECT: Acceleration Cube
@ AUTHOR: Jana Ruch
************************************************/

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN 17
#define LED_COUNT 30
// 20 -> ~32mA, 40 -> ~39mA, 100 -> ~58mA, 150 -> ~74mA
#define BRIGHTNESS 10

/********************************
            |     /
         z+ |    / 
            |   /
            |  /
            | /
  – – – – –   – – – – –
          / |       y+
         /  |
     x+ /   |
       /    |
      /     |


axis:   leds:
* x-    15...19
* x+    10...14
* y-    24...29
* y+    20...24
* z-    04...00
* z+    05...09
********************************/

// Acceleration settings
#define MAX_ACCEL 20.0f
#define SENSITIVITY 1.6f
#define LED_DEADZONE 1.2f
#define UPDATE_INTERVAL 50
#define GRAVITY_FILTER_ALPHA 0.90f

#define SENSOR_ROTATION_DEG 90
#define SENSOR_ROTATION_CW true
#define SENSOR_INVERT_Y true

#define COLOR_X 0x8400FF
#define COLOR_Y 0x0080FF
#define COLOR_Z 0xFF0095

// I2C pins
#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_MPU6050 mpu;

Adafruit_NeoPixel strip(
  LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800
);

// Calculate LED count with nonlinear scaling
int calculateLEDCount(float acceleration) {
  const float absAcc = fabsf(acceleration);
  if (absAcc <= LED_DEADZONE) {
    return 0;
  }

  float normalized = constrain(
    (absAcc - LED_DEADZONE) / (MAX_ACCEL - LED_DEADZONE),
    0.0f, 1.0f
  );

  float scaled = powf(normalized, SENSITIVITY);

  return constrain(
    (int)ceilf(scaled * 5.0f),
    0, 5
  );
}


void setAxisLEDs(float acceleration,
                 int negativeStart,
                 int positiveStart,
                 bool reverseNegative,
                 uint32_t color) {

  int count = calculateLEDCount(acceleration);

  if (acceleration < 0) {
    for (int i = 0; i < count; i++) {
      int led = reverseNegative
        ? negativeStart - i
        : negativeStart + i;
      strip.setPixelColor(led, color);
    }
  }
  else {
    for (int i = 0; i < count; i++) {
      int led = positiveStart + i;
      strip.setPixelColor(led, color);
    }
  }
}

void applySensorRotation(float &x, float &y, float &z) {
#if SENSOR_ROTATION_DEG == 90
  const float inX = x;
  const float inY = y;
  (void)z;

  if (SENSOR_ROTATION_CW) {
    x = inY;
    y = -inX;
  } else {
    x = -inY;
    y = inX;
  }
#endif

#if SENSOR_INVERT_Y
  y = -y;
#endif
}

void removeGravity(float &x, float &y, float &z) {
  static bool gravityInitialized = false;
  static float gravityX = 0.0f;
  static float gravityY = 0.0f;
  static float gravityZ = 0.0f;

  if (!gravityInitialized) {
    gravityX = x;
    gravityY = y;
    gravityZ = z;
    gravityInitialized = true;
  }

  gravityX = GRAVITY_FILTER_ALPHA * gravityX + (1.0f - GRAVITY_FILTER_ALPHA) * x;
  gravityY = GRAVITY_FILTER_ALPHA * gravityY + (1.0f - GRAVITY_FILTER_ALPHA) * y;
  gravityZ = GRAVITY_FILTER_ALPHA * gravityZ + (1.0f - GRAVITY_FILTER_ALPHA) * z;

  x -= gravityX;
  y -= gravityY;
  z -= gravityZ;
}



void setLEDs(float accX, float accY, float accZ) {
  strip.clear();

  const uint8_t xR = (uint8_t)((COLOR_X >> 16) & 0xFF);
  const uint8_t xG = (uint8_t)((COLOR_X >> 8) & 0xFF);
  const uint8_t xB = (uint8_t)(COLOR_X & 0xFF);
  const uint8_t yR = (uint8_t)((COLOR_Y >> 16) & 0xFF);
  const uint8_t yG = (uint8_t)((COLOR_Y >> 8) & 0xFF);
  const uint8_t yB = (uint8_t)(COLOR_Y & 0xFF);
  const uint8_t zR = (uint8_t)((COLOR_Z >> 16) & 0xFF);
  const uint8_t zG = (uint8_t)((COLOR_Z >> 8) & 0xFF);
  const uint8_t zB = (uint8_t)(COLOR_Z & 0xFF);

  const uint32_t intensity = (255U * (BRIGHTNESS + 1U)) >> 8;
  const uint32_t xColor = strip.Color((xR * intensity) / 255U,
                                      (xG * intensity) / 255U,
                                      (xB * intensity) / 255U);
  const uint32_t yColor = strip.Color((yR * intensity) / 255U,
                                      (yG * intensity) / 255U,
                                      (yB * intensity) / 255U);
  const uint32_t zColor = strip.Color((zR * intensity) / 255U,
                                      (zG * intensity) / 255U,
                                      (zB * intensity) / 255U);

  setAxisLEDs(-accX, 15, 10, false, xColor);

  setAxisLEDs(accY, 25, 20, false, yColor);

  setAxisLEDs(accZ, 4, 5, true, zColor);

  strip.show();
}



void setup() {
  Serial.begin(115200);

  // Initialize LEDs
  strip.begin();
  strip.setBrightness(BRIGHTNESS);
  strip.clear();
  strip.show();

  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("MPU6050 not found!");
    while (1) {
      delay(100);
    }
  }

  Serial.println("MPU6050 connected!");

  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);   
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float accX = a.acceleration.x;
  float accY = a.acceleration.y;
  float accZ = a.acceleration.z;

  applySensorRotation(accX, accY, accZ);
  removeGravity(accX, accY, accZ);

  setLEDs(accX, accY, accZ);

  // Debug output
  
  Serial.print("X: ");
  Serial.print(accX, 2);
  Serial.print(" | Y: ");
  Serial.print(accY, 2);
  Serial.print(" | Z: ");
  Serial.println(accZ, 2);
  

  delay(UPDATE_INTERVAL);
}

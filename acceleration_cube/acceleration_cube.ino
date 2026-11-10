
/************************************************
@ PROJECT: Acceleration Cube
@ AUTHOR: Jana Ruch
************************************************/

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN 14
#define LED_COUNT 30
#define BRIGHTNESS 150  // Reduce for real application

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
* x-    25...29
* x+    20...24
* y-    15...19
* y+    10...14
* z-    05...09
* z+    04...00
********************************/

// Acceleration settings
#define MAX_ACCEL 10.0f
#define SENSITIVITY 0.7f
#define UPDATE_INTERVAL 50

// SENSITIVITY:
// 1.0 = Linear
// 0.7 = Slightly enhanced (recommended)
// 0.5 = Square root
// 0.3 = Very sensitive

// I2C pins
#define SDA_PIN 21
#define SCL_PIN 23

Adafruit_MPU6050 mpu;

Adafruit_NeoPixel strip(
  LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800
);

// Calculate LED count with nonlinear scaling
int calculateLEDCount(float acceleration) {
  float normalized = constrain(
    fabsf(acceleration) / MAX_ACCEL,
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
                 bool reversePositive,
                 bool invertColors = false) {

  int count = calculateLEDCount(acceleration);

  uint32_t red = strip.Color(255, 0, 0);
  uint32_t green = strip.Color(0, 255, 0);

  if (acceleration < 0) {
    for (int i = 0; i < count; i++) {
      strip.setPixelColor(
        negativeStart + i,
        invertColors ? green : red
      );
    }
  }
  else if (acceleration > 0) {
    for (int i = 0; i < count; i++) {
      int led = reversePositive
        ? positiveStart + i
        : positiveStart - i;

      strip.setPixelColor(
        led,
        invertColors ? red : green
      );
    }
  }
}

void setLEDs(float accX, float accY, float accZ) {
  strip.clear();

  setAxisLEDs(-accX, 25, 20, true, true);
  setAxisLEDs( accY, 15, 10, true);
  setAxisLEDs( accZ,  5,  4, false);

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

  setLEDs(accX, accY, accZ);

  // Debug output
  /*
  Serial.print("X: ");
  Serial.print(accX, 2);
  Serial.print(" | Y: ");
  Serial.print(accY, 2);
  Serial.print(" | Z: ");
  Serial.println(accZ, 2);
  */

  delay(UPDATE_INTERVAL);
}

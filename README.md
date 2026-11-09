# acceleration_cube

LED cube to display acceleration in x-, y- and z-direction with addressable LEDs.

## Hardware assumptions

- Microcontroller with analog inputs (e.g. Arduino compatible board)
- 3-axis analog accelerometer output connected to:
  - `A0` -> X
  - `A1` -> Y
  - `A2` -> Z
- Addressable LED cube based on WS2812-compatible LEDs

## Arduino dependencies

- `Adafruit NeoPixel` library

## Upload

1. Open `/home/runner/work/acceleration_cube/acceleration_cube/acceleration_cube.ino` in Arduino IDE.
2. Adjust pin numbers and cube size if needed.
3. Install the `Adafruit NeoPixel` library.
4. Build and upload.

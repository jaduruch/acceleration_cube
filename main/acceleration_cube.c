/* Acceleration Cube - Jana Ruch; native ESP-IDF port. */
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

#define LED_PIN 17
#define LED_COUNT 30
#define BRIGHTNESS 10
#define SDA_PIN 21
#define SCL_PIN 22
#define MPU_ADDRESS 0x68
#define MAX_ACCEL 20.0f
#define SENSITIVITY 1.6f
#define LED_DEADZONE 1.2f
#define UPDATE_INTERVAL_MS 50
#define I2C_TIMEOUT_MS 100
#define GRAVITY_FILTER_ALPHA 0.90f
#define SENSOR_ROTATION_DEG 90
#define SENSOR_ROTATION_CW true
#define SENSOR_INVERT_Y true
#define COLOR_X 0x8400FF
#define COLOR_Y 0x0080FF
#define COLOR_Z 0xFF0095

static const char *TAG = "acceleration_cube";
static i2c_master_dev_handle_t mpu;
static led_strip_handle_t strip;

static esp_err_t read_registers(uint8_t reg, uint8_t *data, size_t size)
{
    return i2c_master_transmit_receive(mpu, &reg, 1, data, size, I2C_TIMEOUT_MS);
}

static esp_err_t write_register(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(mpu, data, sizeof(data), I2C_TIMEOUT_MS);
}

static esp_err_t init_mpu(void)
{
    i2c_master_bus_handle_t bus;
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus);
    if (err != ESP_OK) return err;
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU_ADDRESS,
        .scl_speed_hz = 100000,
    };
    err = i2c_master_bus_add_device(bus, &device_config, &mpu);
    if (err != ESP_OK) return err;
    uint8_t identity;
    err = read_registers(0x75, &identity, 1); // WHO_AM_I
    if (err != ESP_OK) return err;
    if (identity != 0x68) return ESP_ERR_INVALID_RESPONSE;

    err = write_register(0x6B, 0x80); // Device reset
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));
    err = write_register(0x6B, 0x01); // Wake, PLL with X gyro reference
    if (err != ESP_OK) return err;
    err = write_register(0x6C, 0x00); // Enable all axes
    if (err != ESP_OK) return err;
    err = write_register(0x1C, 0x00); // Accelerometer +/-2 g
    if (err != ESP_OK) return err;
    err = write_register(0x1B, 0x08); // Gyroscope +/-500 degrees/s
    if (err != ESP_OK) return err;
    err = write_register(0x1A, 0x04); // DLPF: 21 Hz accelerometer
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(100));
    return ESP_OK;
}

static int calculate_led_count(float acceleration)
{
    const float abs_acc = fabsf(acceleration);
    if (abs_acc <= LED_DEADZONE) {
        return 0;
    }
    float normalized = fminf((abs_acc - LED_DEADZONE) / (MAX_ACCEL - LED_DEADZONE), 1.0f);
    int count = (int)ceilf(powf(normalized, SENSITIVITY) * 5.0f);
    return count > 5 ? 5 : count;
}

static void set_axis_leds(float acceleration, int negative_start,
                          int positive_start, bool reverse_negative,
                          uint8_t color_r, uint8_t color_g, uint8_t color_b)
{
    const int count = calculate_led_count(acceleration);
    const uint32_t intensity = (255U * (BRIGHTNESS + 1U)) >> 8;
    const uint8_t scaled_r = (uint8_t)((color_r * intensity) / 255U);
    const uint8_t scaled_g = (uint8_t)((color_g * intensity) / 255U);
    const uint8_t scaled_b = (uint8_t)((color_b * intensity) / 255U);
    for (int i = 0; i < count; ++i) {
        int led = acceleration < 0
            ? negative_start + (reverse_negative ? -i : i)
            : positive_start + i;
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, led, scaled_r, scaled_g, scaled_b));
    }
}

static void set_leds(float x, float y, float z)
{
    // Clear the buffer without transmitting an intermediate dark frame.
    for (int i = 0; i < LED_COUNT; ++i) {
        ESP_ERROR_CHECK(led_strip_set_pixel(strip, i, 0, 0, 0));
    }
    set_axis_leds(-x, 15, 10, false,
                  (uint8_t)((COLOR_X >> 16) & 0xFF),
                  (uint8_t)((COLOR_X >> 8) & 0xFF),
                  (uint8_t)(COLOR_X & 0xFF));
    set_axis_leds(y, 25, 20, false,
                  (uint8_t)((COLOR_Y >> 16) & 0xFF),
                  (uint8_t)((COLOR_Y >> 8) & 0xFF),
                  (uint8_t)(COLOR_Y & 0xFF));
    set_axis_leds(z, 4, 5, true,
                  (uint8_t)((COLOR_Z >> 16) & 0xFF),
                  (uint8_t)((COLOR_Z >> 8) & 0xFF),
                  (uint8_t)(COLOR_Z & 0xFF));
    ESP_ERROR_CHECK(led_strip_refresh(strip));
}

static int16_t decode_axis(const uint8_t *data)
{
    uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
    return (int16_t)raw;
}

static void apply_sensor_rotation(float *x, float *y, float *z)
{
#if SENSOR_ROTATION_DEG == 90
    const float in_x = *x;
    const float in_y = *y;
    (void)z;
    if (SENSOR_ROTATION_CW) {
        // 90 degree clockwise rotation around Z axis.
        *x = in_y;
        *y = -in_x;
    } else {
        // 90 degree counter-clockwise rotation around Z axis.
        *x = -in_y;
        *y = in_x;
    }
#endif
#if SENSOR_INVERT_Y
    *y = -*y;
#endif
}

static void remove_gravity(float *x, float *y, float *z)
{
    static bool gravity_initialized = false;
    static float gravity_x = 0.0f;
    static float gravity_y = 0.0f;
    static float gravity_z = 0.0f;

    if (!gravity_initialized) {
        gravity_x = *x;
        gravity_y = *y;
        gravity_z = *z;
        gravity_initialized = true;
    }

    gravity_x = GRAVITY_FILTER_ALPHA * gravity_x + (1.0f - GRAVITY_FILTER_ALPHA) * *x;
    gravity_y = GRAVITY_FILTER_ALPHA * gravity_y + (1.0f - GRAVITY_FILTER_ALPHA) * *y;
    gravity_z = GRAVITY_FILTER_ALPHA * gravity_z + (1.0f - GRAVITY_FILTER_ALPHA) * *z;

    *x -= gravity_x;
    *y -= gravity_y;
    *z -= gravity_z;
}

void app_main(void)
{
    const led_strip_config_t strip_config = {
        .strip_gpio_num = LED_PIN,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    const led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &strip));
    ESP_ERROR_CHECK(led_strip_clear(strip));
    esp_err_t err = init_mpu();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 initialization failed: %s; check wiring and address 0x%02x",
                 esp_err_to_name(err), MPU_ADDRESS);
        return;
    }
    ESP_LOGI(TAG, "MPU6050 connected");
    while (true) {
        uint8_t data[6];
        err = read_registers(0x3B, data, sizeof(data));
        if (err == ESP_OK) {
            // +/-2 g: 16384 LSB/g.
            const float scale = 9.80665f / 16384.0f;
            float x = decode_axis(data) * scale;
            float y = decode_axis(data + 2) * scale;
            float z = decode_axis(data + 4) * scale;
            apply_sensor_rotation(&x, &y, &z);
            remove_gravity(&x, &y, &z);
            set_leds(x, y, z);
            ESP_LOGI(TAG, "X: %.2f | Y: %.2f | Z: %.2f", x, y, z);
        } else {
            ESP_LOGW(TAG, "MPU6050 read failed: %s", esp_err_to_name(err));
            ESP_ERROR_CHECK(led_strip_clear(strip));
        }
        vTaskDelay(pdMS_TO_TICKS(UPDATE_INTERVAL_MS));
    }
}

#include "led_indicator.h"


static const char *TAG_INDICATOR = "WS2812";

led_strip_handle_t led_strip;
float brightness = 0.1;

esp_err_t indicator_init() { 
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,
        .max_leds = LED_STRIP_LED_NUM, 
    };
 
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_STRIP_RMT_RES_HZ,
        .flags.with_dma = false,
    };
 
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip)); 
    ESP_LOGI(TAG_INDICATOR, "WS2812 Initialized on GPIO 21");
    indicator_red();
    return ESP_OK;
}

esp_err_t indicator_deep_orange() {
    uint8_t red   = 50;
    uint8_t green = 10;
    esp_err_t err; 
    err = led_strip_set_pixel(led_strip, 0, red, green, 0);
    if (err != ESP_OK) {
        return err;
    }
    return led_strip_refresh(led_strip);
}

esp_err_t indicator_red() {
    esp_err_t err;
    uint8_t red  = (uint8_t)(255 * brightness);
    err = led_strip_set_pixel(led_strip, 0, red, 0, 0);
    if (err != ESP_OK) {
        return err;
    }
    return led_strip_refresh(led_strip);
}
esp_err_t indicator_green() {
    esp_err_t err;
    uint8_t green = (uint8_t)(255 * brightness);
    err = led_strip_set_pixel(led_strip, 0, 0, green, 0);
    if (err != ESP_OK) {
        return err;
    }
    return led_strip_refresh(led_strip);
}
esp_err_t indicator_blue(){
    esp_err_t err;
    uint8_t blue  = (uint8_t)(255 * brightness);
    err = led_strip_set_pixel(led_strip, 0, 0, 0, blue);
    if (err != ESP_OK) {
        return err;
    }
    return led_strip_refresh(led_strip);
}
esp_err_t indicator_rgb(int r, int g, int b){
    esp_err_t err;
    uint8_t red   = (uint8_t)(r * brightness);
    uint8_t green = (uint8_t)(g * brightness);
    uint8_t blue  = (uint8_t)(b * brightness);
    err = led_strip_set_pixel(led_strip, 0, red, green, blue);
    if (err != ESP_OK) {
        return err;
    }
    return led_strip_refresh(led_strip);
}

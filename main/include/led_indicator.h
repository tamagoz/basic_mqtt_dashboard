#ifndef _LED_INDICATOR_H_
#define _LED_INDICATOR_H_ 

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"

#define LED_STRIP_BLINK_GPIO  21
#define LED_STRIP_LED_NUM     1    
#define LED_STRIP_RMT_RES_HZ  10 * 1000 * 1000  


esp_err_t indicator_init();
esp_err_t indicator_red();
esp_err_t indicator_green();
esp_err_t indicator_blue();
esp_err_t indicator_deep_orange();
esp_err_t indicator_rgb(int r, int g, int b);

#endif
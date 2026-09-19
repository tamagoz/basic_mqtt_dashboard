#ifndef _NETWORK_H_
#define _NETWORK_H_ 

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_system.h" 
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h" 
#include "lwip/err.h"
#include "lwip/sys.h" 
#ifdef CONFIG_USE_WIFI
#include "esp_wifi.h"
#endif
#ifdef CONFIG_USE_ETHERNET
#include "esp_eth_phy_w5500.h"
#include "esp_eth_mac_w5500.h"
#endif
#include "esp_mac.h"
#include "lwip/ip4_addr.h"

#ifdef CONFIG_USE_WIFI
#define EXAMPLE_ESP_MAXIMUM_RETRY 10
#endif


esp_err_t connect_network();

#endif
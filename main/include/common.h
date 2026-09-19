#ifndef _COMMON_H_
#define _COMMON_H_ 
#include <stdio.h> 
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <esp_console.h>
#include <esp_log.h>
//
#include <ping/ping_sock.h>
#include <lwip/sockets.h> 
#include <lwip/netdb.h>
// wifi
#include <ping/ping_sock.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <nvs_flash.h>
// heap
#include <esp_heap_caps.h>
#include <esp_timer.h>
// i2c driver
#include <driver/i2c_master.h>
#include <driver/temperature_sensor.h>

#include "led_indicator.h"
#include "network.h" 
#include "user_event.h"
#include "file_system.h"
#include "board_io.h"


#define I2C_MASTER_SCL_IO           CONFIG_I2C_MASTER_SCL
#define I2C_MASTER_SDA_IO           CONFIG_I2C_MASTER_SDA
#define I2C_MASTER_NUM              I2C_NUM_0 

#define LOW                         0
#define HIGH                        1


bool is_number(const char *str);


#endif
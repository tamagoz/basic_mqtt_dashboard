#ifndef _FILE_SYSTEM_H_
#define _FILE_SYSTEM_H_ 
#include <stdio.h>
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_mmu_map.h"
#include "esp_idf_version.h"
#include "esp_chip_info.h"
#include "esp_littlefs.h"
#include "nvs_flash.h"          //non volatile storage
#include "esp_log.h"
#include <unistd.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "cJSON.h"

esp_err_t fila_system_init(); 
esp_err_t fila_system_mounted();
esp_err_t remove_file(char * file_name) ;
bool file_exists(const char *path) ;



esp_err_t get_wifi_user_password(char* ssid, char* pass);
esp_err_t save_wifi_config(char* ssid, char* password) ;
esp_err_t remove_wifi_config();
esp_err_t file_config_exists();

#endif
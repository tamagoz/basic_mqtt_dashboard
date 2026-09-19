#include "file_system.h"

static const char *FILE_CONFIG_WIFI = "/littlefs/wifi.json";
static const char *TAG_FILE_SYSTEM = "file_system";
const char *partition_label = "littlefs";
const char *path = "/littlefs";


esp_err_t fila_system_init() {  
    ESP_LOGI(TAG_FILE_SYSTEM, "Initializing LittleFS");
    esp_vfs_littlefs_conf_t conf = {
        .base_path = path,
        .partition_label = partition_label,
        .format_if_mount_failed = true,
        .dont_mount = false,
    };
 
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG_FILE_SYSTEM, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG_FILE_SYSTEM, "Failed to find LittleFS partition");
        }
        else
        {
            ESP_LOGE(TAG_FILE_SYSTEM, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG_FILE_SYSTEM, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG_FILE_SYSTEM, "Partition size: total: %d, used: %d", total, used);
    }
    return ret;
}

esp_err_t fila_system_mounted() {
    if (esp_littlefs_mounted("storage")) {  
        ESP_LOGI(TAG_FILE_SYSTEM, "LittleFS is already mounted.");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG_FILE_SYSTEM, "LittleFS is NOT mounted.");
    }
    return ESP_ERR_INVALID_STATE;
}

esp_err_t remove_file(char * file_name) {
  struct stat st;
  esp_err_t err;
  if (stat(file_name, &st) == 0)  {
    // Delete it if it exists
    unlink(file_name);
    err = ESP_OK;
  } else {
    err = ESP_FAIL;
  }
  return err;
}

esp_err_t get_wifi_user_password(char* ssid, char* pass){
  ESP_LOGI(TAG_FILE_SYSTEM, "Reading file");
  FILE *f = fopen(FILE_CONFIG_WIFI, "r");
  if (f == NULL) {
    ESP_LOGE(TAG_FILE_SYSTEM, "Failed to open file for reading");
    return ESP_FAIL;
  }
  char* wifi_json = (char *)malloc(512);
  // char wifi_json[512];
  fgets(wifi_json, 512, f);
  fclose(f);
  ESP_LOGI(TAG_FILE_SYSTEM, "Read from file: '%s'", wifi_json);

  ESP_LOGI(TAG_FILE_SYSTEM, "Deserialize.....");
  cJSON *root = cJSON_Parse(wifi_json);
  if (cJSON_GetObjectItem(root, "ssid")) {
    char *value = cJSON_GetObjectItem(root, "ssid")->valuestring;
    strcpy(ssid, value);
    ESP_LOGI(TAG_FILE_SYSTEM, "ssid=%s", ssid);
  } else {
    return ESP_FAIL;
  }
  if (cJSON_GetObjectItem(root, "password")) {
    char *value = cJSON_GetObjectItem(root, "password")->valuestring;
    strcpy(pass, value);
    ESP_LOGI(TAG_FILE_SYSTEM, "password=%s", pass);
  }else {
    return ESP_FAIL;
  } 
  cJSON_Delete(root); 

  return ESP_OK;
}

esp_err_t save_wifi_config(char* ssid, char* password) {
  esp_err_t err = ESP_OK;
  cJSON *root = cJSON_CreateObject(); 
  cJSON_AddStringToObject(root, "ssid", ssid);
  cJSON_AddStringToObject(root, "password", password);
  char *payload = cJSON_PrintUnformatted(root);
  ESP_LOGI(TAG_FILE_SYSTEM, "payload: %s",payload); 
  //
  ESP_LOGI(TAG_FILE_SYSTEM, "Opening file");
  FILE *f = fopen(FILE_CONFIG_WIFI, "w"); 
  ESP_LOGI(TAG_FILE_SYSTEM, "Opened file");
  if (f == NULL) {
    ESP_LOGE(TAG_FILE_SYSTEM, "Failed to open file for writing");
    err =  ESP_FAIL; 
  } 
  ESP_LOGI(TAG_FILE_SYSTEM, "Begin write file");
  fprintf(f, payload);
  ESP_LOGI(TAG_FILE_SYSTEM, "Write file success");
  fclose(f);
  ESP_LOGI(TAG_FILE_SYSTEM, "File written"); 
  cJSON_Delete(root);

  return err;
}

bool file_exists(const char *path) {
    struct stat st; 
    if (stat(path, &st) == 0) {
        return true;
    }
    return false;
}

esp_err_t remove_wifi_config() {
    return remove_file((char * )FILE_CONFIG_WIFI);
}


esp_err_t file_config_exists() {
    if (file_exists(FILE_CONFIG_WIFI)) {
        return ESP_OK;
    } 
    return ESP_ERR_NOT_FOUND;
}
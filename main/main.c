#include "common.h"
#include "led_indicator.h"
// network
#include "esp_mac.h"
#include "lwip/ip4_addr.h"
#include "esp_eth_phy_w5500.h"
#include "esp_eth_mac_w5500.h"
// mqtt
#include "mqtt_client.h"
#include "cJSON.h"
// sensor
#include <sht3x.h>

// sntp
#include <time.h>
#include <sys/time.h>
#include <esp_sntp.h>

#include <ssd1306.h>

#define MQTT_MAX_RECONNECT  10

#define MQTT_URL "mqtt://broker.emqx.io"

#define ETH_SPI_SCK_GPIO 13
#define ETH_SPI_MISO_GPIO 12
#define ETH_SPI_MOSI_GPIO 11
#define ETH_SPI_CS_GPIO 8
#define ETH_SPI_INT_GPIO 10
#define ETH_SPI_RST_GPIO 9
#define TAG "MAIN"
#define V "1.0.0"

int s_retry_num = 0;
int mqtt_reconnect = 0;
static esp_mqtt_client_handle_t client = NULL;
bool mqtt_client_connected = false;
bool mqtt_client_started = false;
char *serial_number = NULL;
static sht3x_t sht3x_dev; 
ssd1306_handle_t disp;

SemaphoreHandle_t sem_mqtt; 

float temperature = 0.0;
float humidity = 0.0;
bool ssd1306_found = false;

// topic
char check_topic[64] = {0}; 
char will_topic[64] = {0}; 
char *will_msg;
char out_topic[64] = {0}; 
char in_topic[64] = {0}; 
char in_result_topic[64] = {0}; 
char get_topic[64] = {0}; 
char getall_topic[64] = {0}; 

void mqtt_app_start(void);


static inline void check_boot_reason(void) { 
    esp_reset_reason_t reason = esp_reset_reason();

    printf("\n==================================================\n");
    printf("🔍 [BOOT REASON ANALYSIS]\n");

    switch (reason) {
        // เคสที่ 1: เปิดก๊อกจ่ายไฟเข้าตู้ครั้งแรก (Power On / Cold Boot)
        case ESP_RST_POWERON:
            printf("🔌 สถานะ: [POWER ON] เปิดสวิตช์จ่ายไฟเข้าบอร์ดครั้งแรก\n");
            printf("ลอจิกหน้างาน: ควรเริ่มรันระบบซอฟต์แวร์ปลุกโมเด็ม 4G จากศูนย์\n");
            break;

        // เคสที่ 2: กดปุ่ม EN หรือสั่งเอาสาย RST จัมพ์ลง GND (Hardware Reset)
        case ESP_RST_EXT:
            printf("สถานะ: [EXTERNAL RESET] มีคนมากดปุ่มรีเซ็ตบนบอร์ดหน้าตู้\n");
            break;

        // เคสที่ 3: สั่งซอฟต์แวร์รีบูตตัวเอง (เช่น คำสั่ง esp_restart() ในโค้ด)
        case ESP_RST_SW:
            printf("สถานะ: [SOFTWARE REBOOT] โค้ดสั่งสั่งรีสตาร์ทระบบตัวเองอัตโนมัติ\n");
            break;

        // เคสที่ 4: บั๊กหมาเฝ้าบ้าน (Watchdog Timer ตะปบเพราะโค้ดติดลูปค้างชั่วนิรันดร์)
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
            printf("สถานะ: [WATCHDOG RESET] โค้ดค้าง! ระบบโดนหมาเฝ้าบ้านกัดให้รีบูตด่วน\n");
            printf("ลอจิกหน้างาน: ควรเช็คว่ามี Task ไหนลาก Loop นานเกินไปไหม\n");
            break;

        // ⚡ เคสที่ 5: ไฟเลี้ยงตกชั่วขณะ (Brownout Reset)
        case ESP_RST_BROWNOUT:
            printf("⚡ สถานะ: [BROWNOUT RESET] ไฟเลี้ยงกระชากตกวูบจนบอร์ดวับดับไป!\n");
            printf("ลอจิกหน้างาน: ต้องเช็คภาคจ่ายไฟในตู้จ่ายน้ำมัน หรือตอนโมเด็ม 4G ดึงแอมป์ด่วนครับพี่\n");
            break;

        // เคสอื่น ๆ (เช่น แครชพังหลุดโค้ง Panic)
        case ESP_RST_PANIC:
            printf("สถานะ: [PANIC RESET] โค้ด C++ หรือสแต็กแรมระเบิดกลางอากาศจนบอร์ดดับรีบูต\n");
            break;

        default:
            printf("สถานะ: ไม่ทราบสาเหตุการรีบูตแน่ชัด (%d)\n", reason);
            break;
    }
    printf("==================================================\n\n");
}


// ฟังก์ชัน Callback เมื่อเวลาได้รับอัปเดต
void time_sync_notification_cb(struct timeval *tv) {
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

unsigned long   millis() {
    return (unsigned long) (esp_timer_get_time() / 1000ULL);
}

static bool get_local_time(struct tm * info, uint32_t ms) {
    uint32_t start = millis();
    time_t now;
    while((millis()-start) <= ms) {
        time(&now);
        localtime_r(&now, info);
        if(info->tm_year > (2016 - 1900)){
            return true;
        }
       vTaskDelay(10 / portTICK_PERIOD_MS) ;
    }
    return false;
}

void init_sntp_system(void *pvParameters) {
    ESP_LOGI(TAG, "Initializing SNTP"); 
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL); 
    esp_sntp_setservername(0, "203.185.67.115");
    esp_sntp_setservername(1, "202.12.97.45");
    esp_sntp_setservername(2, "216.239.35.0");  
    sntp_set_time_sync_notification_cb(time_sync_notification_cb); 
    esp_sntp_init();
    // time zone
    setenv("TZ", "ICT-7", 1);
    tzset(); 
    int retry = 0;
    const int retry_count = 15;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
 
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
    while(true) {
        get_local_time(&timeinfo, 5000U);
        ESP_LOGI(TAG, "%04d-%02d-%02d %02d:%02d:%02d", timeinfo.tm_year + 1900 , timeinfo.tm_mon + 1,
            timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelete(NULL);
}

static void init_serial_number() {
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    serial_number = (char *)malloc(32);
    bzero(serial_number, 32);
    sprintf(serial_number, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);  
    ESP_LOGI("SERIAL_NUMBER", "Serial Number: %s", serial_number); 

    // check_topic = "device/" + serial_number + "/checkin";

    sprintf(check_topic, "device/%s/checkin", serial_number);
    sprintf(will_topic, "device/%s/will", serial_number);
    sprintf(out_topic, "device/%s/out", serial_number);
    sprintf(in_topic, "device/%s/in", serial_number);
    sprintf(in_result_topic, "device/%s/in/result", serial_number);
    sprintf(get_topic, "device/%s/get", serial_number);
    sprintf(getall_topic, "device/all/get");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", serial_number);
    cJSON_AddStringToObject(root, "msg", "device  offline");
    will_msg = cJSON_PrintUnformatted(root); 
    cJSON_Delete(root);
}

static void notify() {
    if (temperature == 0 && humidity == 0) {
        return;
    }
    
    char temp_str[16];
    char humi_str[16];
    sprintf(temp_str, "%.1f", temperature); // "25.1",  25.12345688888888"
    sprintf(humi_str, "%.1f", humidity);

    if (ssd1306_found) {
        char temp_data_str[64] = {0};
        char humi_data_str[64] = {0};
        sprintf(temp_data_str, "Temp: %sC", temp_str);
        sprintf(humi_data_str, "Temp: %s%%", humi_str);
        ssd1306_clear(disp);
        ssd1306_draw_text_scaled(disp, 15, 1, "DASHBOARD", true, 2);
        ssd1306_draw_text(disp, 30, 25, temp_data_str, true);
        ssd1306_draw_text(disp, 30, 40, humi_data_str, true);
        ssd1306_display(disp);
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "device_id", serial_number);
    cJSON_AddRawToObject(root, "temperature", temp_str);
    cJSON_AddRawToObject(root, "humidity", humi_str);

    cJSON_AddBoolToObject(root, "y01", !pcf_digital_read(0));
    cJSON_AddBoolToObject(root, "y02", !pcf_digital_read(1));
    cJSON_AddBoolToObject(root, "y03", !pcf_digital_read(2));
    cJSON_AddBoolToObject(root, "y04", !pcf_digital_read(3));
    cJSON_AddBoolToObject(root, "y05", !pcf_digital_read(4));

    cJSON_AddNumberToObject(root, "free_heap", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_AddNumberToObject(root, "spi_ram", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    char *payload = cJSON_PrintUnformatted(root);
    ESP_LOGI("MQTT", "payload: %s", payload);
    if (mqtt_client_connected) {
        esp_mqtt_client_publish(client, out_topic, payload, 0, 1, 0);
    }
    cJSON_free(payload);
    cJSON_Delete(root);
}

void in_callback(const char * payload) {
    cJSON *root = cJSON_Parse(payload);
    if (cJSON_HasObjectItem(root, "y01")) {
        cJSON *node = cJSON_GetObjectItem(root, "y01"); 
        pcf_digital_write(0, !node->valueint);  

        cJSON *result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "y01", !pcf_digital_read(0));
        char *y01_result = cJSON_PrintUnformatted(result);
        esp_mqtt_client_publish(client, in_result_topic, y01_result, 0, 1, 0);
        cJSON_free(y01_result);
        cJSON_Delete(result);

    }
    if (cJSON_HasObjectItem(root, "y02")) {
        cJSON *node = cJSON_GetObjectItem(root, "y02"); 
        pcf_digital_write(1, !node->valueint);

        cJSON *result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "y02", !pcf_digital_read(1));
        char *y02_result = cJSON_PrintUnformatted(result);
        esp_mqtt_client_publish(client, in_result_topic, y02_result, 0, 2, 0);
        cJSON_free(y02_result);
        cJSON_Delete(result);
    }
    if (cJSON_HasObjectItem(root, "y03")) {
        cJSON *node = cJSON_GetObjectItem(root, "y03"); 
        pcf_digital_write(2, !node->valueint);

        cJSON *result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "y03", !pcf_digital_read(2));
        char *y03_result = cJSON_PrintUnformatted(result);
        esp_mqtt_client_publish(client, in_result_topic, y03_result, 0, 2, 0);
        cJSON_free(y03_result);
        cJSON_Delete(result);
    }
    if (cJSON_HasObjectItem(root, "y04")) {
        cJSON *node = cJSON_GetObjectItem(root, "y04"); 
        pcf_digital_write(3, !node->valueint);

        cJSON *result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "y04", !pcf_digital_read(3));
        char *y03_result = cJSON_PrintUnformatted(result);
        esp_mqtt_client_publish(client, in_result_topic, y03_result, 0, 2, 0);
        cJSON_free(y03_result);
        cJSON_Delete(result);
    }
    if (cJSON_HasObjectItem(root, "y05")) {
        cJSON *node = cJSON_GetObjectItem(root, "y05"); 
        pcf_digital_write(4, !node->valueint);

        cJSON *result = cJSON_CreateObject();
        cJSON_AddBoolToObject(result, "y05", !pcf_digital_read(4));
        char *y03_result = cJSON_PrintUnformatted(result);
        esp_mqtt_client_publish(client, in_result_topic, y03_result, 0, 2, 0);
        cJSON_free(y03_result);
        cJSON_Delete(result);
    }
    cJSON_Delete(root);   
}

void task_sht3x(void *pvParameters)
{ 
    TickType_t last_wakeup = xTaskGetTickCount();

    while (true) {
        // perform one measurement and do something with the results
        ESP_ERROR_CHECK(sht3x_measure(&sht3x_dev, &temperature, &humidity));
        // temperature = 25.0 + ((float)esp_random() / (float)UINT32_MAX) * 10.0;
        // humidity = 40.0 + ((float)esp_random() / (float)UINT32_MAX) * 40.0;
        notify(); 
        printf("SHT3x Sensor: %.2f °C, %.2f %%\n", temperature, humidity);
        // wait until 5 seconds are over
        vTaskDelayUntil(&last_wakeup, pdMS_TO_TICKS(10000));
    }
}

static void network_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
#if defined(CONFIG_USE_ETHERNET)
    if (event_base == ETH_EVENT) {
        switch (event_id)
        {
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Up");
            indicator_green();
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Ethernet Link Down");
            indicator_red();
            break;
        default:
            break;
        }
    } 
#elif defined(CONFIG_USE_WIFI)   
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "Wifi station start");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                indicator_red();
                if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(TAG, "retry to connect to the AP");
                }
                else {
                    s_retry_num = 0; 
                    esp_restart();
                }
                ESP_LOGI(TAG, "connect to the AP fail"); 
                break;
            case WIFI_EVENT_STA_CONNECTED:
                indicator_green();
                ESP_LOGI(TAG, "connect to the AP success"); 
                break;
        }
    }
#else
    #error "Default network interface not found"
#endif
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Ethernet Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip)); 
    if (!mqtt_client_started) { 
        xTaskCreatePinnedToCore(init_sntp_system, "init_sntp_system", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, APP_CPU_NUM); 
        mqtt_client_started = true;
        mqtt_app_start();
    } 
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    ESP_LOGI("MQTT", "Event: %d", event_id);
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_ERROR:
        ESP_LOGI("MQTT", "MQTT_EVENT_ERROR!");
        mqtt_reconnect++;
        if (mqtt_reconnect > MQTT_MAX_RECONNECT) {
            esp_restart();
        }

        break;
    case MQTT_EVENT_DISCONNECTED:
        
        mqtt_client_connected = false;
        if (ssd1306_found) {
            ssd1306_clear(disp);
            ssd1306_draw_text_scaled(disp, 0, 20, "Disconnected", true, 2);
            ssd1306_display(disp);
        }
        break;
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI("MQTT", "Connected to Broker!");
        mqtt_client_connected = true;
        if (ssd1306_found) {
            ssd1306_clear(disp);
            ssd1306_draw_text_scaled(disp, 15, 20, "Connected", true, 2);
            ssd1306_display(disp);
        }
        // ส่งข้อความแรกทักทาย Server
        esp_mqtt_client_subscribe(client, in_topic, 0);
        esp_mqtt_client_subscribe(client, get_topic, 0);
        // esp_mqtt_client_subscribe(client, getall_topic, 0);
        // 
        cJSON *root = cJSON_CreateObject(); 
        cJSON_AddStringToObject(root, "device_id", serial_number);
        cJSON_AddStringToObject(root, "v", V); 
        char *payload = cJSON_PrintUnformatted(root);
        ESP_LOGI("MQTT", "checkin payload: %s",payload); 
        esp_mqtt_client_publish(client, check_topic, payload, 0, 1, 0);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI("MQTT", "Received Topic: %.*s", event->topic_len, event->topic);
        ESP_LOGI("MQTT", "Received Data: %.*s", event->data_len, event->data);
        if (strstr(event->topic, get_topic)) { 
            if (xSemaphoreTake(sem_mqtt, 2000)) {
                notify();
                xSemaphoreGive(sem_mqtt);
            }
            
            ESP_LOGI("MQTT", "notify get data");
        }
        if (strstr(event->topic, in_topic)) {
            if (xSemaphoreTake(sem_mqtt, 2000)) {
                in_callback(event->data); 
                xSemaphoreGive(sem_mqtt);
            }
            
            ESP_LOGI("MQTT", "in message for control gpi");
        }
        break;
    default:
        break;
    }
}

void mqtt_app_start(void) {
    size_t int_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t psram_before = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .verification.use_global_ca_store = false,
            .address = {
                .uri = MQTT_URL,
                .port = 1883,
            },
        },
        .credentials = {
            .authentication = {
                .password = NULL,
            },
            .client_id = serial_number
        },
        .session = {
            .keepalive = 20,
            .last_will = {
                .topic =will_topic,
                .msg = will_msg,
                .qos = 1,
                .retain = false
            }
        },
        .buffer = {.out_size = 1024 * 4, .size = 1024 * 4}};

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    size_t int_after = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t psram_after = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    // 4. แสดงผลต่าง
    ESP_LOGW("TEST", "Internal RAM used: %d bytes", (int_before - int_after));
    ESP_LOGW("TEST", "PSRAM used:        %d bytes", (psram_before - psram_after));
}


#if defined(CONFIG_USE_WIFI)
void wifi_init() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &network_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &got_ip_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        // .sta = {
        //     .ssid = "Tenda_8CDB40",
        //     .password = "0863219053", 
        //     .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        //     .sae_pwe_h2e = WPA3_SAE_PWE_BOTH 
        // },
        .sta = {
            .ssid = "Test_Gateway",
            .password = "123456789", 
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH 
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config)); 
    ESP_ERROR_CHECK( esp_wifi_start()); 
    ESP_LOGI(TAG, "wifi_init_sta finished."); 
    
}
#elif defined(CONFIG_USE_ETHERNET) 
void w5500_reset() {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << ETH_SPI_INT_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE, 
    };
    gpio_config(&io_conf);
 
    gpio_config_t rst_gpio_conf = {
        .pin_bit_mask = (1ULL << ETH_SPI_RST_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,  
    };
    gpio_config(&rst_gpio_conf);

    gpio_set_level(ETH_SPI_RST_GPIO, 0);  
    vTaskDelay(pdMS_TO_TICKS(100));  
    gpio_set_level(ETH_SPI_RST_GPIO, 1); 
    vTaskDelay(pdMS_TO_TICKS(500));  
}

esp_err_t w5500_init() {
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &network_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    // Hardware Reset
    w5500_reset();

    // 1. Create Netif
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    // 2. Init SPI Bus
    spi_bus_config_t buscfg = {
        .miso_io_num = ETH_SPI_MISO_GPIO,
        .mosi_io_num = ETH_SPI_MOSI_GPIO,
        .sclk_io_num = ETH_SPI_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 3. Setup SPI Device Config (จุดที่เราแก้เรื่อง Pointer)
    static spi_device_interface_config_t devcfg = {
        .command_bits = 16,
        .address_bits = 8,
        .mode = 0,
        .clock_speed_hz = 40 * 1000 * 1000,
        .spics_io_num = ETH_SPI_CS_GPIO,
        .queue_size = 20};

    // 4. W5500 MAC & PHY Config
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = ETH_SPI_RST_GPIO;

    // 5. ใช้ Macro โดยส่ง Pointer ของ devcfg เข้าไปตามพิมพ์เขียวที่เราแกะกัน
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    w5500_config.int_gpio_num = ETH_SPI_INT_GPIO;

    // 6. Create Instances & Install Driver
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    // 7. ตั้งค่า MAC Address (ต้องทำก่อน Start เสมอ!)
    // uint8_t msg_mac[6] = { 0x02, 0x00, 0x00, 0x12, 0x34, 0x56 };
    uint8_t msg_mac[6];
    esp_read_mac(msg_mac, ESP_MAC_ETH); // ดึงเลข MAC พื้นฐานของบอร์ดมาใช้
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, msg_mac));

    // 8. Attach & Start
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_netif_dhcpc_start(eth_netif));

    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
    ESP_LOGI(TAG, "Ethernet W5500 initialized successfully!");

    return ESP_OK; 
}
#else
    #error "Default network interface not found"
#endif
 
void sensor_sht3x_init() {
    ESP_ERROR_CHECK(i2cdev_init());
    memset(&sht3x_dev, 0, sizeof(sht3x_t)); 
    ESP_ERROR_CHECK(sht3x_init_desc(&sht3x_dev, 0x44, 0, CONFIG_I2C_MASTER_SDA, CONFIG_I2C_MASTER_SCL));
    ESP_ERROR_CHECK(sht3x_init(&sht3x_dev)); 
    io_board_init(0xFF);

    ssd1306_config_t cfg = {
        .bus = SSD1306_I2C,
        .width = 128,
        .height = 64,
        .iface.i2c = {
            .port = I2C_NUM_0,
            .addr = 0x3C,
            .rst_gpio = GPIO_NUM_NC,
        },
    };
 
    if (ssd1306_new_i2c(&cfg, &disp) == ESP_OK) {
        ssd1306_found = true;
        ssd1306_clear(disp);
        ssd1306_draw_text_scaled(disp, 10, 20, "STARTING..", true, 2);
        ssd1306_display(disp);
    } 
}

void app_main(void) {
    check_boot_reason();
    sem_mqtt = xSemaphoreCreateMutex();
    init_serial_number();
    indicator_init();
    // 
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    sensor_sht3x_init();
#if defined(CONFIG_USE_WIFI)
    wifi_init();
#elif defined(CONFIG_USE_ETHERNET) 
    w5500_init();
#else
    #error "Default network interface not found"
#endif
    xTaskCreatePinnedToCore(task_sht3x, "sh301x_test", configMINIMAL_STACK_SIZE * 8, NULL, 5, NULL, APP_CPU_NUM); 

}

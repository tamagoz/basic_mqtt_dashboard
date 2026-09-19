#include "network.h"
#include "user_event.h"
#if defined(CONFIG_WIFI_MODE_SMARTCONFIG)
#include "esp_smartconfig.h"
#endif
#include "file_system.h"

static const char *TAG_NETWORK = "NETWORK";

#ifdef CONFIG_USE_WIFI
int s_retry_num = 0;
#endif
#if defined(CONFIG_WIFI_MODE_SMARTCONFIG)
static void smartconfig_task(void * parm);
#endif

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    user_event_post(NETWORK_EVENTS_BASE, EVENT_NETWORK_GOT_IP, (void *)event_data, sizeof(ip_event_got_ip_t), 0);
}

#if defined(CONFIG_WIFI_MODE_SMARTCONFIG)
static void smart_config_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data) {
    if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG_NETWORK, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG_NETWORK, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG_NETWORK, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

#ifdef CONFIG_SET_MAC_ADDRESS_OF_TARGET_AP
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            ESP_LOGI(TAG, "Set MAC address of target AP: "MACSTR" ", MAC2STR(evt->bssid));
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }
#endif

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG_NETWORK, "SSID:%s", ssid);
        ESP_LOGI(TAG_NETWORK, "PASSWORD:%s", password);
        if (save_wifi_config((char*)&ssid, (char*)&password) == ESP_OK) {
            ESP_LOGI(TAG_NETWORK, "Save wifi ssid/password success");
        } else {
            ESP_LOGE(TAG_NETWORK, "Save wifi ssid/password failed");
        }
        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI(TAG_NETWORK, "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        esp_smartconfig_stop();
    }                               
}
#endif

static void network_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
#if defined(CONFIG_USE_ETHERNET)
    switch (event_id)
    {
    case ETHERNET_EVENT_CONNECTED:
        // ESP_LOGI(TAG_NETWORK, "Ethernet Link Up");
        user_event_post(NETWORK_EVENTS_BASE, EVENT_NETWORK_UP, NULL, 0, 0);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        // ESP_LOGI(TAG_NETWORK, "Ethernet Link Down");
        user_event_post(NETWORK_EVENTS_BASE, EVENT_NETWORK_DOWN, NULL, 0, 0);
        break;
    default:
        break;
    }
#elif defined(CONFIG_USE_WIFI)
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
#if defined(CONFIG_WIFI_MODE_SMARTCONFIG)
        xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);
#else
        esp_wifi_connect();
#endif 
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_NETWORK, "retry to connect to the AP");
        }
        else
        {
            s_retry_num = 0;
            // post event
        }
        ESP_LOGI(TAG_NETWORK, "connect to the AP fail");
        user_event_post(NETWORK_EVENTS_BASE, EVENT_NETWORK_DOWN, NULL, 0, 0);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG_NETWORK, "connect to the AP fail");
        // post event
        user_event_post(NETWORK_EVENTS_BASE, EVENT_NETWORK_UP, NULL, 0, 0);
    }
#endif
}

#ifdef CONFIG_USE_ETHERNET
static esp_err_t w5500_eth_init()
{
    esp_err_t err = ESP_OK;
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &network_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << CONFIG_ETHER_INT_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE, // เปิด Pull-up
    };
    gpio_config(&io_conf);

    // เพิ่มส่วนนี้ก่อนเริ่มตั้งค่า SPI และ Ethernet
    gpio_config_t rst_gpio_conf = {
        .pin_bit_mask = (1ULL << CONFIG_ETHER_RST_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE, // เปิด Pull-up
    };
    gpio_config(&rst_gpio_conf);

    gpio_set_level(CONFIG_ETHER_RST_GPIO, 0);  
    vTaskDelay(pdMS_TO_TICKS(100));            
    gpio_set_level(CONFIG_ETHER_RST_GPIO, 1); 
    vTaskDelay(pdMS_TO_TICKS(500));            

    // Create Netif
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *eth_netif = esp_netif_new(&netif_cfg);

    // Init SPI Bus
    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_ETHER_MISO_GPIO,
        .mosi_io_num = CONFIG_ETHER_MOSI_GPIO,
        .sclk_io_num = CONFIG_ETHER_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
 
    static spi_device_interface_config_t devcfg = {
        .command_bits = 16,
        .address_bits = 8,
        .mode = 0,
        .clock_speed_hz = 29 * 1000 * 1000,
        .spics_io_num = CONFIG_ETHER_CS_GPIO,
        .queue_size = 20};

    // W5500 MAC & PHY Config
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = CONFIG_ETHER_RST_GPIO;

    // ใช้ Macro โดยส่ง Pointer ของ devcfg
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(SPI2_HOST, &devcfg);
    w5500_config.int_gpio_num = CONFIG_ETHER_INT_GPIO;

    // Create Instances & Install Driver
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &eth_handle));

    // ตั้งค่า MAC Address (ต้องทำก่อน Start เสมอ!) W5500 ไม่มี mac address
    // uint8_t msg_mac[6] = { 0x02, 0x00, 0x00, 0x12, 0x34, 0x56 };
    uint8_t msg_mac[6];
    esp_read_mac(msg_mac, ESP_MAC_ETH); // ดึงเลข MAC พื้นฐานของบอร์ดมาใช้
    ESP_ERROR_CHECK(esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, msg_mac));

    // Attach & Start
    ESP_ERROR_CHECK(esp_netif_attach(eth_netif, esp_eth_new_netif_glue(eth_handle)));
    ESP_ERROR_CHECK(esp_netif_dhcpc_start(eth_netif));

#if defined(CONFIG_ETHER_USE_STATIC)
    // หยุดการขอ DHCP
    ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif));
    // ตั้งค่า IP เอง (Static IP)
    esp_netif_ip_info_t ip_info;
    esp_netif_str_to_ip4(CONFIG_ETHER_STATIC_IP, &ip_info.ip);
    esp_netif_str_to_ip4(CONFIG_ETHER_STATIC_GW, &ip_info.gw);
    esp_netif_str_to_ip4(CONFIG_ETHER_STATIC_NETMASK, &ip_info.netmask);

    // IP4_ADDR(&ip_info.ip, CONFIG_ETHER_STATIC_IP);
    // IP4_ADDR(&ip_info.gw, 192, 168, 0, 1);
    // IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(eth_netif, &ip_info));
    ESP_LOGI(TAG_NETWORK, "Static IP Manual Set: %s", CONFIG_ETHER_STATIC_IP);
#endif
    err = esp_eth_start(eth_handle);
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG_NETWORK, "Ethernet W5500 initialized successfully!");
    if (err == ESP_OK)
    {
        user_event_post(NETWORK_EVENTS_BASE, EVENT_NETWORK_INIT, NULL, 0, 0);
    }

    return err;
}
#endif

#if defined(CONFIG_USE_WIFI)
static esp_err_t wifi_sta_init()
{
    esp_err_t err = ESP_OK;
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
#if defined(CONFIG_WIFI_MODE_SMARTCONFIG)
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &smart_config_event_handler, NULL) );
#endif
#if defined(CONFIG_WIFI_MODE_STATIC)   
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD, 
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "",
#ifdef CONFIG_ESP_WIFI_WPA3_COMPATIBLE_SUPPORT
            .disable_wpa3_compatible_mode = 0,
#endif
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    err = esp_wifi_start();
    ESP_ERROR_CHECK(err);
#elif defined(CONFIG_WIFI_MODE_SMARTCONFIG)
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() ); 
#endif 
    ESP_LOGI(TAG_NETWORK, "wifi_init_sta finished."); 
    return err;
}
#endif

#if defined(CONFIG_WIFI_MODE_SMARTCONFIG)
static void smartconfig_task(void * parm) {
    if (file_config_exists() == ESP_OK) {
        ESP_LOGI(TAG_NETWORK, "smart config file found, esp_wifi_connect "); 
        char *ssid = (char *)malloc(32);
        char *pass = (char *)malloc(64);  
        esp_err_t err = get_wifi_user_password(ssid, pass);
        if (err == ESP_OK) {
            wifi_config_t wifi_config;  
            bzero(&wifi_config, sizeof(wifi_config_t));
            memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
            memcpy(wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
            wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;      
            
            ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
            esp_wifi_connect(); 
        } else {
            ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
            smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
            ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) ); 
        }
    } else {
        ESP_LOGI(TAG_NETWORK, "smart config file not found, esp_smartconfig_start "); 
        ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
        smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
        ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) ); 
    }

    vTaskDelete(NULL);

}

#endif

esp_err_t connect_network()
{
    esp_err_t err = ESP_OK; 
#if defined(CONFIG_USE_ETHERNET)
    err = w5500_eth_init();
#elif defined(CONFIG_USE_WIFI) 
    err = wifi_sta_init();
#endif 
    return err;
}
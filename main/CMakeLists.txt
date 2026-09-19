set(extra_srcs ssd1306 pcf8574 sht3x i2cdev)
set(driver_srcs driver esp_driver_tsens esp_driver_i2c esp_driver_spi esp_driver_gpio esp_driver_uart)
set(network_scrs nvs_flash esp_wifi esp_eth esp_netif)
set(eth_srcs w5500)
set(fat_srcs vfs fatfs sdmmc esp_mm spi_flash esp_partition littlefs)
set(app_srcs led_strip cjson mqtt esp-modbus)
set(includes . ./include)
file(GLOB SOURCES *.c)
idf_component_register(SRCS "user_event.c" "led_indicator.c" "file_system.c" "network.c" "board_io.c" "main.c" "${SOURCES}"
                    INCLUDE_DIRS "${includes}"
                    REQUIRES 
                        ${extra_srcs} 
                        ${driver_srcs}
                        ${network_scrs}
                        ${fat_srcs}
                        ${eth_srcs}
                        ${app_srcs}
                        console 
                        freertos  
                        esp_event
)

# Note: you must have a partition named the first argument (here it's "littlefs")
# in your partition table csv file.
#littlefs_create_partition_image(littlefs ../flash_data FLASH_IN_PROJECT)

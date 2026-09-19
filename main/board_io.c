#include "board_io.h"

// static const char *TAG_IO = "PCF8574";

static i2c_dev_t *pcf8574;
uint8_t _writebuf = 0x00;
uint8_t _readbuf = 0x0;

esp_err_t io_board_init(int init_value) {
    pcf8574 = (i2c_dev_t*)malloc(sizeof(i2c_dev_t)); 
    memset(pcf8574, 0, sizeof(i2c_dev_t));
    ESP_ERROR_CHECK(pcf8574_init_desc(pcf8574, PCF_I2C_ADDR, 0, CONFIG_I2C_MASTER_SDA, CONFIG_I2C_MASTER_SCL)); 

    pcf8574_port_write(pcf8574, 0xFF); 
    
    return ESP_OK;
}

int pcf_digital_write(int pin, int value) {
    if (pin > 7) { 
        return 0;
    }
    pcf8574_port_read(pcf8574, &_writebuf);
    if (value == 0) {
        _writebuf &= ~(1 << pin);
    } else {
        _writebuf |= (1 << pin);
    }
    // ESP_LOGI(TAG_IO, "VALUE: %d", _writebuf);
    pcf8574_port_write(pcf8574, _writebuf);
    return value;
}

int pcf_digital_read(int pin) {
    if (pin > 7) { 
        return 0;
    }
    pcf8574_port_read(pcf8574, &_readbuf);
    return (_readbuf & (1 << pin)) > 0;
}
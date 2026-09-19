#ifndef _BOARD_IO_H_
#define _BOARD_IO_H_ 

#include "common.h"
#include <pcf8574.h>

#define PCF_I2C_ADDR CONFIG_PCF_I2C_ADDR

esp_err_t io_board_init(int init_value);

int pcf_digital_write(int pin, int value);
int pcf_digital_read(int pin);

#endif
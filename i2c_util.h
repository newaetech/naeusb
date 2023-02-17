#pragma once
#include <stdint.h>
extern volatile uint8_t I2C_LOCK;

void i2c_setup(void);
void i2c_reset(void);

int i2c_write(uint8_t chip_addr, uint8_t reg_addr, void *data, uint8_t len);
int i2c_read(uint8_t chip_addr, uint8_t reg_addr,  void *data, uint8_t len);
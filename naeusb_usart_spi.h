#include <stdint.h>
#pragma once

void ctrl_spi1util(void);
uint8_t spi1util_xferbyte(uint8_t b);
extern uint8_t spi1util_data_buffer[64];
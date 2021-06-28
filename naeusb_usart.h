#pragma once
#include "naeusb.h"

#define REQ_USART0_DATA 0x1A
#define REQ_USART0_CONFIG 0x1B

void naeusart_register_handlers(void);
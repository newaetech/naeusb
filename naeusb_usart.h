#pragma once
#include "naeusb.h"

#define REQ_USART0_DATA 0x1A
#define REQ_USART0_CONFIG 0x1B
#define REQ_XMEGA_PROGRAM 0x20
#define REQ_AVR_PROGRAM 0x21

void naeusart_register_handlers(void);
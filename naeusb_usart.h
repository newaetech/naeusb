#pragma once
#include "naeusb.h"

#define REQ_USART0_DATA 0x1A
#define REQ_USART0_CONFIG 0x1B
#define REQ_XMEGA_PROGRAM 0x20
#define REQ_AVR_PROGRAM 0x21
#define REQ_CDC_SETTINGS_EN 0x31

/* Target/Extra SPI Transfer */
#define REQ_FPGASPI1_XFER 0x35

typedef struct {
	Usart * usart;
	sam_usart_opt_t usartopts;
	tcirc_buf rxbuf;
	tcirc_buf txbuf;
	tcirc_buf rx_cdc_buf;
	int usart_id;
	uint8_t cdc_supported:1;
	uint8_t enabled:1;
	uint8_t cdc_enabled:1;
	uint8_t cdc_settings_change:1;
} usart_driver;


void naeusart_register_handlers(void);
usart_driver *get_usart_from_id(int id);
usart_driver *get_nth_available_driver(int id);

void cdc_send_to_pc(void);
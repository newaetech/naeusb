#pragma once
#include "naeusb.h"

// Ctrl Write
// wValue top byte selects which UART to use (0-3)
#define REQ_USART0_DATA 0x1A // Writes data to be handled by UART buffer
#define REQ_USART0_CONFIG 0x1B
#define REQ_XMEGA_PROGRAM 0x20
#define REQ_AVR_PROGRAM 0x21
#define REQ_CDC_SETTINGS_EN 0x31
#define REQ_FPGASPI1_XFER 0x35

// Ctrl Read
#define REQ_USART0_DATA 0x1A
#define REQ_USART0_CONFIG 0x1B
#define REQ_XMEGA_PROGRAM 0x20
#define REQ_AVR_PROGRAM 0x21
#define REQ_CDC_SETTINGS_EN 0x31
#define REQ_FPGASPI1_XFER 0x35

// #define USART_BUF_SZ 256
#define USART_TX_BUF_SZ 512
#define USART_RX_BUF_SZ 1024*6

#define USART_XON 0x11
#define USART_XOFF 0x13

struct dma_state {
    // uint8_t buf[USART_BUF_SZ];
    Pdc *pdc;
    Usart *usart;

    union {
        uint32_t buf_st;
        uint8_t *buf_st_ptr;
    };

    union {
        uint32_t buf_head;
        uint8_t *buf_head_ptr;
    };


    union {
        uint32_t buf_end;
        uint8_t *buf_end_ptr;
    };
    uint8_t is_tx;
	uint16_t buf_sz;

	uint8_t dropped;
};

struct usart_state {
    Pdc *pdc;
    Usart *usart;
	struct dma_state tx_st;
	struct dma_state rx_st;

	sam_usart_opt_t opts;
    uint8_t cdc_enabled;
    uint8_t cdc_supported;
    uint8_t cdc_os_enabled;

    uint8_t xonxoff_enabled;
    uint8_t currently_xoff;

    union {
        uint8_t *xonxoff_last_rx_ptr;
        uint32_t xonxoff_last_rx;
    };
    
    uint8_t usart_id;
    uint32_t errors;
};

void naeusart_register_handlers(void);

int usart_rx_overrun(struct dma_state *state);
int usart_tx_overrun(struct dma_state *state);

// void cdc_send_to_pc(void);
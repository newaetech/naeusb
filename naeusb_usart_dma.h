#pragma once
#include <stdint.h>
#include "naeusb_usart.h"
#include "pdc.h"


uint16_t usart_inwait(struct     dma_state *dst);
uint16_t usart_free_space(struct dma_state *dst);
void setup_usart_dma(struct      usart_state *dst);
void usart_enable_dma(struct      usart_state *dst);
void usart_disable_dma(struct      usart_state *dst);
uint16_t usart_dma_read(struct   usart_state *dst, uint8_t *buf, uint16_t len);
uint16_t usart_dma_write(struct  usart_state *dst, uint8_t *buf, uint16_t len);
void handle_sw_flow_control_int(struct usart_state *ust);
void handle_rx_usart_overrun_int(struct usart_state *ust);

static inline uint32_t usart_dma_ptr(struct dma_state *dst)
{
    if (dst->is_tx) {
        return pdc_read_tx_ptr(dst->pdc);
    } else {
        return pdc_read_rx_ptr(dst->pdc);
    }
}

static inline uint32_t usart_dma_nptr(struct dma_state *dst)
{
    if (dst->is_tx) {
        return pdc_read_tx_next_ptr(dst->pdc);
    } else {
        return pdc_read_rx_next_ptr(dst->pdc);
    }
}

static inline char is_dma_wrapped(struct dma_state *dst)
{
    if (dst->is_tx) {
        return (dst->buf_head < usart_dma_ptr(dst)); //dst->pdc->PERIPH_TNCR != 0;
    } else {
        return (dst->buf_head > usart_dma_ptr(dst)) || ((dst->buf_head != dst->buf_st) && (dst->dropped));
    }
}

void handle_cdc_txrx(void);
void handle_sw_flow_control(void);
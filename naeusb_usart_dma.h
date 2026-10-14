#include <stdint.h>
#include "naeusb_usart.h"
#pragma once


uint16_t usart_inwait(struct     dma_state *dst);
uint16_t usart_free_space(struct dma_state *dst);
void setup_usart_dma(struct      usart_state *dst);
void usart_enable_dma(struct      usart_state *dst);
void usart_disable_dma(struct      usart_state *dst);
uint16_t usart_dma_read(struct   usart_state *dst, uint8_t *buf, uint16_t len);
uint16_t usart_dma_write(struct  usart_state *dst, uint8_t *buf, uint16_t len);
void handle_rx_usart_overrun_int(struct usart_state *ust);
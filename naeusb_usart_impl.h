#pragma once

#include "naeusb_usart.h"
#include "pdc.h"
bool usart_fill_opts(sam_usart_opt_t *opts, uint32_t baud, uint8_t stop_bits, uint8_t parity, uint8_t dbits);
int driver_to_port(struct usart_state *driver);
struct usart_state *get_nth_available_driver(int port);
struct usart_state *get_usart_from_id(int id);
void usart_handle_xon(struct usart_state *driver);
void usart_handle_xoff(struct usart_state *driver);
void usart_enableIO(struct usart_state *driver);

#define word2buf(buf, word)   do{buf[0] = LSB0W(word); buf[1] = LSB1W(word); buf[2] = LSB2W(word); buf[3] = LSB3W(word);}while (0)
#define buf2word(word, buf)   do{word = *((uint32_t *)buf);}while (0)
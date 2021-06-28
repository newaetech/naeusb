/*
 * rs232_driver.h
 *
 * Created: 16/12/2014 10:02:58 AM
 *  Author: colin
 */ 

#ifndef USART_DRIVER_H_
#define USART_DRIVER_H_

#include "circbuffer.h"

bool ctrl_usart(Usart * usart, bool directionIn);
void usart_driver_putchar(Usart * usart, tcirc_buf * txbuf, uint8_t data);
uint8_t usart_driver_getchar(Usart * usart);

extern tcirc_buf rx0buf, tx0buf;
extern tcirc_buf rx1buf, tx1buf;
extern tcirc_buf rx2buf, tx2buf;
extern tcirc_buf rx3buf, tx3buf;
extern tcirc_buf usb_usart_circ_buf;

#define USART_TARGET USART0
#define PIN_USART0_RXD	         (PIO_PA19_IDX)
#define PIN_USART0_RXD_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)
#define PIN_USART0_TXD	        (PIO_PA18_IDX)
#define PIN_USART0_TXD_FLAGS      (PIO_PERIPH_A | PIO_DEFAULT)

#endif /* USART_DRIVER_H_ */
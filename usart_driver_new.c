/*
 * Copyright (c) 2014-2015 NewAE Technology Inc.
 * All rights reserved.
 *
 * Based on code from FIP, the Flexible IP Stack.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or other
 *   materials provided with the distribution.
 *
 * * Neither the name of the author nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <asf.h>
#include "circbuffer.h"
#include "usart_driver.h"
#include "usart.h"
#include "usb_protocol_cdc.h"
extern bool enable_cdc_transfer[2];

#define USART_WVREQ_INIT    0x0010
#define USART_WVREQ_ENABLE  0x0011
#define USART_WVREQ_DISABLE 0x0012
#define USART_WVREQ_NUMWAIT 0x0014
#define USART_WVREQ_NUMWAIT_TX 0x0018

#define word2buf(buf, word)   do{buf[0] = LSB0W(word); buf[1] = LSB1W(word); buf[2] = LSB2W(word); buf[3] = LSB3W(word);}while (0)
#define buf2word(word, buf)   do{word = *((uint32_t *)buf);}while (0)

typedef struct {
    Usart * usart;,
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

void usart_dummy_func(void)
{

}

void usart0_enableIO(void) __attribute__((weak, alias("usart_dummy_func")));
void usart1_enableIO(void) __attribute__((weak, alias("usart_dummy_func")));
void usart2_enableIO(void) __attribute__((weak, alias("usart_dummy_func")));
void usart3_enableIO(void) __attribute__((weak, alias("usart_dummy_func")));

void usart_enableIO(usart_driver *driver)
{
    if (driver->usart_id == 0) {
        usart0_enableIO();
    } else if (driver->usart_id == 1) {
        usart1_enableIO();
    } else if (driver->usart_id == 2) {
        usart2_enableIO();
    } else if (driver->usart_id == 3) {
        usart3_enableIO();
    }
}

void generic_isr(usart_driver *driver)
{
	uint32_t status;
	status = usart_get_status(usart);
	if (status & US_CSR_RXRDY){
		uint32_t temp;
		temp = driver->usart->US_RHR & US_RHR_RXCHR_Msk;
		add_to_circ_buf(&driver->rxbuf, temp, false);
        if (driver->use_cdc)
            add_to_circ_buf(&driver->rx_cdc_buf, temp, false);
	}
	
	if (status & US_CSR_TXRDY){
		if (circ_buf_has_char(&driver->txbuf)){
			//Still data to send
			usart_putchar(driver->usart, get_from_circ_buf(&driver->txbuf));			
		} else {
			//No more data, stop this madness
			usart_disable_interrupt(driver->usart, UART_IER_TXRDY);
		}
	}
}

#ifdef CW_USE_USART0
usart_driver usart0_driver;
ISR(USART0_Handler)
{
	generic_isr(USART0, &rx0buf, &tx0buf);
}
#else
#endif

#ifdef CW_USE_USART1
usart_driver usart1_driver;
ISR(USART1_Handler)
{
	generic_isr(USART1, &rx1buf, &tx1buf);
}
#else
#endif

#ifdef CW_USE_USART2
#ifndef USART2
#error "USART2 unavailable"
#endif
usart_driver usart2_driver;
ISR(USART2_Handler)
{
	generic_isr(USART2, &rx1buf, &tx1buf);
}
#else
#endif

#ifdef CW_USE_USART3
#ifndef USART3
#error "USART3 unavailable"
#endif
usart_driver usart3_driver;
#else
#endif
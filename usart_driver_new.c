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

#ifdef CW_USE_USART0
usart_driver usart0_driver = {
    .usart = USART0,
    .usart_id = 0,
    .cdc_supported = 1,
    .cdc_settings_change = 1,

};
ISR(USART0_Handler)
{
	generic_isr(USART0, &rx0buf, &tx0buf);
}
#else
#endif

#ifdef CW_USE_USART1
usart_driver usart1_driver = {
    .usart = USART1,
    .usart_id = 0,
    .cdc_supported = 1,
    .cdc_settings_change = 1,

};
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

bool ctrl_naeusart_in(void)
{
    usart_driver *driver = get_usart_from_id(udd_g_ctrlreq.req.wValue >> 8);
    uint32_t cnt;

    if (!driver) return false;

    switch (udd_g_ctrlreq.req.wValue & 0xFF) {
    case USART_WVREQ_INIT:
        return true;
        
    case USART_WVREQ_NUMWAIT:
        if (udd_g_ctrlreq.req.wLength < 4) {
            return false;
        }

        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 4;
        cnt = circ_buf_count(&driver->rxbuf);
        word2buf(respbuf, cnt);
        return true;
    
    case USART_WVREQ_NUMWAIT_TX:
        if (udd_g_ctrlreq.req.wLength < 4) {
            return false;
        }
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 4;
        cnt = circ_buf_count(&driver->txbuf);
        word2buf(respbuf, cnt);
        return true;

    }

    return false;
}

bool configure_usart(usart_driver *driver, uint32_t baud, uint8_t stop_bits, uint8_t parity, uint8_t dbits)
{
    driver->usartopts.baud = baud;
    switch(stop_bits)
        {
        case 0:
            driver->usartopts.stop_bits = US_MR_NBSTOP_1_BIT;
            break;
        case 1:
            driver->usartopts.stop_bits = US_MR_NBSTOP_1_5_BIT;
            break;
        case 2:
            driver->usartopts.stop_bits = US_MR_NBSTOP_2_BIT;
            break;
        default:
            driver->usartopts.stop_bits = US_MR_NBSTOP_1_BIT;
        }

    /* Parity */
    switch(parity)
        {
        case 0:
            driver->usartopts.parity_type = US_MR_PAR_NO;
            break;
        case 1:
            driver->usartopts.parity_type = US_MR_PAR_ODD;
            break;
        case 2:
            driver->usartopts.parity_type = US_MR_PAR_EVEN;
            break;
        case 3:
            driver->usartopts.parity_type = US_MR_PAR_MARK;
            break;
        case 4:
            driver->usartopts.parity_type = US_MR_PAR_SPACE;
            break;							
        default:
            driver->usartopts.parity_type = US_MR_PAR_NO;
        }

    /* Data Bits */
    switch(dbits)
        {
        case 5:
            driver->usartopts.char_length = US_MR_CHRL_5_BIT;
            break;
        case 6:
            driver->usartopts.char_length = US_MR_CHRL_6_BIT;
            break;
        case 7:
            driver->usartopts.char_length = US_MR_CHRL_7_BIT;
            break;					
        case 8:							
        default:
            driver->usartopts.char_length = US_MR_CHRL_8_BIT;
        }
        
    driver->usartopts.channel_mode = US_MR_CHMODE_NORMAL;
    usart_enableIO(driver);
    driver->enabled = 1;
    init_circ_buf(&driver->txbuf);
    init_circ_buf(&driver->rxbuf);
    init_circ_buf(&driver->rx_cdc_buf);

    usart_init_rs232(driver->usart, &driver->usartopts, sysclk_get_cpu_hz());
    
}

void ctrl_naeusbusart_out(void)
{
    usart_driver *driver = get_usart_from_id(udd_g_ctrlreq.req.wValue >> 8);
    if (!driver) return false;

    uint32_t baud;

    switch (udd_g_ctrlreq.req.wValue & 0xFF) {
    case USART_WVREQ_INIT:
        if (udd_g_ctrlreq.req.wLength != 7) return false;

        buf2word(baud, udd_g_ctrlreq.payload);
        configure_usart(driver, baud, udd_g_ctrlreq.payload[4], 
        udd_g_ctrlreq.payload[5], udd_g_ctrlreq.payload[6]);

        return true;

    case USART_RVREQ_ENABLE:
        usart_enable_rx(driver->usart);
        usart_enable_tx(driver->usart);

        usart_enable_interupt(usart, UART_IER_RXRDY);
        usart_enableIO(driver);

    case USART_WVREQ_DISABLE:
        usart_disable_rx(driver->usart);
        usart_disable_tx(driver->usart);
        usart_disable_interrupt(driver->usart, UART_IER_RXRDY | USART_IER_TXRDY);
        return true;
    }

}

void usart_driver_putchar(usart_driver *driver, uint8_t data)
{

    add_to_circ_buf(&driver->txbuf, data, false);

	// Send the first byte if nothing is yet being sent
	// This is determined by seeing if the TX complete interrupt is
	// enabled.
	if ((usart_get_interrupt_mask(driver->usart) & US_CSR_TXRDY) == 0) {
		if ((usart_get_status(driver->usart) & US_CSR_TXRDY))
			usart_putchar(driver->usart, get_from_circ_buf(txbuf));
		usart_enable_interrupt(driver->usart, US_CSR_TXRDY);
	}
}

uint8_t usart_driver_getchar(usart_driver *driver)
{
    return get_from_circ_buf(&driver->rxbuf);
}

usart_driver *get_usart_from_id(int id) {
    #ifdef CW_USE_USART0
        if (id == 0) return &usart0_driver;
    #endif
    #ifdef CW_USE_USART1
        if (id == 1) return &usart1_driver;
    #endif

    return NULL;
}


// naeusart because I wouldn't be surpised if usart_register_handlers collides with something
void naeusart_register_handlers(void)
{
    naeusb_add_in_handler(usart_setup_in_received);
    naeusb_add_out_handler(usart_setup_out_received);
}

//////CDC FUNC

void naeusb_cdc_settings_out(void)
{
    for (uint8_t i = 0; i < UDI_CDC_PORT_NB; i++) {

    }
}
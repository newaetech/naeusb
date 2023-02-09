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
#include "naeusb_default.h"
#include "usart.h"
#include "usb_protocol_cdc.h"
#include "naeusb_usart.h"

#ifdef CW_TARGET_SPI
#include "targetspi.h"
#endif

#ifdef CW_PROG_XMEGA
#include "XPROGNewAE.h"
#endif

#ifdef CW_PROG_AVR
#include "V2Protocol.h"
#endif

#define USART_WVREQ_INIT    0x0010
#define USART_WVREQ_ENABLE  0x0011
#define USART_WVREQ_DISABLE 0x0012
#define USART_WVREQ_NUMWAIT 0x0014
#define USART_WVREQ_NUMWAIT_TX 0x0018

#define word2buf(buf, word)   do{buf[0] = LSB0W(word); buf[1] = LSB1W(word); buf[2] = LSB2W(word); buf[3] = LSB3W(word);}while (0)
#define buf2word(word, buf)   do{word = *((uint32_t *)buf);}while (0)

bool usart_setup_in_received(void);
bool usart_setup_out_received(void);
bool naeusb_cdc_settings_in(void);
void naeusb_cdc_settings_out(void);

void generic_isr(usart_driver *driver);

bool NAEUSB_CDC_IS_RUNNING = false;

#ifdef CW_TARGET_SPI

void spi1util_toggleclk(uint8_t cycles, bool mosi_status)
{
    if (mosi_status){
        gpio_set_pin_high(SPI_MOSI_GPIO);
        gpio_configure_pin(SPI_MOSI_GPIO, (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT));
    } else {
        gpio_set_pin_low(SPI_MOSI_GPIO);
        gpio_configure_pin(SPI_MOSI_GPIO, (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT));
    }
    gpio_set_pin_low(SPI_SPCK_GPIO);
	gpio_configure_pin(SPI_SPCK_GPIO, (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT));

    while(cycles--){
        gpio_set_pin_low(SPI_SPCK_GPIO);
        delay_cycles(1);
        gpio_set_pin_high(SPI_SPCK_GPIO);
        delay_cycles(1);
        gpio_set_pin_low(SPI_SPCK_GPIO);
    }
    
	gpio_configure_pin(SPI_MOSI_GPIO, SPI_MOSI_FLAGS);
	gpio_configure_pin(SPI_SPCK_GPIO, SPI_SPCK_FLAGS);
}

void spi1util_init(uint32_t prog_freq)
{
    spi_enable_clock(SPI);
	spi_reset(SPI);
	spi_set_master_mode(SPI);
	spi_disable_mode_fault_detect(SPI);
	spi_disable_loopback(SPI);

	spi_set_clock_polarity(SPI, 0, 0);
	spi_set_clock_phase(SPI, 0, 1);
	spi_set_baudrate_div(SPI, 0, spi_calc_baudrate_div(prog_freq, sysclk_get_cpu_hz()));

	spi_enable(SPI);

	gpio_configure_pin(SPI_MOSI_GPIO, SPI_MOSI_FLAGS);
	gpio_configure_pin(SPI_SPCK_GPIO, SPI_SPCK_FLAGS);
	gpio_configure_pin(SPI_MISO_GPIO, SPI_MISO_FLAGS);
}

void spi1util_deinit(void)
{
    ;
}

static uint8_t spi1util_data_buffer[64];
static int spi1util_databuffer_len = 0;

uint8_t spi1util_xferbyte(uint8_t b){
    uint16_t data = b;
    uint8_t ignored;
    spi_write(SPI, data, 0, 0);
    spi_read(SPI, &data, &ignored);
    return data;
}

/* TODO - Move following into seperate file? Maybe remove this from naeusb_fpga_target.c too then */
static void ctrl_spi1util(void){
	
    uint32_t prog_freq = 100E3;
	switch(udd_g_ctrlreq.req.wValue){
		case 0xA0:
            if (udd_g_ctrlreq.req.wLength == 4) {
                prog_freq = *(CTRLBUFFER_WORDPTR);
            }
			spi1util_init(prog_freq);			
			break;
			
		case 0xA1:
			spi1util_deinit();
			break;
			
		case 0xA2: //Not implemented here, done via Python GPIO
			//spi1util_cs_low();
			break;

		case 0xA3: //Not implemented here, done via Python GPIO
			//spi1util_cs_high();
			break;

		case 0xA4:
			//Catch heartbleed-style error
			if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
				return;
			}

			if (udd_g_ctrlreq.req.wLength > sizeof(spi1util_data_buffer)){
				return;
			}
			spi1util_databuffer_len = udd_g_ctrlreq.req.wLength;
			for (int i = 0; i < udd_g_ctrlreq.req.wLength; i++){
				spi1util_data_buffer[i] = spi1util_xferbyte(udd_g_ctrlreq.payload[i]);
			}

			break;
        
        case 0xA5:
            //nrst low (required due to sharing of nRST/SPI mode with programming)
            gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT));
            gpio_set_pin_low(PIN_TARG_NRST_GPIO);
            break;
        
        case 0xA6:
            //nrst high (required due to sharing of nRST/SPI mode with programming)
            gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT));
            gpio_set_pin_high(PIN_TARG_NRST_GPIO);
            break;
        
        case 0xA7:
            //nrst high-z (required due to sharing of nRST/SPI mode with programming)
            gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_INPUT | PIO_DEFAULT));
            break;
        
        case 0xA8:
            //Toggle SPCK pin as needed
            if(udd_g_ctrlreq.req.wLength == 2){
                spi1util_toggleclk(udd_g_ctrlreq.payload[0], udd_g_ctrlreq.payload[1]);
            }
            break;

		default:
			break;
	}
}
#endif


#ifdef CW_USE_USART0
// #error CWUSEUSART0
usart_driver usart0_driver = {
    .usart = USART0,
    .usart_id = 0,
    .cdc_supported = 1,
    .cdc_settings_change = 1,

};

ISR(USART0_Handler)
{
	generic_isr(&usart0_driver);
}
void usart0_enableIO(void)
{
	sysclk_enable_peripheral_clock(ID_USART0);
	#if USB_DEVICE_PRODUCT_ID != 0xC310
	gpio_configure_pin(PIN_USART0_RXD, PIN_USART0_RXD_FLAGS);
	gpio_configure_pin(PIN_USART0_TXD, PIN_USART0_TXD_FLAGS);
	#endif
	irq_register_handler(USART0_IRQn, 3);
}
#else
void usart0_enableIO(void)
{
	
}
#endif

#ifdef CW_USE_USART1
usart_driver usart1_driver = {
    .usart = USART1,
    .usart_id = 1,
    .cdc_supported = 1,
    .cdc_settings_change = 1,

};
ISR(USART1_Handler)
{
	generic_isr(&usart1_driver);
}

void usart1_enableIO(void)
{
	sysclk_enable_peripheral_clock(ID_USART1);
	
	#if USB_DEVICE_PRODUCT_ID != 0xC310
	gpio_configure_pin(PIN_USART1_RXD_IDX, PIN_USART1_RXD_FLAGS);
	gpio_configure_pin(PIN_USART1_TXD_IDX, PIN_USART1_TXD_FLAGS);
	#endif
	irq_register_handler(USART1_IRQn, 3);
}
#else
void usart1_enableIO(void)
{
	
}
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
void usart2_enableIO(void)
{
	
}
#endif

#ifdef CW_USE_USART3
#ifndef USART3
#endif
usart_driver usart3_driver = {
    .usart = USART3,
    .usart_id = 3,
    .cdc_supported = 1,
    .cdc_settings_change = 1,

};
ISR(USART3_Handler)
{
	generic_isr(&usart3_driver);
}
void usart3_enableIO(void)
{
	sysclk_enable_peripheral_clock(ID_USART3);
	gpio_configure_pin(PIN_USART3_RXD, PIN_USART3_RXD_FLAGS);
	gpio_configure_pin(PIN_USART3_TXD, PIN_USART3_TXD_FLAGS);
	irq_register_handler(USART3_IRQn, 3);
}
#else
void usart3_enableIO(void)
{
	
}
#endif

void usart_dummy_func(void)
{

}


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
	status = usart_get_status(driver->usart);
	if (status & US_CSR_RXRDY){
		uint32_t temp;
		temp = driver->usart->US_RHR & US_RHR_RXCHR_Msk;
		add_to_circ_buf(&driver->rxbuf, temp, false);
        if (driver->cdc_enabled)
            add_to_circ_buf(&driver->rx_cdc_buf, temp, false);
        if (driver->rxbuf.dropped > 0) {
            // LED_On(LED1_GPIO);
            CURRENT_ERRORS |= CW_ERR_USART_RX_OVERFLOW;
        }
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

bool ctrl_usart_in(void)
{
    usart_driver *driver = get_nth_available_driver(udd_g_ctrlreq.req.wValue >> 8);
    if (!driver)
        return false;
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
        driver->rxbuf.dropped = 0; //clear dropped characters
        word2buf(respbuf, cnt);
        CURRENT_ERRORS &= ~CW_ERR_USART_RX_OVERFLOW;
        return true;
    
    case USART_WVREQ_NUMWAIT_TX:
        if (udd_g_ctrlreq.req.wLength < 4) {
            return false;
        }
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 4;
        cnt = circ_buf_count(&driver->txbuf);
        driver->txbuf.dropped = 0; //clear dropped characters
        word2buf(respbuf, cnt);
        CURRENT_ERRORS &= ~CW_ERR_USART_TX_OVERFLOW;
        return true;



    }

    return false;
}

bool configure_usart(usart_driver *driver, uint32_t baud, uint8_t stop_bits, uint8_t parity, uint8_t dbits)
{
    driver->usartopts.baudrate = baud;
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
    return true;
}

void ctrl_usart_out(void)
{
    usart_driver *driver = get_nth_available_driver(udd_g_ctrlreq.req.wValue >> 8);
    if (!driver) return;

    uint32_t baud;

    switch (udd_g_ctrlreq.req.wValue & 0xFF) {
    case USART_WVREQ_INIT:
        if (udd_g_ctrlreq.req.wLength != 7) return;

        buf2word(baud, udd_g_ctrlreq.payload);
		usart_enableIO(driver);
        configure_usart(driver, baud, udd_g_ctrlreq.payload[4], 
        udd_g_ctrlreq.payload[5], udd_g_ctrlreq.payload[6]);

        return ;

    case USART_WVREQ_ENABLE:
		usart_enableIO(driver);
        usart_enable_rx(driver->usart);
        usart_enable_tx(driver->usart);

        usart_enable_interrupt(driver->usart, UART_IER_RXRDY);
        
		return;

    case USART_WVREQ_DISABLE:
        usart_disable_rx(driver->usart);
        usart_disable_tx(driver->usart);
        usart_disable_interrupt(driver->usart, UART_IER_RXRDY | UART_IER_TXRDY);
        return ;


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
			usart_putchar(driver->usart, get_from_circ_buf(&driver->txbuf));
		usart_enable_interrupt(driver->usart, US_CSR_TXRDY);
	}
}

uint8_t usart_driver_getchar(usart_driver *driver)
{
    return get_from_circ_buf(&driver->rxbuf);
}

usart_driver *get_usart_from_id(int id) 
{
    #ifdef CW_USE_USART0
        if (id == 0) return &usart0_driver;
    #endif
    #ifdef CW_USE_USART1
        if (id == 1) return &usart1_driver;
    #endif
    #ifdef CW_USE_USART2
        if (id == 2) return &usart2_driver;
    #endif
    #ifdef CW_USE_USART3
        if (id == 3) return &usart3_driver;
    #endif

    return NULL;
}

usart_driver *get_nth_available_driver(int port)
{
	usart_driver *driver;
	for (uint8_t i = 0; i < 4; i++) {
		driver = get_usart_from_id(i);
		if (!driver)
		continue;
		if (port == 0) break;
		port--;
	}
	if (port != 0) return NULL;
	return driver;
}

static void ctrl_usart_cb(void)
{
	
	ctrl_usart_out();
}

static void ctrl_usart_cb_data(void)
{		
	usart_driver *driver = get_nth_available_driver(udd_g_ctrlreq.req.wValue >> 8);
	if (!driver) return;
	//Catch heartbleed-style error
	if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
		return;
	}
	
	for (int i = 0; i < udd_g_ctrlreq.req.wLength; i++){
		usart_driver_putchar(driver, udd_g_ctrlreq.payload[i]);
	}
    if (driver->txbuf.dropped > 0) {
        CURRENT_ERRORS |= CW_ERR_USART_TX_OVERFLOW;
    }
}

#ifdef CW_PROG_XMEGA
void ctrl_xmega_program_void(void)
{
	XPROGProtocol_Command();
}
#endif

#ifdef CW_PROG_AVR
void ctrl_avr_program_void(void)
{
	V2Protocol_ProcessCommand();
}
#endif

// naeusart because I wouldn't be surpised if usart_register_handlers collides with something
void naeusart_register_handlers(void)
{
	for (uint8_t i = 0; i < 4; i++) {
		usart_driver *driver = get_nth_available_driver(i);
		if (!driver) continue;
		usart_enableIO(driver);
	}
    naeusb_add_in_handler(usart_setup_in_received);
    naeusb_add_out_handler(usart_setup_out_received);
}

//////CDC FUNC



void naeusb_cdc_settings_out(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        usart_driver *driver = get_nth_available_driver(i);
        if (driver) {
            if (udd_g_ctrlreq.req.wValue & (1 << i)) {
                driver->cdc_settings_change = 1;
            } else {
                driver->cdc_settings_change = 0;
            }
        }
    }
}

bool naeusb_cdc_settings_in(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        usart_driver *driver = get_nth_available_driver(i);
		respbuf[i] = 0;
        if (driver)
            respbuf[i] = driver->cdc_settings_change;
    }
    udd_g_ctrlreq.payload = respbuf;
    udd_g_ctrlreq.payload_size = min(4, udd_g_ctrlreq.req.wLength);
    return true;

}

bool cdc_enable(uint8_t port)
{
	usart_driver *driver = get_nth_available_driver(port);
	
    driver->cdc_enabled = 1;
    return true;
}

void cdc_disable(uint8_t port)
{
	usart_driver *driver = get_nth_available_driver(port);

    driver->cdc_enabled = 0;
    NAEUSB_CDC_IS_RUNNING = false;
}

uint8_t uart_buf[512] = {0};
void my_callback_rx_notify(uint8_t port)
{

	usart_driver *driver = get_nth_available_driver(port);
    NAEUSB_CDC_IS_RUNNING = true;
    
    if (driver->cdc_enabled && driver->enabled) {
        iram_size_t num_char = udi_cdc_multi_get_nb_received_data(port);
        while (num_char > 0) {
            num_char = (num_char > 512) ? 512 : num_char;
            udi_cdc_multi_read_buf(port, uart_buf, num_char);
            for (uint16_t i = 0; i < num_char; i++) {
                usart_driver_putchar(driver, uart_buf[i]);
            }
            num_char = udi_cdc_multi_get_nb_received_data(port);
        }
    }
}

void my_callback_config(uint8_t port, usb_cdc_line_coding_t * cfg)
{
	usart_driver *driver = get_nth_available_driver(port);

    if (driver->cdc_enabled) {
        uint32_t baud = cfg->dwDTERate;

        uint8_t stop_bits = ((uint32_t)cfg->bCharFormat) << 12;
        uint8_t dbits = ((uint32_t)cfg->bDataBits - 5) << 6;
        uint16_t parity_type = US_MR_PAR_NO;
        switch(cfg->bParityType) {
            case CDC_PAR_NONE:
            parity_type = US_MR_PAR_NO;
            break;
            case CDC_PAR_ODD:
            parity_type = US_MR_PAR_ODD;
            break;
            case CDC_PAR_EVEN:
            parity_type = US_MR_PAR_EVEN;
            break;
            case CDC_PAR_MARK:
            parity_type = US_MR_PAR_MARK;
            break;
            case CDC_PAR_SPACE:
            parity_type = US_MR_PAR_SPACE;
            break;
            default:
            return;
        }
		


        configure_usart(driver, baud, stop_bits, parity_type, dbits);
		if (!(usart_get_interrupt_mask(driver->usart) & UART_IER_RXRDY)) {
			usart_enable_rx(driver->usart);
			usart_enable_tx(driver->usart);

			usart_enable_interrupt(driver->usart, UART_IER_RXRDY);
		}
		
    }
}

bool usart_setup_out_received(void)
{
    switch(udd_g_ctrlreq.req.bRequest) {
    case REQ_USART0_CONFIG:
        udd_g_ctrlreq.callback = ctrl_usart_cb;
        return true;
        
    case REQ_USART0_DATA:
        udd_g_ctrlreq.callback = ctrl_usart_cb_data;
        return true;
    
#ifdef CW_TARGET_SPI
    /* Target SPI1 */
    case REQ_FPGASPI1_XFER:
        udd_g_ctrlreq.callback = ctrl_spi1util;
        return true;
#endif
#ifdef CW_PROG_XMEGA
    case REQ_XMEGA_PROGRAM:
        /*
        udd_g_ctrlreq.payload = xmegabuffer;
        udd_g_ctrlreq.payload_size = min(udd_g_ctrlreq.req.wLength,	sizeof(xmegabuffer));
        */
        udd_g_ctrlreq.callback = ctrl_xmega_program_void;
        return true;
#endif
#ifdef CW_PROG_AVR
		/* AVR Programming */
    case REQ_AVR_PROGRAM:
        udd_g_ctrlreq.callback = ctrl_avr_program_void;
        return true;
#endif
    case REQ_CDC_SETTINGS_EN:
        udd_g_ctrlreq.callback = naeusb_cdc_settings_out;
        return true;
        break;
    }
    return false;
}

bool usart_setup_in_received(void)
{
    switch(udd_g_ctrlreq.req.bRequest) {
    case REQ_USART0_CONFIG:
        return ctrl_usart_in();
        break;
        
    case REQ_USART0_DATA:						
        0;
        unsigned int cnt;
		usart_driver *driver = get_nth_available_driver(udd_g_ctrlreq.req.wValue >> 8);
		if (!driver) return false;
        unsigned int data = (udd_g_ctrlreq.req.wLength > 128) ? 128 : udd_g_ctrlreq.req.wLength;
        for(cnt = 0; cnt < data; cnt++){
            respbuf[cnt] = usart_driver_getchar(driver);
        }
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = cnt;
        return true;
        break;
		
#ifdef CW_TARGET_SPI
        case REQ_FPGASPI1_XFER:
            if (udd_g_ctrlreq.req.wLength > sizeof(spi1util_data_buffer))
            {
                return false;
            }
            udd_g_ctrlreq.payload = spi1util_data_buffer;
            udd_g_ctrlreq.payload_size = udd_g_ctrlreq.req.wLength;
            return true;
        break;
#endif

#ifdef CW_PROG_XMEGA
    case REQ_XMEGA_PROGRAM:
        return XPROGProtocol_Command();
        break;
#endif

#ifdef CW_PROG_AVR        
    case REQ_AVR_PROGRAM:
        return V2Protocol_ProcessCommand();
        break;
#endif
	
	case REQ_CDC_SETTINGS_EN:
        return naeusb_cdc_settings_in();
        break;
    }
    return false;
}

void cdc_send_to_pc(void)
{
    // if (!NAEUSB_CDC_IS_RUNNING) return; //fixes Pro streaming requiring connection to CDC
	for (uint8_t i = 0; i < 4; i++) {
		usart_driver *driver = get_nth_available_driver(i);
		if (!driver) continue;
		if (driver->cdc_enabled && driver->enabled && udi_cdc_multi_is_tx_ready(i)) {
			while (circ_buf_has_char(&driver->rx_cdc_buf)) {
				udi_cdc_multi_putc(i, get_from_circ_buf(&driver->rx_cdc_buf));
			}
		}

	}
	
}

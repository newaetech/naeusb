#include "naeusb_usart.h"
#include "naeusb_usart_dma.h"
#include "pdc.h"

#ifdef CW_USE_USART0
// TODO, probably need specific alignment, etc.
uint8_t USART0_TX_BUF[USART_TX_BUF_SZ];
uint8_t USART0_RX_BUF[USART_RX_BUF_SZ];
struct usart_state USART0_ST = {
    .pdc = PDC_USART0,
    .usart = USART0,
    .tx_st = {
        .buf_st_ptr = USART0_TX_BUF,
        .pdc = PDC_USART0,
        .usart = USART0,
        .is_tx = 1
    },
    .rx_st = {
        .buf_st_ptr = USART0_RX_BUF,
        .pdc = PDC_USART0,
        .usart = USART0,
        .is_tx = 0
    },
    .cdc_enabled = 0,
    .cdc_supported = 1,
    .xonxoff_enabled = 0,
    .currently_xoff = 0,
    .usart_id = 0
};

ISR(USART0_Handler)
{
	handle_rx_usart_overrun_int(&USART0_ST);

}

void usart0_enableIO(void)
{
	sysclk_enable_peripheral_clock(ID_USART0);
	#if (USB_DEVICE_PRODUCT_ID == 0xC310) || (USB_DEVICE_PRODUCT_ID == 0xC340)
    if (FPGA_PINS_ON) {
        gpio_configure_pin(PIN_USART0_RXD, PIN_USART0_RXD_FLAGS);
        gpio_configure_pin(PIN_USART0_TXD, PIN_USART0_TXD_FLAGS);
    }
    #else
        gpio_configure_pin(PIN_USART0_RXD, PIN_USART0_RXD_FLAGS);
        gpio_configure_pin(PIN_USART0_TXD, PIN_USART0_TXD_FLAGS);
	#endif
	irq_register_handler(USART0_IRQn, 3);
}

void usart0_tx_break(void)
{
	#if (USB_DEVICE_PRODUCT_ID == 0xC310) || (USB_DEVICE_PRODUCT_ID == 0xC340)
    if (FPGA_PINS_ON) {
        gpio_configure_pin(PIN_USART0_TXD, PIO_OUTPUT_0 | PIO_DEFAULT);
    }
    #endif
}

uint8_t usart0_is_io_low(void)
{
    uint8_t pin_setup = 0;
    pin_setup |= gpio_pin_is_low(PIN_USART0_RXD);
    pin_setup |= gpio_pin_is_low(PIN_USART0_TXD) << 1;
    return pin_setup;
}
#else
void usart0_enableIO(void)
{
	
}
void usart0_tx_break(void)
{

}
uint8_t usart0_is_io_low(void)
{

}
#endif

#ifdef CW_USE_USART1
uint8_t USART1_TX_BUF[USART_TX_BUF_SZ];
uint8_t USART1_RX_BUF[USART_RX_BUF_SZ];
struct usart_state USART0_ST = {
    .pdc = PDC_USART1,
    .usart = USART1,
    .tx_st = {
        .buf_st_ptr = USART1_TX_BUF,
        .pdc = PDC_USART1,
        .usart = USART1,
        .is_tx = 1
    },
    .rx_st = {
        .buf_st_ptr = USART1_RX_BUF,
        .pdc = PDC_USART1,
        .usart = USART1,
        .is_tx = 0
    },
    .cdc_enabled = 0,
    .cdc_supported = 1,
    .xonxoff_enabled = 0,
    .currently_xoff = 0,
    .usart_id = 0
};

ISR(USART1_Handler)
{
	handle_rx_usart_overrun_int(&USART1_ST);
}

void usart1_enableIO(void)
{
	sysclk_enable_peripheral_clock(ID_USART1);
	#if (USB_DEVICE_PRODUCT_ID == 0xC310) || (USB_DEVICE_PRODUCT_ID == 0xC340)
    if (FPGA_PINS_ON) {
        gpio_configure_pin(PIN_USART1_RXD, PIN_USART1_RXD_FLAGS);
        gpio_configure_pin(PIN_USART1_TXD, PIN_USART1_TXD_FLAGS);
    }
    #else
        gpio_configure_pin(PIN_USART1_RXD, PIN_USART1_RXD_FLAGS);
        gpio_configure_pin(PIN_USART1_TXD, PIN_USART1_TXD_FLAGS);
	#endif
}

void usart1_tx_break(void)
{
	#if (USB_DEVICE_PRODUCT_ID == 0xC310) || (USB_DEVICE_PRODUCT_ID == 0xC340)
    if (FPGA_PINS_ON) {
        gpio_configure_pin(PIN_USART1_TXD, PIO_OUTPUT_0 | PIO_DEFAULT);
    }
    #endif
}
uint8_t usart1_is_io_low(void)
{
    uint8_t pin_setup = 0;
    pin_setup |= gpio_pin_is_low(PIN_USART1_RXD);
    pin_setup |= gpio_pin_is_low(PIN_USART1_TXD) << 1;
    return pin_setup;
}
#else
void usart1_enableIO(void)
{
	
}
void usart1_tx_break(void)
{

}
uint8_t usart1_is_io_low(void)
{

}
#endif

#ifdef CW_USE_USART2
#ifndef USART2
#error "USART2 unavailable"
#endif
#else
void usart2_enableIO(void)
{
	
}
void usart2_tx_break(void)
{
	
}
uint8_t usart2_is_io_low(void)
{

}
#endif

#ifdef CW_USE_USART3
uint8_t USART3_TX_BUF[USART_TX_BUF_SZ];
uint8_t USART3_RX_BUF[USART_RX_BUF_SZ];
struct usart_state USART0_ST = {
    .pdc = PDC_USART3,
    .usart = USART3,
    .tx_st = {
        .buf_st_ptr = USART3_TX_BUF,
        .pdc = PDC_USART3,
        .usart = USART3,
        .is_tx = 1
    },
    .rx_st = {
        .buf_st_ptr = USART3_RX_BUF,
        .pdc = PDC_USART3,
        .usart = USART3,
        .is_tx = 0
    },
    .cdc_enabled = 0,
    .cdc_supported = 1,
    .xonxoff_enabled = 0,
    .currently_xoff = 0,
    .usart_id = 0
};

ISR(USART3_Handler)
{
	handle_rx_usart_overrun_int(&USART3_ST);
}
void usart3_enableIO(void)
{
	sysclk_enable_peripheral_clock(ID_USART3);
	#if (USB_DEVICE_PRODUCT_ID == 0xC310) || (USB_DEVICE_PRODUCT_ID == 0xC340)
    if (FPGA_PINS_ON) {
        gpio_configure_pin(PIN_USART3_RXD, PIN_USART3_RXD_FLAGS);
        gpio_configure_pin(PIN_USART3_TXD, PIN_USART3_TXD_FLAGS);
    }
    #else
        gpio_configure_pin(PIN_USART3_RXD, PIN_USART3_RXD_FLAGS);
        gpio_configure_pin(PIN_USART3_TXD, PIN_USART3_TXD_FLAGS);
	#endif
}

void usart3_tx_break(void)
{
	#if (USB_DEVICE_PRODUCT_ID == 0xC310) || (USB_DEVICE_PRODUCT_ID == 0xC340)
    if (FPGA_PINS_ON) {
        gpio_configure_pin(PIN_USART3_TXD, PIO_OUTPUT_0 | PIO_DEFAULT);
    }
    #endif
}
uint8_t usart3_is_io_low(void)
{
    uint8_t pin_setup = 0;
    pin_setup |= gpio_pin_is_low(PIN_USART3_RXD);
    pin_setup |= gpio_pin_is_low(PIN_USART3_TXD) << 1;
    return pin_setup;
}
#else
void usart3_enableIO(void)
{
	
}
void usart3_tx_break(void)
{

}
uint8_t usart3_is_io_low(void)
{

}
#endif

// Enable IO/ISR/Clock for a given driver
void usart_enableIO(struct usart_state *driver)
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

// Set TX pin low for a given driver
void usart_tx_break(struct usart_state *driver)
{
    if (driver->usart_id == 0) {
        usart0_tx_break();
    } else if (driver->usart_id == 1) {
        usart1_tx_break();
    } else if (driver->usart_id == 2) {
        usart2_tx_break();
    } else if (driver->usart_id == 3) {
        usart3_tx_break();
    }
}

uint8_t usart_is_io_low(struct usart_state *driver)
{
    if (driver->usart_id == 0) {
        return usart0_is_io_low();
    } else if (driver->usart_id == 1) {
        return usart1_is_io_low();
    } else if (driver->usart_id == 2) {
        return usart2_is_io_low();
    } else if (driver->usart_id == 3) {
        return usart3_is_io_low();
    }
    return 0;
}


// TODO: Fix these
void usart_handle_xoff(struct usart_state *driver)
{
    driver->currently_xoff = 1;
    // should do everything else next time it tries to send a char
    // if (usart_get_interrupt_mask(driver)) {

    // }
}

void usart_handle_xon(struct usart_state *driver)
{
    // disable xoff
    driver->currently_xoff = 0;

    // rerun ISR
    // don't think I need this, as it's called via the ISR
    // generic_isr(driver);
}

void usart_dummy_func(void)
{

}

struct usart_state *get_usart_from_id(int id)
{
    #ifdef CW_USE_USART0
        if (id == 0) return &USART0_ST;
    #endif
    #ifdef CW_USE_USART1
        if (id == 1) return &USART1_ST;
    #endif
    #ifdef CW_USE_USART2
        if (id == 2) return &USART2_ST;
    #endif
    #ifdef CW_USE_USART3
        if (id == 3) return &USART3_ST;
    #endif

    return NULL;
}

/*
Get the nth usart driver in use.

e.g. if we use USART1 and USART3, then port=0 returns driver 1
and port=1 returns driver 3
*/
struct usart_state *get_nth_available_driver(int port)
{
	struct usart_state *driver;
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

// Inverse of above function
int driver_to_port(struct usart_state *driver)
{
    struct usart_state *potential_driver = NULL;
    for (uint8_t i = 0; i < 4; i++) {
        if (get_nth_available_driver(i) == driver) {
            return i;
        }
    }
    return -1;
}


bool usart_fill_opts(sam_usart_opt_t *opts, uint32_t baud, uint8_t stop_bits, uint8_t parity, uint8_t dbits)
{
    opts->baudrate = baud;
    switch(stop_bits)
        {
        case 0:
            opts->stop_bits = US_MR_NBSTOP_1_BIT;
            break;
        case 1:
            opts->stop_bits = US_MR_NBSTOP_1_5_BIT;
            break;
        case 2:
            opts->stop_bits = US_MR_NBSTOP_2_BIT;
            break;
        default:
            opts->stop_bits = US_MR_NBSTOP_1_BIT;
        }

    /* Parity */
    switch(parity)
        {
        case 0:
            opts->parity_type = US_MR_PAR_NO;
            break;
        case 1:
            opts->parity_type = US_MR_PAR_ODD;
            break;
        case 2:
            opts->parity_type = US_MR_PAR_EVEN;
            break;
        case 3:
            opts->parity_type = US_MR_PAR_MARK;
            break;
        case 4:
            opts->parity_type = US_MR_PAR_SPACE;
            break;							
        default:
            opts->parity_type = US_MR_PAR_NO;
        }

    /* Data Bits */
    switch(dbits)
        {
        case 5:
            opts->char_length = US_MR_CHRL_5_BIT;
            break;
        case 6:
            opts->char_length = US_MR_CHRL_6_BIT;
            break;
        case 7:
            opts->char_length = US_MR_CHRL_7_BIT;
            break;
        case 8:							
        default:
            opts->char_length = US_MR_CHRL_8_BIT;
        }
        
    // Not loopback or anything weird, for example
    opts->channel_mode = US_MR_CHMODE_NORMAL;
    return true;
}


bool naeusb_cdc_handle_send_break(uint8_t port, uint16_t wValue)
{
    struct usart_state *driver = get_nth_available_driver(port);
    if (!driver) return false;

    switch(wValue) {
        case 0xFFFF:
            usart_tx_break(driver);
            return true;
        case 0x0000:
            usart_enableIO(driver);
            return true;
        default:
        return true;
    }
}
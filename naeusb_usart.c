#include <asf.h>
#include "circbuffer.h"
#include "usart_driver.h"
#include "naeusb_default.h"
#include "usart.h"
#include "usb_protocol_cdc.h"
#include "naeusb_usart.h"
#include "naeusb_usart_dma.h"
#include "naeusb_usart_util.h"

#ifdef CW_TARGET_SPI
#include "naeusb_usart_spi.h"
#endif


#define USART_WVREQ_INIT    0x0010
#define USART_WVREQ_ENABLE  0x0011
#define USART_WVREQ_DISABLE 0x0012
#define USART_WVREQ_NUMWAIT 0x0014
#define USART_WVREQ_NUMWAIT_TX 0x0018
#define USART_WVREQ_XONXOFF 0x0020

#define USART_XON 0x11
#define USART_XOFF 0x13

bool naeusb_cdc_settings_in(void)
{
    for (uint8_t i = 0; i < 4; i++) {
        struct usart_state *driver = get_nth_available_driver(i);
		respbuf[i] = 0;
        if (driver)
            respbuf[i] = driver->cdc_enabled;
    }
    udd_g_ctrlreq.payload = respbuf;
    udd_g_ctrlreq.payload_size = min(4, udd_g_ctrlreq.req.wLength);
    return true;

}

#ifdef CW_PROG_XMEGA
#include "XPROGNewAE.h"
void ctrl_xmega_program_void(void)
{
	XPROGProtocol_Command();
}
#endif

#ifdef CW_PROG_AVR
#include "V2Protocol.h"
void ctrl_avr_program_void(void)
{
	V2Protocol_ProcessCommand();
}
#endif

// Handle USART control IN messages (SAM->PC)
bool ctrl_usart_in(void)
{
    struct usart_state *driver = get_nth_available_driver(udd_g_ctrlreq.req.wValue >> 8);
    if (!driver)
        return false;
    uint32_t cnt;

    if (!driver) return false;

    switch (udd_g_ctrlreq.req.wValue & 0xFF) {
    case USART_WVREQ_INIT:
        return true;
        
    // number of available RX characters to send to PC
    case USART_WVREQ_NUMWAIT:
        if (udd_g_ctrlreq.req.wLength < 4) {
            return false;
        }
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 4;
        // driver->rxbuf.dropped = 0; //clear dropped characters

        cnt = usart_inwait(&driver->rx_st);
        if (driver->rx_st.dropped > 0) {
            cnt |= (1 << 16);
        }
        word2buf(respbuf, cnt);
        // CURRENT_ERRORS &= ~CW_ERR_USART_RX_OVERFLOW;
        return true;
    
    // number of TX characters we haven't sent yet
    case USART_WVREQ_NUMWAIT_TX:
        if (udd_g_ctrlreq.req.wLength < 4) {
            return false;
        }
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 4;
        cnt = usart_inwait(&driver->tx_st);
        // driver->txbuf.dropped = 0; //clear dropped characters
        word2buf(respbuf, cnt);
        // CURRENT_ERRORS &= ~CW_ERR_USART_TX_OVERFLOW;
        return true;
    case USART_WVREQ_XONXOFF:
        if (udd_g_ctrlreq.req.wLength < 1) {
            return false;
        }
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 1;
        if (!driver->xonxoff_enabled) driver->currently_xoff = 0;
        respbuf[0] = driver->xonxoff_enabled | (driver->currently_xoff << 1);
        return true;
    }

    return false;
}
// Handle USB IN

bool usart_setup_in_received(void)
{
    switch(udd_g_ctrlreq.req.bRequest) {
    case REQ_USART0_CONFIG:
        return ctrl_usart_in();
        break;
        
    case REQ_USART0_DATA:						
        0;
        unsigned int cnt;
		struct usart_state *driver = get_nth_available_driver(udd_g_ctrlreq.req.wValue >> 8);
		if (!driver) return false;
        unsigned int data = (udd_g_ctrlreq.req.wLength > 128) ? 128 : udd_g_ctrlreq.req.wLength;
        uint16_t read_len = 0;
        read_len = usart_dma_read(driver, respbuf, data);

        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = read_len;
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
        return true;
        break;
    }
    return false;
}


void ctrl_usart_cb(void)
{
    struct usart_state *driver = get_nth_available_driver(udd_g_ctrlreq.req.wValue >> 8);
    if (!driver) return;

    uint32_t baud;

    // Init USART with baud, other settings
    switch (udd_g_ctrlreq.req.wValue & 0xFF) {
    case USART_WVREQ_INIT:
        if (udd_g_ctrlreq.req.wLength != 7) return;

        buf2word(baud, udd_g_ctrlreq.payload);
		usart_enableIO(driver);

        usart_fill_opts(&driver->opts, baud, udd_g_ctrlreq.payload[4], 
        udd_g_ctrlreq.payload[5], udd_g_ctrlreq.payload[6]);
        setup_usart_dma(driver);

        return ;

    // Enable USART
    case USART_WVREQ_ENABLE:
		usart_enableIO(driver);
        usart_enable_rx(driver->usart);
        usart_enable_tx(driver->usart);

        usart_enable_dma(driver);
        
		return;

    // Disable USART
    case USART_WVREQ_DISABLE:
        usart_disable_rx(driver->usart);
        usart_disable_tx(driver->usart);
        usart_disable_dma(driver);
        return ;
    case USART_WVREQ_XONXOFF:
        driver->xonxoff_enabled = udd_g_ctrlreq.payload[0];
        if (!driver->xonxoff_enabled) driver->currently_xoff = 0;
        return;
    }
}

void ctrl_usart_cb_data(void)
{
	struct usart_state *driver = get_nth_available_driver(udd_g_ctrlreq.req.wValue >> 8);
    if (!driver) return;

	//Catch heartbleed-style error
	if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
		return;
	}

    uint16_t send_len = usart_dma_write(driver, udd_g_ctrlreq.payload, udd_g_ctrlreq.req.wLength);
    if (send_len < udd_g_ctrlreq.req.wLength) {
        CURRENT_ERRORS |= CW_ERR_USART_TX_OVERFLOW;
    }
}


// Handle USB OUT
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
        // udd_g_ctrlreq.callback = naeusb_cdc_settings_out;
        return true;
        break;
    }
    return false;
}
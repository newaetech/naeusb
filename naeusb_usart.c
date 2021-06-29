#include "naeusb_usart.h"
#include "usart_driver.h"
#if USB_DEVICE_PRODUCT_ID != 0xACE0
#include "V2Protocol.h"
#include "XPROGNewAE.h"
#endif
#include "conf_usb.h"

static void ctrl_usart_cb(void)
{
	ctrl_usart(USART_TARGET, false);
}

static void ctrl_usart_cb_data(void)
{		
	//Catch heartbleed-style error
	if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
		return;
	}
	
	for (int i = 0; i < udd_g_ctrlreq.req.wLength; i++){
		usart_driver_putchar(USART_TARGET, NULL, udd_g_ctrlreq.payload[i]);
	}
}
#if USB_DEVICE_PRODUCT_ID != 0xACE0
void ctrl_xmega_program_void(void)
{
	XPROGProtocol_Command();
}

void ctrl_avr_program_void(void)
{
	V2Protocol_ProcessCommand();
}
#endif
bool usart_setup_out_received(void)
{
    switch(udd_g_ctrlreq.req.bRequest) {
    case REQ_USART0_CONFIG:
        udd_g_ctrlreq.callback = ctrl_usart_cb;
        return true;
        
    case REQ_USART0_DATA:
        udd_g_ctrlreq.callback = ctrl_usart_cb_data;
        return true;
#if USB_DEVICE_PRODUCT_ID != 0xACE0
    case REQ_XMEGA_PROGRAM:
        /*
        udd_g_ctrlreq.payload = xmegabuffer;
        udd_g_ctrlreq.payload_size = min(udd_g_ctrlreq.req.wLength,	sizeof(xmegabuffer));
        */
        udd_g_ctrlreq.callback = ctrl_xmega_program_void;
        return true;

		/* AVR Programming */
    case REQ_AVR_PROGRAM:
        udd_g_ctrlreq.callback = ctrl_avr_program_void;
        return true;
#endif
    }
    return false;
}

bool usart_setup_in_received(void)
{
    switch(udd_g_ctrlreq.req.bRequest) {
    case REQ_USART0_CONFIG:
        return ctrl_usart(USART_TARGET, true);
        break;
        
    case REQ_USART0_DATA:						
        0;
        unsigned int cnt;
        for(cnt = 0; cnt < udd_g_ctrlreq.req.wLength; cnt++){
            respbuf[cnt] = usart_driver_getchar(USART_TARGET);
        }
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = cnt;
        return true;
        break;
		
	#if USB_DEVICE_PRODUCT_ID != 0xACE0
    case REQ_XMEGA_PROGRAM:
        return XPROGProtocol_Command();
        break;
        
    case REQ_AVR_PROGRAM:
        return V2Protocol_ProcessCommand();
        break;
	#endif
    }
    return false;
}

// naeusart because I wouldn't be surpised if usart_register_handlers collides with something
void naeusart_register_handlers(void)
{
    naeusb_add_in_handler(usart_setup_in_received);
    naeusb_add_out_handler(usart_setup_out_received);
}
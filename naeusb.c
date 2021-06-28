#include "naeusb.h"
static usb_request_handle_func naeusb_in_request_handlers[NAEUSB_MAX_HANDLERS] = {NULL};
static uint8_t naeusb_num_in_handlers = 0;

static usb_request_handle_func naeusb_out_request_handlers[NAEUSB_MAX_HANDLERS] = {NULL};
static uint8_t naeusb_num_out_handlers = 0;

COMPILER_WORD_ALIGNED
uint8_t main_buf_loopback[MAIN_LOOPBACK_SIZE];

uint8_t ctrl_respbuf[64];

bool naeusb_add_in_handler(usb_requst_handle_func new_handler)
{
    if (naeusb_num_in_handlers >= 16)
        return false;
    
    naeusb_in_request_handlers[naeusb_num_in_handlers++] = new_handler;
    return true;
}

bool naeusb_add_out_handler(usb_requst_handle_func new_handler)
{
    if (naeusb_num_out_handlers >= 16)
        return false;
    
    naeusb_out_request_handlers[naeusb_num_out_handlers++] = new_handler;
    return true;
}

//this stuff just turns leds on and off
void main_suspend_action(void)
{
	active = false;
	ui_powerdown();
}

void main_resume_action(void)
{
    ui_wakeup();
}

void main_sof_action(void)
{
    if (!main_b_vendor_enable)
        return;
    ui_process(udd_get_frame_number());
}
volatile uint8_t started_read = 0;

bool main_vendor_enable(void)
{
    active = true;
    main_b_vendor_enable = true;
    // Start data reception on OUT endpoints
#if UDI_VENDOR_EPS_SIZE_BULK_FS
    //main_vendor_bulk_in_received(UDD_EP_TRANSFER_OK, 0, 0);
    udi_vendor_bulk_out_run(
        main_buf_loopback,
        sizeof(main_buf_loopback),
        main_vendor_bulk_out_received);
#endif
    return true;
}

void main_vendor_disable(void)
{
    main_b_vendor_enable = false;
}

bool main_setup_out_received(void)
{
    bool handler_status = false;
    udd_g_ctrlreq.payload = ctrl_respbuf;
    udd_g_ctrlreq.payload_size = min(udd_g_ctrlreq.req.wLength,	sizeof(ctrl_respbuf));

    for (uint8_t i = naeusb_num_out_handlers; i > 0; i--) {
        handler_status = naeusb_out_request_handlers[i-1]();
        if (handler_status == true) {
            return true;
        }
    }

    return false;

}

bool main_setup_in_received(void)
{
    bool handler_status = false;

    for (uint8_t i = naeusb_num_in_handlers; i > 0; i--) {
        handler_status = naeusb_in_request_handlers[i-1]();
        if (handler_status == true) {
            return true;
        }
    }

    return false;

}
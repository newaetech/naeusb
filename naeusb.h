#include <asf.h>
#pragma once
#define NAEUSB_MAX_HANDLERS 16

typedef (bool)(*usb_request_handle_func)(void);

bool naeusb_add_in_handler(usb_requst_handle_func new_handler);
bool naeusb_add_out_handler(usb_requst_handle_func new_handler);

extern uint8_t ctrl_respbuf[64];

static volatile bool main_b_vendor_enable = true;


void main_suspend_action(void);
void main_resume_action(void);
void main_sof_action(void);
bool main_vendor_enable(void);
void main_vendor_disable(void);
bool main_setup_out_received(void);
bool main_setup_in_received(void);
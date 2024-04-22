#pragma once
#include <asf.h>
#include "usb_xmem.h"
#include "conf_usb.h"
#include "usb.h"
#define NAEUSB_MAX_HANDLERS 16
#define CTRLBUFFER_WORDPTR ((uint32_t *) ((void *)ctrlbuffer))

typedef bool (*usb_request_handle_func)(void);

typedef bool (*usb_bulk_handle_func)(udd_ep_status_t, iram_size_t, udd_ep_id_t);

bool naeusb_add_in_handler(usb_request_handle_func new_handler);
bool naeusb_add_out_handler(usb_request_handle_func new_handler);

// NOTE: bulk in handler doesn't make any sense
bool naeusb_add_bulk_out_handler(usb_bulk_handle_func);

extern COMPILER_WORD_ALIGNED uint8_t ctrlbuffer[128];
extern COMPILER_WORD_ALIGNED uint8_t respbuf[128];

COMPILER_WORD_ALIGNED
extern uint8_t main_buf_loopback[MAIN_LOOPBACK_SIZE];

void main_suspend_action(void);
void main_resume_action(void);
void main_sof_action(void);
bool main_vendor_enable(void);
void main_vendor_disable(void);
bool main_setup_out_received(void);
bool main_setup_in_received(void);
bool usb_is_enabled(void);
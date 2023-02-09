#pragma once
#include "naeusb.h"
#include "fpga_program.h"

#define REQ_MEMREAD_BULK 0x10
#define REQ_MEMWRITE_BULK 0x11
#define REQ_MEMREAD_CTRL 0x12
#define REQ_MEMWRITE_CTRL 0x13

#define REQ_FPGA_STATUS 0x15
#define REQ_FPGA_PROGRAM 0x16

#define REQ_FPGA_RESET 0x25

void openadc_register_handlers(void);

void main_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);
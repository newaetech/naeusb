#pragma once
#include "naeusb.h"
#include "fpga_program.h"

// Ctrl Write
#define REQ_MEMREAD_BULK 0x10 // Setup FPGA bulk read. wValue unused. Bytes 0-3 = buflen, Bytes 4-7 = address
#define REQ_MEMWRITE_BULK 0x11 // Setup FPGA bulk write. wValue unused. Bytes 0-3 = reserved, Bytes 4-7 = address
#define REQ_MEMREAD_CTRL 0x12 // FPGA read. wValue unused. Bytes 0-3 = buflen, Bytes 4-7 = address
#define REQ_MEMWRITE_CTRL 0x13 // FPGA write. wValue unused. Bytes 0-3 = buflen, Bytes 4-7 = address

#define REQ_FPGA_PROGRAM 0x16 // Change FPGA program mode/settings. wValue = see below. Data = see below
// wValue & 0xFF:
    #define FPGA_PROGRAM_SETUP 0xA0 // Setup device for FPGA program and set FPGA erase pin. Bytes 0-3 = programming speed in Hz
    #define FPGA_PROGRAM_READY 0xA1 // Unset FPGA erase pin and setup bulk write to send to FPGA
    #define FPGA_PROGRAM_FINISH 0xA2 // Set bulk write back to normal
    #define SPI_PASSTHRU_SETUP 0xB0 // Setup device for SPI FPGA passthrough program
    #define SPI_PASSTHRU_READY 0xB1 // Setup bulk write to send to FPGA
    #define SPI_PASSTHRU_FINISH 0xB2 // Set bulk write back to normal

// Ctrl Read
#define REQ_MEMREAD_CTRL 0x12 // Transfer data read by REQ_MEMREAD_CTRL write command (see above)
#define REQ_FPGA_STATUS 0x15 // Byte 0 = 1 if FPGA programmed, 0 otherwise. Bytes 1-3 = Reserved

#define REQ_FPGA_RESET 0x25 // Reserved

void openadc_register_handlers(void);

void main_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);
void main_vendor_bulk_out_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);
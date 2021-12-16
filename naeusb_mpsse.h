#pragma once
#include "naeusb.h"

void mpsse_register_handlers(void);
void MPSSE_main_sendrecv_byte(void);
uint8_t mpsse_enabled(void);

// extern uint8_t MPSSE_ENABLED;

enum FTDI_CMD_BITS {
    FTDI_NEG_CLK_WRITE  = 1 << 0, // output data on negative clock edge
    FTDI_BIT_MODE       = 1 << 1, // send 1-8 bytes
    FTDI_NEG_CLK_READ   = 1 << 2, // read data on negative clock edge
    FTDI_LITTLE_ENDIAN  = 1 << 3, // bytes data are in little endian format
    FTDI_WRITE_TDI      = 1 << 4, // do a write on TDI
    FTDI_READ_TDO       = 1 << 5, // read data from TDO
    FTDI_WRITE_TMS      = 1 << 6, // do a write on TMS (bit mode only, 1/7 bits on TDO)
    FTDI_SPECIAL_CMD    = 1 << 7, // special commands
};

enum FTDI_SPECIAL_CMDS {
    FTDI_SET_OPLB = 0x80, // configure IO pins 0-7
    FTDI_READ_IPLB,       // read IO pins values 0-7
    FTDI_SET_OPHB,        // configure IO pins 8-11
    FTDI_READ_IPHB,       // read IO pins 8-11
    FTDI_EN_LOOPBACK,     // enable loopback
    FTDI_DIS_LOOPBACK,    // disable loopback
    FTDI_DIS_CLK_DIV_5 = 0x8A, // ignored
    FTDI_EN_CLK_DIV_5 = 0x8A,  // ignored
    FTDI_SEND_IMM=0x87,        // send data back to PC (hacky implementation due to small buffer, depends on async USB from openocd)
    FTDI_ADT_CLK_ON=0x96,      // adaptive clock for ARM (ignored)
    FTDI_ADT_CLK_OFF           // ignored
};

// control USB requests, can mostly ignore
#define BITMODE_MPSSE 0x02

#define SIO_RESET_REQUEST             0x00
#define SIO_SET_LATENCY_TIMER_REQUEST 0x09
#define SIO_GET_LATENCY_TIMER_REQUEST 0x0A
#define SIO_SET_BITMODE_REQUEST       0x0B

#define SIO_RESET_SIO 0
#define SIO_RESET_PURGE_RX 1
#define SIO_RESET_PURGE_TX 2
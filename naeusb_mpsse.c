#include "naeusb_default.h"
#include <string.h>

#define uint8_t unsigned char

// used to switch USB configuration from CDC to MPSSE
extern void switch_configurations(void);

// static uint16_t MPSSE_TX_IDX = 0;
static uint8_t MPSSE_CUR_CMD = 0; // the current MPSSE command
static int16_t MPSSE_TX_IDX = 0; // the current TX buffer index
static int16_t MPSSE_TX_BYTES = 0; // the number of bytes in the tx buffer
static int16_t MPSSE_RX_BYTES = 2; // the number of bytes in the rx buffer NOTE: 2 bytes reserved for status
static int16_t MPSSE_TRANSMISSION_LEN = 0; // the number of bits/bytes in the current data transmission

static uint8_t MPSSE_LOOPBACK_ENABLED = 0; // connect TDI to TDO for loopback

//need lock to make sure we don't accept another transaction until we're done sending data back
//should be fine since  it looks like openocd just keeps retrying transaction until it works
static uint8_t MPSSE_TRANSACTION_LOCK = 1;  //Currently sending data back to the PC

static uint32_t NUM_PROCESSED_CMDS = 0; // the number of commands processed for debug purposes
static uint8_t MPSSE_TX_REQ = 0; // a command needs more bytes than currently available before it can be processed
static uint8_t MPSSE_FIRST_BIT = 0; // First bit handled differently for some reason?

static uint8_t MPSSE_ENABLED = 0;
// static uint8_t MPSSE_COMMAND_IDX = 0; //debug variable for command history

static uint8_t MPSSE_SCK_IDLE_LEVEL = 0; // sck can idle high or low

// static uint32_t NUM_BYTE_SINCE_SEND_IMM = 0; // debug variable


// holds data to send back to PC
COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_RX_BUFFER[64] __attribute__ ((__section__(".mpssemem")));
// COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_RX_CTRL_BUFFER[64] __attribute__ ((__section__(".mpssemem"))); //TODO: implement this

// holds data received from PC
COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_TX_BUFFER[80] __attribute__ ((__section__(".mpssemem"))) = {0};

// holds data receieved from PC when a command overlaps between reads
COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_TX_BUFFER_BAK[64] __attribute__ ((__section__(".mpssemem"))) = {0};

// debug variable for command history
// COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_COMMAND_HIST[256] __attribute__ ((__section__(".mpssemem"))) = {0};

void mpsse_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);
void mpsse_vendor_bulk_out_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);

// control USB requests, can mostly ignore
#define BITMODE_MPSSE 0x02

#define SIO_RESET_REQUEST             0x00
#define SIO_SET_LATENCY_TIMER_REQUEST 0x09
#define SIO_GET_LATENCY_TIMER_REQUEST 0x0A
#define SIO_SET_BITMODE_REQUEST       0x0B

#define SIO_RESET_SIO 0
#define SIO_RESET_PURGE_RX 1
#define SIO_RESET_PURGE_TX 2

#define ADDR_IOROUTE 55 // FPGA IO addr for setting NRST (should remove?)

// NOTE: DOUT is TDI and DIN is TDO
#if AVRISP_USEUART
#ifndef MPSSE_DOUT_GPIO
    #define MPSSE_DOUT_GPIO AVRISP_MOSI_GPIO
#endif

#ifndef MPSSE_DIN_GPIO
    #define MPSSE_DIN_GPIO  AVRISP_MISO_GPIO
#endif

#ifndef MPSSE_SCK_GPIO
    #define MPSSE_SCK_GPIO  AVRISP_SCK_GPIO
#endif
#else
#ifndef MPSSE_DOUT_GPIO
    #define MPSSE_DOUT_GPIO SPI_MOSI_GPIO
#endif
#ifndef MPSSE_DIN_GPIO
    #define MPSSE_DIN_GPIO SPI_MISO_GPIO
#endif
#ifndef MPSSE_SCK_GPIO
    #define MPSSE_SCK_GPIO SPI_SPCK_GPIO
#endif
#endif

// use PDIC/D for TMS/TRST
#ifndef MPSSE_TMS_GPIO
    #define MPSSE_TMS_GPIO PIN_PDIC_GPIO
#endif

#ifndef MPSSE_TRST_GPIO
    #define MPSSE_TRST_GPIO PIN_PDIDTX_GPIO
#endif

// pins used for MPSSE IO
static uint32_t MPSSE_PINS_GPIO[8] = {
    MPSSE_SCK_GPIO,
    MPSSE_DIN_GPIO,
    MPSSE_DOUT_GPIO,
    MPSSE_TMS_GPIO,
    MPSSE_TRST_GPIO
};

/*
Gets remaining available space in the RX buffer
*/
static int16_t mpsse_rx_buffer_remaining(void)
{
    return sizeof(MPSSE_RX_BUFFER) - MPSSE_RX_BYTES;
}

/*
Gets the number of unprocessed TX bytes in the buffer
*/
static int16_t mpsse_tx_buffer_remaining(void)
{
    return MPSSE_TX_BYTES - MPSSE_TX_IDX;
}

bool mpsse_setup_out_received(void)
{
    /*
    Intercept SAM_CFG command for configuration switch
    */

    uint8_t wValue = udd_g_ctrlreq.req.wValue & 0xFF;
    if ((wValue == 0x42) && (udd_g_ctrlreq.req.bRequest == REQ_SAM_CFG)) {
        //disconnect + stop USB
        udc_stop();
        //change interface pointers so that the second one points to the MPSSE vendor interface
        switch_configurations(); 
        //restart USB
        udc_start();
        return true;
    }

    if ((udd_g_ctrlreq.req.wIndex != 0x01) && (udd_g_ctrlreq.req.wIndex != 0x02)) {
        return false;
    }

    /*
    There's lots of FTDI commands for setting MPSSE mode, clearing buffers, etc
    but we can get away with just setting this stuff up when a reset request is done
    */
    if ((udd_g_ctrlreq.req.bRequest == SIO_RESET_REQUEST)) {
        // clear all buffers
        memset(MPSSE_RX_BUFFER, 0, sizeof(MPSSE_RX_BUFFER));
        // memset(MPSSE_RX_CTRL_BUFFER, 0, sizeof(MPSSE_RX_CTRL_BUFFER));
        memset(MPSSE_TX_BUFFER, 0, sizeof(MPSSE_TX_BUFFER_BAK));
        memset(MPSSE_TX_BUFFER_BAK, 0, sizeof(MPSSE_TX_BUFFER_BAK));

        // abort any transactions going on on these endpoints
        udd_ep_abort(UDI_MPSSE_EP_BULK_OUT);
        udd_ep_abort(UDI_MPSSE_EP_BULK_IN);
        gpio_configure_pin(MPSSE_DIN_GPIO, PIO_DEFAULT | PIO_TYPE_PIO_INPUT);
        gpio_configure_pin(MPSSE_DOUT_GPIO, PIO_OUTPUT_0);
        gpio_configure_pin(MPSSE_SCK_GPIO, PIO_OUTPUT_0);
        gpio_configure_pin(MPSSE_TMS_GPIO, PIO_OUTPUT_0);
		MPSSE_ENABLED = 1;
        MPSSE_TRANSACTION_LOCK = 1;
        NUM_PROCESSED_CMDS = 0;

        // start looking for data from openocd
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);
    }
    return true;
}

bool mpsse_setup_in_received(void)
{
    // don't handle if not sent to our interface
    if (udd_g_ctrlreq.req.wIndex != 0x01) {
        return false;
    }

    // For debug, reads a bunch of internal variables back. TODO: change to using a separate buf
    if (udd_g_ctrlreq.req.bRequest == 0xA0) {
        MPSSE_RX_BUFFER[0] = MPSSE_CUR_CMD;
        MPSSE_RX_BUFFER[1] = MPSSE_TX_IDX & 0xFF;
        MPSSE_RX_BUFFER[2] = MPSSE_TX_BYTES & 0xFF;
        MPSSE_RX_BUFFER[3] = MPSSE_RX_BYTES & 0xFF;
        MPSSE_RX_BUFFER[4] = MPSSE_TRANSMISSION_LEN & 0xFF;
        MPSSE_RX_BUFFER[5] = MPSSE_TRANSACTION_LOCK;
        MPSSE_RX_BUFFER[6] = NUM_PROCESSED_CMDS & 0xFF;
        MPSSE_RX_BUFFER[7] = (NUM_PROCESSED_CMDS >> 8) & 0xFF;
        MPSSE_RX_BUFFER[8] = (NUM_PROCESSED_CMDS >> 16) & 0xFF;
        MPSSE_RX_BUFFER[9] = (NUM_PROCESSED_CMDS >> 24) & 0xFF;
        MPSSE_RX_BUFFER[10] = MPSSE_LOOPBACK_ENABLED;
        udd_g_ctrlreq.payload = MPSSE_RX_BUFFER;
        udd_g_ctrlreq.payload_size = 11;
        return true;
    }

    // Debug commands for reading from internal buffers
    uint16_t wValue = udd_g_ctrlreq.req.wValue;
    if (udd_g_ctrlreq.req.bRequest == 0xA1) {
        uint32_t addr = (uint32_t)(MPSSE_RX_BUFFER + wValue);
        addr &= ~(0b11);
        udd_g_ctrlreq.payload = (void *) addr;
        udd_g_ctrlreq.payload_size = 256;
    }
    if (udd_g_ctrlreq.req.bRequest == 0xA2) {
        uint32_t addr = (uint32_t)(MPSSE_TX_BUFFER + wValue);
        addr &= ~(0b11);
        udd_g_ctrlreq.payload = (void *) addr;
        udd_g_ctrlreq.payload_size = 256;
    }
    return true;
}


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

#define DOUT_NMATCH_SCK ((gpio_pin_is_low(MPSSE_SCK_GPIO) ? \
                    !!(MPSSE_CUR_CMD & FTDI_NEG_CLK_WRITE) : \
                    !(MPSSE_CUR_CMD & FTDI_NEG_CLK_WRITE)))

#define DIN_NMATCH_SCK ((gpio_pin_is_low(MPSSE_SCK_GPIO) ? \
                    !!(MPSSE_CUR_CMD & FTDI_NEG_CLK_READ) : \
                    !(MPSSE_CUR_CMD & FTDI_NEG_CLK_READ)))

// send one bit over TDI and read one bit on TDO
// TODO: reimplement handling for different NEG_CLK configs
uint8_t mpsse_send_bit(uint8_t value)
{
    value &= 0x01;
    uint8_t read_value = 0;
    uint32_t dpin;
    if (MPSSE_LOOPBACK_ENABLED) {
        return value & 0x01;
    }

        // actually do write
    dpin = MPSSE_DOUT_GPIO;

    if (1) { //Clock out data on IDLE->NON_IDLE
        if (value) {
            gpio_set_pin_high(dpin);
        } else {
            gpio_set_pin_low(dpin);
        }
    }
    gpio_toggle_pin(MPSSE_SCK_GPIO);

    if (0) { //Clock out data on NON_IDLE->IDLE
        if (value) {
            gpio_set_pin_high(dpin);
        } else {
            gpio_set_pin_low(dpin);
        }
    }

    if (1) { //read data on IDLE->NON_IDLE
        read_value = gpio_pin_is_high(MPSSE_DIN_GPIO);
    }



    gpio_toggle_pin(MPSSE_SCK_GPIO);

    if (0) { //read data on NON_IDLE->IDLE
        read_value = gpio_pin_is_high(MPSSE_DIN_GPIO);
    }
    
    return read_value & 0x01;
}

// send up to 8 bits over TDI/TDO
uint8_t mpsse_send_bits(uint8_t value, uint8_t num_bits)
{
    // if (num_bits > 8) num_bits = 8;
    uint8_t read_value = 0;
    for (uint8_t i = 0; i < num_bits; i++) {
        if (MPSSE_CUR_CMD & FTDI_LITTLE_ENDIAN) {
            // NOTE: for little endian, bits are written to bit 7, then shifted down
            read_value >>= 1;
            read_value |= mpsse_send_bit((value >> i) & 0x01) << 7;
        } else {
            read_value |= mpsse_send_bit((value >> (7 - i)) & 0x01) << (7 - i);
        }
    }
    return read_value;
}

// send 1 byte over TDI/TDO
uint8_t mpsse_send_byte(uint8_t value)
{
    return mpsse_send_bits(value, 8);
}

// send 1 bit over TDI/TMS
uint8_t mpsse_tms_bit_send(uint8_t value)
{
    value &= 0x01;
    uint8_t read_value = 0;
    uint32_t dpin;

        // actually do write
    dpin = MPSSE_TMS_GPIO;

    if (1) { //Clock out data on IDLE->NON_IDLE
        if (value) {
            gpio_set_pin_high(dpin);
        } else {
            gpio_set_pin_low(dpin);
        }
    }
    gpio_toggle_pin(MPSSE_SCK_GPIO);

    if (1) { //read data on IDLE->NON_IDLE
        read_value = gpio_pin_is_high(MPSSE_DIN_GPIO);
    }

    if (0) { //Clock out data on NON_IDLE->IDLE
        if (value) {
            gpio_set_pin_high(dpin);
        } else {
            gpio_set_pin_low(dpin);
        }
    }

    gpio_toggle_pin(MPSSE_SCK_GPIO);

    if (0) { //read data on NON_IDLE->IDLE
        read_value = gpio_pin_is_high(MPSSE_DIN_GPIO);
    }
    
    return read_value & 0x01;

}

// send up to 7 bits ? should be up to 8? unspecified by mpsse
uint8_t mpsse_tms_send(uint8_t value, uint8_t num_bits)
{
    uint8_t read_value = 0;
    uint8_t i = 0;

    // one bit is clocked output on TDI for some reason if there's 7 bits written
    if (num_bits == 7) {
        uint8_t bitval = 0;
        bitval = (value >> 7) & 0x01;
        if (bitval) {
            gpio_set_pin_high(MPSSE_DOUT_GPIO);
        } else {
            gpio_set_pin_low(MPSSE_DOUT_GPIO);
        }
        i++;
    }

    // send the rest of the bits out on TMS
    // TODO: should this work like the normal rx with little endian?
    for (; i < num_bits; i++) {
        if (MPSSE_CUR_CMD & FTDI_LITTLE_ENDIAN) {
            read_value |= mpsse_tms_bit_send((value >> i) & 0x01) << i;
        } else {
            read_value |= mpsse_tms_bit_send((value >> (7 - i)) & 0x01) << (7 - i);
        }

    }
	return read_value;
}

void mpsse_handle_transmission(void)
{
    uint8_t read_val = 0;
    if (MPSSE_TRANSMISSION_LEN == 0) { 
        // not currently doing a transmission
        /*
        Always need at least 2 more bytes here
        For byte tx/rx, both used for tx/rx len
        For bit tx/rx, first is num bits, second is data
        */
        if (mpsse_tx_buffer_remaining() < 2) {
            MPSSE_TX_REQ = 1;
            return; //need more data
        }

        // read length in
        MPSSE_TRANSMISSION_LEN = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];

        if (!(MPSSE_CUR_CMD & FTDI_BIT_MODE)) {
            // clock in high byte of length if in byte mode
            MPSSE_TRANSMISSION_LEN |= MPSSE_TX_BUFFER[MPSSE_TX_IDX++] << 8;
            MPSSE_FIRST_BIT = 1;
            MPSSE_TRANSMISSION_LEN++; //0x00 sends 1 byte, 0x01 sends 2, etc
        } else {

            // take care of bit transmission right now
            MPSSE_TRANSMISSION_LEN++; //0x00 sends 1 bit
            if (MPSSE_CUR_CMD & FTDI_WRITE_TMS) {
                read_val = mpsse_tms_send(MPSSE_TX_BUFFER[MPSSE_TX_IDX++], MPSSE_TRANSMISSION_LEN);
                MPSSE_TRANSMISSION_LEN = 0;
            } else {
                // need TDI valid before we start clocking
                uint8_t value = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
				uint8_t bit;
                if (MPSSE_CUR_CMD & FTDI_LITTLE_ENDIAN) {
                    bit = value & 0x01;
                } else {
                    bit = value & 0x80;
                }

                if (bit) {
                    gpio_set_pin_high(MPSSE_DOUT_GPIO);
                } else {
                    gpio_set_pin_low(MPSSE_DOUT_GPIO);
                }

                // do the rest of the send
                read_val = mpsse_send_bits(value, MPSSE_TRANSMISSION_LEN);
            }

            // bit send done
            MPSSE_TRANSMISSION_LEN = 0;

            // if a read was requested on the bit transmission
            if (MPSSE_CUR_CMD & FTDI_READ_TDO) {
                MPSSE_RX_BUFFER[MPSSE_RX_BYTES++] = read_val; // put TDO data into RX buf
                if (mpsse_rx_buffer_remaining() > 0) {
                } else {
                    // if we've got no more room in the RX buffer, send stuff back to openocd
                    MPSSE_TRANSACTION_LOCK = 1;
                    udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, 
                        sizeof(MPSSE_RX_BUFFER), mpsse_vendor_bulk_in_received);
                }
            }
            MPSSE_CUR_CMD = 0;
            return;
        }
    }

    // need at least one byte to send
    if (mpsse_tx_buffer_remaining() < 1) {
        MPSSE_TX_REQ = 1;
        return;
    }

    if (MPSSE_CUR_CMD & (FTDI_WRITE_TDI)) {
        if ((!MPSSE_LOOPBACK_ENABLED)) {
            // data needs to be valid before we start clocking
            uint8_t value = (MPSSE_TX_BUFFER[MPSSE_TX_IDX]);
            uint8_t bit = 0;
            if (MPSSE_CUR_CMD & FTDI_LITTLE_ENDIAN) {
                bit = value & 0x01;
            } else {
                bit = value & 0x80;
            }

            if (bit) {
                gpio_set_pin_high(MPSSE_DOUT_GPIO);
            } else {
                gpio_set_pin_low(MPSSE_DOUT_GPIO);
            }
            MPSSE_FIRST_BIT = 0;
        }

        // do the rest of the read
        read_val = mpsse_send_byte(MPSSE_TX_BUFFER[MPSSE_TX_IDX++]);
    } else {
        // if no write was requested, just send out 0's
        read_val = mpsse_send_byte(0);
    }

    if (MPSSE_CUR_CMD & FTDI_READ_TDO) {
        MPSSE_RX_BUFFER[MPSSE_RX_BYTES++] = read_val; //TDO data into RX buffer
        if (mpsse_rx_buffer_remaining() > 0) {
        } else {
            // if at end of buffer, send stuff back to the PC
            MPSSE_TRANSACTION_LOCK = 1;
            udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, 
                sizeof(MPSSE_RX_BUFFER), mpsse_vendor_bulk_in_received);
        } 
    }

    // decrement data left in transmission
    if (--MPSSE_TRANSMISSION_LEN == 0) {
        // if at the end of the transmission, do the next command
        MPSSE_CUR_CMD = 0;
    }

}

void mpsse_handle_special(void)
{
    uint8_t value = 0;
    uint8_t direction = 0;
    switch (MPSSE_CUR_CMD) {
    case FTDI_SET_OPLB:
        // need a byte for IO direction and IO value
        if (mpsse_tx_buffer_remaining() < 2) {
            MPSSE_TX_REQ = 1;
            return; //need to read more data in
        }
        value = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        direction = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];

        // configure IO pins
        for (uint8_t i = 0; i < 5; i++) {
            if (direction & (1 << i)) {
				if (value & (1 << i)) {
					gpio_configure_pin(MPSSE_PINS_GPIO[i], PIO_OUTPUT_1);
                    if (i == 0)
                        MPSSE_SCK_IDLE_LEVEL = 1;
				} else {
					gpio_configure_pin(MPSSE_PINS_GPIO[i], PIO_OUTPUT_0);
                    if (i == 0)
                        MPSSE_SCK_IDLE_LEVEL = 0;
				}
            } else {
                gpio_configure_pin(MPSSE_PINS_GPIO[i], PIO_DEFAULT);
            }
        }
        // FPGA_releaselock();
        // while(!FPGA_setlock(fpga_generic));
        // FPGA_setaddr(ADDR_IOROUTE);
        // uint8_t gpio_state[8];
        // memcpy(gpio_state, xram, 8);
        // if (direction & (1 << 6)) {
        //     gpio_state[6] |= 1 << 0;
        //     if (value & (1 << 6)) {
        //         gpio_state[6] |= 1 << 1;
        //     } else {
        //         gpio_state[6] &= ~(1 << 1);
        //     }
        // } else {
        //     gpio_state[6] &= ~(1 << 0);
        // }

        // FPGA_setaddr(ADDR_IOROUTE);
        // memcpy(xram, gpio_state, 8);
        // FPGA_releaselock();
        MPSSE_CUR_CMD = 0x00;
        break;
    case FTDI_SET_OPHB:
        // we don't support these IO pins, so just ignore command
        // still need to read bytes though
        if (mpsse_tx_buffer_remaining() < 2) {
            MPSSE_TX_REQ = 1;
            return; //need to read more data in
        }
        MPSSE_CUR_CMD = 0x00;
        value = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        direction = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        break;
    case FTDI_READ_IPLB:
        // ignore
        MPSSE_CUR_CMD = 0x00;
        MPSSE_RX_BUFFER[2] = 0x00;
        MPSSE_RX_BYTES = 3;
        MPSSE_TRANSACTION_LOCK = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        break;
    case FTDI_READ_IPHB:
        // ignore
        MPSSE_CUR_CMD = 0x00;
        MPSSE_RX_BUFFER[2] = 0x01;
        MPSSE_RX_BYTES = 3;
        MPSSE_TRANSACTION_LOCK = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        break;
    case FTDI_EN_LOOPBACK:
        MPSSE_LOOPBACK_ENABLED = 1;
        MPSSE_CUR_CMD = 0x00;
        break;
    case FTDI_DIS_LOOPBACK:
        MPSSE_LOOPBACK_ENABLED = 0;
        MPSSE_CUR_CMD = 0x00;
        break;
    case FTDI_SEND_IMM:
        // send the rest of the data back to openocd
        MPSSE_TRANSACTION_LOCK = 1;
        MPSSE_CUR_CMD = 0x00;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        break;
    case 0x86:
        // some clock command
        MPSSE_TX_IDX += 2;
        MPSSE_CUR_CMD = 0x00;
        break;
    case 0x8A:
        MPSSE_CUR_CMD = 0x00;
        break;
    default:
        MPSSE_CUR_CMD = 0x00;
        break;
    }
}


void mpsse_vendor_bulk_out_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep)
{
    // we just receive stuff here, then handle in main()
    if (UDD_EP_TRANSFER_OK != status) {
        // restart
        if (MPSSE_TX_REQ) {
            udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER_BAK, 
                sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);

        }
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, 
            sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);
        MPSSE_TRANSACTION_LOCK = 1;
        return;
    }

    if (MPSSE_TX_REQ) {
        // we read into the backup buffer, move the data over to the usual one
        // extra room in the normal buffer so we always read the same amount
        for (uint16_t i = 0; i < nb_transfered; i++) {
            MPSSE_TX_BUFFER[i + mpsse_tx_buffer_remaining()] = MPSSE_TX_BUFFER_BAK[i];
        }
        MPSSE_TX_BYTES = mpsse_tx_buffer_remaining() + nb_transfered;
    } else {
        MPSSE_TX_BYTES = nb_transfered;
    }
    MPSSE_TX_REQ = 0;
    MPSSE_TX_IDX = 0;
    MPSSE_TRANSACTION_LOCK = 0;
}

void mpsse_vendor_bulk_in_received(udd_ep_status_t status, iram_size_t nb_transferred, udd_ep_id_t ep)
{
    if (UDD_EP_TRANSFER_OK != status) {
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        return;
    }
    for (uint16_t i = 0; i < (MPSSE_RX_BYTES - nb_transferred); i++) {
        // if we haven't finished sending, move the rest of the stuff to the start of the buffer
        MPSSE_RX_BUFFER[i] = MPSSE_RX_BUFFER[nb_transferred + i];
    }
    MPSSE_RX_BYTES -= nb_transferred;
    
    if (MPSSE_RX_BYTES) {
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
    } else {
        // always have 2 bytes for status
        MPSSE_RX_BYTES = 2;
        MPSSE_RX_BUFFER[0] = 0x00;
        MPSSE_RX_BUFFER[1] = 0x00;
        MPSSE_TRANSACTION_LOCK = 0;
    }

}

void mpsse_register_handlers(void)
{
    naeusb_add_out_handler(mpsse_setup_out_received);
    naeusb_add_in_handler(mpsse_setup_in_received);
}

// TODO: do writing here as we have time
// TODO: if we need to implement adaptive clock, should do in a GPIO based ISR I think?
void MPSSE_main_sendrecv_byte(void)
{
	if (!MPSSE_ENABLED) return;

    if (MPSSE_TRANSACTION_LOCK) {
        // current doing a send back to PC, so wait until that's done
        return;
    }

    if (MPSSE_TX_REQ) {
        // command split between USB transactions,
        // so move unused data back to start and read more in
        for (uint16_t i = 0; i < (mpsse_tx_buffer_remaining()); i++) {
            MPSSE_TX_BUFFER[i] = MPSSE_TX_BUFFER[i+MPSSE_TX_IDX];
        }
        MPSSE_TRANSACTION_LOCK = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, 
            MPSSE_TX_BUFFER_BAK, sizeof(MPSSE_TX_BUFFER_BAK), 
            mpsse_vendor_bulk_out_received);
        return;
    }

    // we're at end of the TX buffer, so read more in
    if (mpsse_tx_buffer_remaining() <= 0) {
        MPSSE_TRANSACTION_LOCK = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);
        return;
    }

    if (MPSSE_CUR_CMD == 0x00) {
        MPSSE_CUR_CMD = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        NUM_PROCESSED_CMDS++;
    }

    if (MPSSE_CUR_CMD & 0x80) {
        mpsse_handle_special();
    } else {
        mpsse_handle_transmission();
    }

}
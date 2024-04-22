#include "naeusb_default.h"
#include <string.h>
#include "naeusb_mpsse.h"

#define uint8_t unsigned char

// used to switch USB configuration from CDC to MPSSE
extern void switch_configurations(void);

volatile uint8_t waitvar = 0;
#define numwait 2

static inline void toggle_sck(void)
{
    for (waitvar = 0; waitvar < numwait; waitvar++);
    gpio_toggle_pin(MPSSE_SCK_GPIO);
    for (waitvar = 0; waitvar < numwait; waitvar++);
}

#pragma pack(1)
struct  {
    union {
        uint8_t u8;
        struct {
            uint8_t neg_clk_write:1;
            uint8_t bit_mode:1;
            uint8_t neg_clk_read:1;
            uint8_t lendian:1;
            uint8_t wtdi:1;
            uint8_t rtdo:1;
            uint8_t wtms:1;
            uint8_t special:1;
        } b;
    } cur_cmd; //the current command, available as a bit field or as a u8

    int16_t tx_idx; //the current index in the tx buffer
    int16_t tx_bytes; //the number of bytes in the tx buffer
    int16_t rx_bytes; //the number of bytes in the rx buffer
    int16_t txn_len; //the length of the current transmission

    uint8_t loopback_en; //loopback mode enabled?
    uint8_t txn_lock; //lock for waiting on USB transaction

    uint32_t n_processed_cmds; //the number of commands we've processed

    uint8_t tx_req; //more data from the pc is required to process the current command
    uint8_t swd_mode; //swd mode enabled
    uint8_t swd_out_en; //1 for swd output, 0 for swd input
    uint8_t enabled; // mpsse mode enabled?
    uint32_t swd_out_en_pin;

    uint32_t pins[12];
} static mpsse_state = {
    .cur_cmd.u8 = 0x00, 
    .tx_idx = 0x00, .tx_bytes = 0x00, .rx_bytes = 0x02, .txn_len = 0x00, 
    .loopback_en = 0x00, .txn_lock = 0x00,
    .n_processed_cmds = 0x00,

    .tx_req = 0x00, .swd_mode = 0x00, .swd_out_en = 0x00, .enabled = 0x00,
    #ifdef MPSSE_TMS_WR
    .swd_out_en_pin = MPSSE_TMS_WR,
    #elif MPSSE_TMS_WR_PIN_0
    .swd_out_en_pin = MPSSE_TMS_WR_PIN_0,
    #endif
    .pins = {
        MPSSE_SCK_GPIO,
        MPSSE_DOUT_GPIO,
        MPSSE_DIN_GPIO,
        MPSSE_TMS_GPIO,
        #ifdef MPSSE_GPIOL0
        MPSSE_GPIOL0,
        #else
        0x00,
        #endif
        #ifdef MPSSE_GPIOL1
        MPSSE_GPIOL1,
        #else
        0x00,
        #endif
        #ifdef MPSSE_GPIOL2
        MPSSE_GPIOL2,
        #else
        0x00,
        #endif
        #ifdef MPSSE_GPIOL3
        MPSSE_GPIOL3,
        #else
        0x00,
        #endif
    }
};
#pragma pack()

uint8_t mpsse_enabled(void)
{
    return mpsse_state.enabled;
}

//BUFFERS
// holds data to send back to PC
COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_RX_BUFFER[64] __attribute__ ((__section__(".mpssemem")));
// COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_RX_CTRL_BUFFER[64] __attribute__ ((__section__(".mpssemem"))); //TODO: implement this

// holds data received from PC
COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_TX_BUFFER[80] __attribute__ ((__section__(".mpssemem"))) = {0};

// holds data receieved from PC when a command overlaps between reads
COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_TX_BUFFER_BAK[64] __attribute__ ((__section__(".mpssemem"))) = {0};

void mpsse_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);
void mpsse_vendor_bulk_out_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);

/*
Gets remaining available space in the RX buffer
*/
static int16_t mpsse_rx_buffer_remaining(void)
{
    return sizeof(MPSSE_RX_BUFFER) - mpsse_state.rx_bytes;
}

/*
Gets the number of unprocessed TX bytes in the buffer
*/
static int16_t mpsse_tx_buffer_remaining(void)
{
    return mpsse_state.tx_bytes - mpsse_state.tx_idx;
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
        mpsse_state.enabled = 1;
        mpsse_state.txn_lock = 1;
        //restart USB
        udc_start();
        #ifdef PIN_TARG_NRST_GPIO
        gpio_configure_pin(PIN_TARG_NRST_GPIO, PIO_DEFAULT | PIO_OUTPUT_0);
        #endif
        return true;
    } else if ((wValue == 0x43) && (udd_g_ctrlreq.req.bRequest == REQ_SAM_CFG)) {
        uint8_t swd_pin = udd_g_ctrlreq.req.wValue >> 8;
        #ifndef MPSSE_TMS_WR_PIN_0
            return false;
        #else
        if (swd_pin == 0x00) {
            mpsse_state.swd_out_en_pin = MPSSE_TMS_WR_PIN_0;
            return true;
        } 
        #endif
        #ifdef MPSSE_TMS_WR_PIN_1
        else if (swd_pin == 0x01) {
            mpsse_state.swd_out_en_pin = MPSSE_TMS_WR_PIN_1;
            return true;
        }
        #endif
        #ifdef MPSSE_TMS_WR_PIN_2
        else if (swd_pin == 0x02) {
            mpsse_state.swd_out_en_pin = MPSSE_TMS_WR_PIN_2;
            return true;
        }
        #endif
        return false;
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

        mpsse_state.enabled = 1;
		mpsse_state.enabled = 1;
        mpsse_state.txn_lock = 1;
        mpsse_state.n_processed_cmds = 0;
        mpsse_state.swd_mode = 0;

        // start looking for data from openocd
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);
    }
    return true;
}

/* Handle ctrl transfer on interface 1/2. Mostly used for debug purposes */
bool mpsse_setup_in_received(void)
{
    if (udd_g_ctrlreq.req.bRequest == REQ_SAM_STATUS) {
        if ((udd_g_ctrlreq.req.wValue & 0xFF) == 0x42) {
            respbuf[0] = mpsse_state.enabled;
            udd_g_ctrlreq.payload = respbuf;
            udd_g_ctrlreq.payload_size = 1;
            return true;
        }
    }
    // don't handle if not sent to our interface
    if (udd_g_ctrlreq.req.wIndex != 0x01) {
        return false;
    }

    // For debug, reads a bunch of internal variables back. TODO: change to using a separate buf
    if (udd_g_ctrlreq.req.bRequest == 0xA0) {
        MPSSE_RX_BUFFER[0] = mpsse_state.cur_cmd.u8;
        MPSSE_RX_BUFFER[1] = mpsse_state.tx_idx & 0xFF;
        MPSSE_RX_BUFFER[2] = mpsse_state.tx_bytes & 0xFF;
        MPSSE_RX_BUFFER[3] = mpsse_state.rx_bytes & 0xFF;
        MPSSE_RX_BUFFER[4] = mpsse_state.txn_len & 0xFF;
        MPSSE_RX_BUFFER[5] = mpsse_state.txn_lock;
        MPSSE_RX_BUFFER[6] =  mpsse_state.n_processed_cmds & 0xFF;
        MPSSE_RX_BUFFER[7] = (mpsse_state.n_processed_cmds >> 8) & 0xFF;
        MPSSE_RX_BUFFER[8] = (mpsse_state.n_processed_cmds >> 16) & 0xFF;
        MPSSE_RX_BUFFER[9] = (mpsse_state.n_processed_cmds >> 24) & 0xFF;
        MPSSE_RX_BUFFER[10] = mpsse_state.loopback_en;
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

/*
Send one bit over TDI and read one bit on TDO

Does a small delay after setting DOUT high to
allow a reasonable setup time for the target receiver.

Really DOUT should be changed on the previous clock transition, but
that makes things messier and comms is slow enough that this
probably won't cause issues.
*/
uint8_t mpsse_send_bit(uint8_t value)
{
    value &= 0x01;
    uint8_t read_value = 0;
    uint32_t dpin;
    if (mpsse_state.loopback_en) {
        return value & 0x01;
    }

    // do write
    dpin = MPSSE_DOUT_GPIO;
    if (value) {
        gpio_set_pin_high(dpin);
    } else {
        gpio_set_pin_low(dpin);
    }

    // last bit will be received on TDI
    toggle_sck();
    //now we read data in
    read_value = gpio_pin_is_high(MPSSE_DIN_GPIO);

    toggle_sck();
    

    
    return read_value & 0x01;
}

/*
Send a bit over SWD or read a bit

Works similarly to the regular bit send,
except this is over TMS for read/write
and the read is done on the rising edge of the clock
*/
uint8_t mpsse_swd_send_bit(uint8_t value)
{
    value &= 0x01;
    uint8_t read_value = 0;
    uint32_t dpin;

    // actually do write
    dpin = mpsse_state.pins[3];

    // this is really dumb
    if (mpsse_state.swd_out_en) {
        if (value) {
            gpio_set_pin_high(dpin);
        } else {
            gpio_set_pin_low(dpin);
        }
    }

    //setup delay

    if (!mpsse_state.swd_out_en)
        read_value = gpio_pin_is_high(dpin);

    // last bit will be received on TDI

    toggle_sck();

    toggle_sck();


    
    return read_value & 0x01;
}

// send up to 8 bits over TDI/TDO
uint8_t mpsse_send_bits(uint8_t value, uint8_t num_bits)
{
    // if (num_bits > 8) num_bits = 8;
    uint8_t read_value = 0;

    for (uint8_t i = 0; i < num_bits; i++) {
        if (mpsse_state.cur_cmd.b.lendian) {
            // NOTE: for little endian, bits are written to bit 7, then shifted down
            read_value >>= 1;
            if (mpsse_state.swd_mode)
                read_value |= mpsse_swd_send_bit((value >> i) & 0x01) << 7;
            else
                read_value |= mpsse_send_bit((value >> i) & 0x01) << 7;
        } else {
            if (mpsse_state.swd_mode)
                read_value |= mpsse_swd_send_bit((value >> (7 - i)) & 0x01) << (7 - i);
            else
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

/*
Send/read one bit on TMS

Works the same as the normal bit send except writes on TMS
*/
uint8_t mpsse_tms_bit_send(uint8_t value)
{
    value &= 0x01;
    uint8_t read_value = 0;
    uint32_t dpin;

    dpin = MPSSE_TMS_GPIO;

    if (value) {
        gpio_set_pin_high(dpin);
    } else {
        gpio_set_pin_low(dpin);
    }

    //setup delay

    read_value = gpio_pin_is_high(dpin);

    toggle_sck();

    read_value = gpio_pin_is_high(MPSSE_DIN_GPIO);


    toggle_sck();

    
    return read_value & 0x01;

}

// send up to 7 bits over TMS
uint8_t mpsse_tms_send(uint8_t value, uint8_t num_bits)
{
    uint8_t read_value = 0;
    uint8_t i = 0;

    // bit 7 is sent out on the dout pin
    // only counts if sending 7 bits tho ;)
    uint8_t bitval = 0;
    bitval = (value >> 7) & 0x01;
    if (bitval) {
        gpio_set_pin_high(MPSSE_DOUT_GPIO);
    } else {
        gpio_set_pin_low(MPSSE_DOUT_GPIO);
    }
    if (num_bits == 7) {
        i++;
    }

    // send the rest of the bits out on TMS
    // TODO: should this work like the normal rx with little endian?
    // I don't think reads during TMS are typical, so hard to say
    for (; i < num_bits; i++) {
        if (mpsse_state.cur_cmd.b.lendian) {
            read_value >>= 1;
            read_value |= mpsse_tms_bit_send((value >> i) & 0x01) << (7 - i);
        } else {
            read_value |= mpsse_tms_bit_send((value >> (7 - i)) & 0x01) << (7 - i);
        }

    }
	return read_value;
}

/*
Handle all MPSSE transmissions

Sends/receives 1 byte at a time, but may return early if we're at
the end of the buffer and need more data
*/
void mpsse_handle_transmission(void)
{
    uint8_t read_val = 0;
    if (mpsse_state.txn_len == 0) { 
        // not currently doing a transmission
        /*
        Always need at least 2 more bytes here
        For byte tx/rx, both used for tx/rx len
        For bit tx/rx, first is num bits, second is data

        NOTE: Technically bit reads only need 1 byte, so could cause issues here
        */
        if (mpsse_tx_buffer_remaining() < 2) {
            mpsse_state.tx_req = 1;
            return; //need more data
        }

        // read length in
        mpsse_state.txn_len = MPSSE_TX_BUFFER[mpsse_state.tx_idx++];

        if (!mpsse_state.cur_cmd.b.bit_mode) {
            // clock in high byte of length if in byte mode
            mpsse_state.txn_len |= MPSSE_TX_BUFFER[mpsse_state.tx_idx++] << 8;
            mpsse_state.txn_len++; //0x00 sends 1 byte, 0x01 sends 2, etc
        } else {

            // take care of bit transmission right now
            mpsse_state.txn_len++; //0x00 sends 1 bit
            if (mpsse_state.cur_cmd.b.wtms) {
                read_val = mpsse_tms_send(MPSSE_TX_BUFFER[mpsse_state.tx_idx++], mpsse_state.txn_len);
                mpsse_state.txn_len = 0;
            } else {
                uint8_t value = 0;
                if (mpsse_state.cur_cmd.b.wtdi) // if we're writing, read a byte from the buffer
                     value = MPSSE_TX_BUFFER[mpsse_state.tx_idx++];

                read_val = mpsse_send_bits(value, mpsse_state.txn_len);
            }

            // bit send done
            mpsse_state.txn_len = 0;

            // if a read was requested on the bit transmission
            if (mpsse_state.cur_cmd.b.rtdo) {
                MPSSE_RX_BUFFER[mpsse_state.rx_bytes++] = read_val; // put TDO data into RX buf
                if (mpsse_rx_buffer_remaining() > 0) {
                } else {
                    // if we've got no more room in the RX buffer, send stuff back to openocd
                    mpsse_state.txn_lock = 1;
                    udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, 
                        sizeof(MPSSE_RX_BUFFER), mpsse_vendor_bulk_in_received);
                }
            }
            mpsse_state.cur_cmd.u8 = 0;
            return;
        }
    }

    // Now that the transaction length is known, we can start writing bytes
    // need at least one byte to send

    // TMS not available for byte write
    if (mpsse_tx_buffer_remaining() < 1) {
        mpsse_state.tx_req = 1;
        return;
    }

    if (mpsse_state.cur_cmd.b.wtdi) {
        read_val = mpsse_send_byte(MPSSE_TX_BUFFER[mpsse_state.tx_idx++]);
    } else {
        // if no write was requested, just send out 0's
        read_val = mpsse_send_byte(0);
    }

    if (mpsse_state.cur_cmd.b.rtdo) {
        MPSSE_RX_BUFFER[mpsse_state.rx_bytes++] = read_val; //TDO data into RX buffer
        if (mpsse_rx_buffer_remaining() > 0) {
        } else {
            // if at end of buffer, send stuff back to the PC
            mpsse_state.txn_lock = 1;
            udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, 
                sizeof(MPSSE_RX_BUFFER), mpsse_vendor_bulk_in_received);
        } 
    }

    // decrement data left in transmission
    if (--mpsse_state.txn_len == 0) {
        // if at the end of the transmission, do the next command
        mpsse_state.cur_cmd.u8 = 0;

    }

}

/*
Handle special (usually non read/write) MPSSE commands

Ex. Pin config, read buffer back
*/
void mpsse_handle_special(void)
{
    uint8_t value = 0;
    uint8_t direction = 0;
    switch (mpsse_state.cur_cmd.u8) {
    // GPIO setup commands
    case FTDI_SET_OPLB:
        // need a byte for IO direction and IO value
        if (mpsse_tx_buffer_remaining() < 2) {
            mpsse_state.tx_req = 1;
            return;
        }

        value = MPSSE_TX_BUFFER[mpsse_state.tx_idx++];
        direction = MPSSE_TX_BUFFER[mpsse_state.tx_idx++];

        // configure IO pins
        for (uint8_t i = 0; i < 8; i++) {
            if (!mpsse_state.pins[i])
                continue;
            if (direction & (1 << i)) {
				if (value & (1 << i) || (!mpsse_state.swd_mode)) {
					gpio_configure_pin(mpsse_state.pins[i], PIO_OUTPUT_1);
                    if (i == 0) 
                        gpio_toggle_pin(mpsse_state.pins[0]); //ignore idle high clock
                        
				} else {
					gpio_configure_pin(mpsse_state.pins[i], PIO_OUTPUT_0);
				}
            } else {
                gpio_configure_pin(mpsse_state.pins[i], PIO_INPUT);
            }
        }

        if (!mpsse_state.swd_mode) {
            #ifdef MPSSE_TMS_WR
            gpio_configure_pin(MPSSE_TMS_WR, PIO_OUTPUT_1);
            #endif
            #ifdef MPSSE_TRST_WR
            gpio_configure_pin(MPSSE_TRST_WR, PIO_OUTPUT_1);
            #endif
        }

        mpsse_state.cur_cmd.u8 = 0x00;
        break;
    case FTDI_SET_OPHB:
        // Special "IO" for enabling SWD and SWD output
        if (mpsse_tx_buffer_remaining() < 2) {
            mpsse_state.tx_req = 1;
            return; //need to read more data in
        }

        mpsse_state.cur_cmd.u8 = 0x00;
        value = MPSSE_TX_BUFFER[mpsse_state.tx_idx++];
        direction = MPSSE_TX_BUFFER[mpsse_state.tx_idx++];

    uint32_t swd_out_en_pin;
        #if MPSSE_SWD_SUPPORT
        if ((value & 1)) {
            // special SWD enable case
            mpsse_state.swd_mode = 1;
            if (value & 2) {
                mpsse_state.swd_out_en = 1;
                // hack since TX and RX pins aren't connected like on Lite
                #if USB_DEVICE_PRODUCT_ID == 0xACE3
                    mpsse_state.pins[3] = PIN_PDIDTX_GPIO;
                #endif

                #if defined(MPSSE_TMS_WR) || defined(MPSSE_TMS_WR_PIN_0)
                gpio_configure_pin(mpsse_state.swd_out_en_pin, PIO_OUTPUT_1);
                #endif
                gpio_configure_pin(mpsse_state.pins[3], PIO_OUTPUT_1);

            } else {
                //hack for Pro
                #if USB_DEVICE_PRODUCT_ID == 0xACE3
                    mpsse_state.pins[3] = PIN_PDIDRX_GPIO;
                #endif

                gpio_configure_pin(mpsse_state.pins[3], PIO_INPUT);

                #if defined(MPSSE_TMS_WR) || defined(MPSSE_TMS_WR_PIN_0)
                gpio_configure_pin(mpsse_state.swd_out_en_pin, PIO_OUTPUT_0);
                #endif

                mpsse_state.swd_out_en = 0;
            }
        } else {
            mpsse_state.swd_mode = 0;
        }
        #endif
        break;
    case FTDI_READ_IPLB:
        // ignore
        mpsse_state.cur_cmd.u8 = 0x00;
        MPSSE_RX_BUFFER[2] = 0x00;
        mpsse_state.rx_bytes = 3;
        mpsse_state.txn_lock = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, mpsse_state.rx_bytes, mpsse_vendor_bulk_in_received);
        break;
    case FTDI_READ_IPHB:
        // ignore
        mpsse_state.cur_cmd.u8 = 0x00;
        MPSSE_RX_BUFFER[2] = 0x01;
        mpsse_state.rx_bytes = 3;
        mpsse_state.txn_lock = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, mpsse_state.rx_bytes, mpsse_vendor_bulk_in_received);
        break;
    case FTDI_EN_LOOPBACK:
        mpsse_state.loopback_en = 1;
        mpsse_state.cur_cmd.u8 = 0x00;
        break;
    case FTDI_DIS_LOOPBACK:
        mpsse_state.loopback_en = 0;
        mpsse_state.cur_cmd.u8 = 0x00;
        break;
    case FTDI_SEND_IMM:
        // send the rest of the data back to openocd
        mpsse_state.txn_lock = 1;
        mpsse_state.cur_cmd.u8 = 0x00;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, mpsse_state.rx_bytes, mpsse_vendor_bulk_in_received);
        break;
    case 0x86:
        // some clock command, ignore
        mpsse_state.tx_idx += 2;
        mpsse_state.cur_cmd.u8 = 0x00;
        break;
    case 0x8A:
        mpsse_state.cur_cmd.u8 = 0x00;
        break;
    default:
        mpsse_state.cur_cmd.u8 = 0x00;
        break;
    }
}


void mpsse_vendor_bulk_out_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep)
{
    // we just receive stuff here, then handle in main()
    if (UDD_EP_TRANSFER_OK != status) {
        // restart
        if (mpsse_state.tx_req) {
            udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER_BAK, 
                sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);

        }
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, 
            sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);
        mpsse_state.txn_lock = 1;
        return;
    }

    if (mpsse_state.tx_req) {
        // we read into the backup buffer, move the data over to the usual one
        // extra room in the normal buffer so we always read the same amount

        // reading unususal sizes breaks USB, so don't change this
        for (uint16_t i = 0; i < nb_transfered; i++) {
            MPSSE_TX_BUFFER[i + mpsse_tx_buffer_remaining()] = MPSSE_TX_BUFFER_BAK[i];
        }
        mpsse_state.tx_bytes = mpsse_tx_buffer_remaining() + nb_transfered;
    } else {
        mpsse_state.tx_bytes = nb_transfered;
    }
    mpsse_state.tx_req = 0;
    mpsse_state.tx_idx = 0;
    mpsse_state.txn_lock = 0;
}

void mpsse_vendor_bulk_in_received(udd_ep_status_t status, iram_size_t nb_transferred, udd_ep_id_t ep)
{
    if (UDD_EP_TRANSFER_OK != status) {
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, mpsse_state.rx_bytes, mpsse_vendor_bulk_in_received);
        return;
    }
    for (uint16_t i = 0; i < (mpsse_state.rx_bytes - nb_transferred); i++) {
        // if we haven't finished sending, move the rest of the stuff to the start of the buffer
        MPSSE_RX_BUFFER[i] = MPSSE_RX_BUFFER[nb_transferred + i];
    }
    mpsse_state.rx_bytes -= nb_transferred;
    
    if (mpsse_state.rx_bytes) {
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, mpsse_state.rx_bytes, mpsse_vendor_bulk_in_received);
    } else {
        // always have 2 bytes for status
        mpsse_state.rx_bytes = 2;
        MPSSE_RX_BUFFER[0] = 0x00;
        MPSSE_RX_BUFFER[1] = 0x00;
        mpsse_state.txn_lock = 0;
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
	if (!mpsse_state.enabled) return;

    if (mpsse_state.txn_lock) {
        // waiting on a USB transaction to/from the PC, so wait for that to be done
        // before doing anything else
        return;
    }

    if (mpsse_state.tx_req) {
        // If here, we tried to process a command, but were at the end
        // of the buffer and need more data.

        // command split between USB transactions,
        // so move unused data back to start and read more in
        for (uint16_t i = 0; i < (mpsse_tx_buffer_remaining()); i++) {
            MPSSE_TX_BUFFER[i] = MPSSE_TX_BUFFER[i+mpsse_state.tx_idx];
        }
        mpsse_state.txn_lock = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, 
            MPSSE_TX_BUFFER_BAK, sizeof(MPSSE_TX_BUFFER_BAK), 
            mpsse_vendor_bulk_out_received);
        return;
    }

    // we're at end of the TX buffer, so read more in
    if (mpsse_tx_buffer_remaining() <= 0) {
        mpsse_state.txn_lock = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, sizeof(MPSSE_TX_BUFFER_BAK), mpsse_vendor_bulk_out_received);
        return;
    }

    // finished processing the last command, so read a new one
    if (mpsse_state.cur_cmd.u8 == 0x00) {
        mpsse_state.cur_cmd.u8 = MPSSE_TX_BUFFER[mpsse_state.tx_idx++];
        mpsse_state.n_processed_cmds++;
    }

    if (mpsse_state.cur_cmd.b.special) {
        mpsse_handle_special();
    } else {
        mpsse_handle_transmission();
    }

}
#include "naeusb_default.h"

// static uint16_t MPSSE_TX_IDX = 0;
static uint8_t MPSSE_CUR_CMD = 0; // the current MPSSE command
static uint16_t MPSSE_TX_IDX = 0; // the current TX buffer index
static uint16_t MPSSE_TX_BYTES = 0; // the number of bytes in the tx buffer
static uint16_t MPSSE_RX_BYTES = 0; // the number of bytes in the rx buffer
static uint16_t MPSSE_TRANSMISSION_LEN = 0; // the number of bits/bytes in the current data transmission

static uint8_t MPSSE_LOOPBACK_ENABLED = 0; // connect TDI to TDO for loopback

//need lock to make sure we don't accept another transaction until we're done sending data back
//should be fine since  it looks like openocd just keeps retrying transaction until it works
static uint8_t MPSSE_TRANSACTION_LOCK = 1;  //Currently sending data back to the PC


COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_RX_BUFFER[0x4000] __attribute__ ((__section__(".mpssemem")));
COMPILER_WORD_ALIGNED volatile uint8_t MPSSE_TX_BUFFER[512] = {0};
void mpsse_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);
void mpsse_vendor_bulk_out_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);

extern void switch_configurations(void);
// need to implement reset
// latency timer request
// purge tx and TX on reset
// set bitmode - just selects MPSSE mode
// I think we can just ignore all this stuff, since they're just to setup 
bool mpsse_setup_out_received(void)
{
    // don't handle 
    uint8_t wValue = udd_g_ctrlreq.req.wValue & 0xFF;
    if ((wValue == 0x42) && (udd_g_ctrlreq.req.bRequest == REQ_SAM_CFG)) {
        udc_stop();
        switch_configurations();
        udc_start();
        return true;
    }
    if (udd_g_ctrlreq.req.wIndex != 0x01) {
        return false;
    }
    return true;
}

#define BITMODE_MPSSE 0x02

#define SIO_RESET_REQUEST             0x00
#define SIO_SET_LATENCY_TIMER_REQUEST 0x09
#define SIO_GET_LATENCY_TIMER_REQUEST 0x0A
#define SIO_SET_BITMODE_REQUEST       0x0B

#define SIO_RESET_SIO 0
#define SIO_RESET_PURGE_RX 1
#define SIO_RESET_PURGE_TX 2


bool mpsse_setup_in_received(void)
{
    // don't handle 
    if (udd_g_ctrlreq.req.wIndex != 0x01) {
        return false;
    }
    if ((udd_g_ctrlreq.req.bRequest == SIO_RESET_REQUEST)) {
        // TODO: cancel old run if one setup
        udd_ep_abort(UDI_MPSSE_EP_BULK_OUT);
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, false, MPSSE_TX_BUFFER, 512, mpsse_vendor_bulk_out_received);
        MPSSE_TRANSACTION_LOCK = 1;
    }
    return true;
}


// 
enum FTDI_CMD_BITS {
    FTDI_NEG_CLK_WRITE  = 1 << 0,
    FTDI_BIT_MODE       = 1 << 1,
    FTDI_NEG_CLK_READ   = 1 << 2,
    FTDI_LITTLE_ENDIAN  = 1 << 3,
    FTDI_WRITE_TDI      = 1 << 4,
    FTDI_READ_TDO       = 1 << 5,
    FTDI_WRITE_TMS      = 1 << 6,
    FTDI_SPECIAL_CMD    = 1 << 7,
};

enum FTDI_SPECIAL_CMDS {
    FTDI_SET_OPLB = 0x80,
    FTDI_READ_IPLB,
    FTDI_SET_OPHB,
    FTDI_READ_IPHB,
    FTDI_EN_LOOPBACK,
    FTDI_DIS_LOOPBACK,
    FTDI_DIS_CLK_DIV_5 = 0x8A,
    FTDI_EN_CLK_DIV_5 = 0x8A,
    FTDI_SEND_IMM=0x87,
    FTDI_ADT_CLK_ON=0x96,
    FTDI_ADT_CLK_OFF
};


uint8_t mpsse_send_bit(uint8_t value)
{
    value &= 0x01;
    uint8_t read_value;
    if (MPSSE_LOOPBACK_ENABLED) {
        read_value = value;
    } else {
        // actually do write
    }
    return read_value & 0x01;
}

uint8_t mpsse_send_bits(uint8_t value, uint8_t num_bits)
{
    uint8_t read_value = 0;
    for (uint8_t i = 0; i < num_bits; i++) {
        if (MPSSE_CUR_CMD & FTDI_LITTLE_ENDIAN) {
            read_value |= mpsse_send_bit((value >> i) & 0x01) << i;
        } else {
            read_value |= mpsse_send_bit((value >> (7 - i)) & 0x01) << (7 - i);
        }
    }
    return read_value;
}

uint8_t mpsse_send_byte(uint8_t value)
{
    return mpsse_send_bits(value, 8);
}

void mpsse_handle_transmission(void)
{
    uint8_t read_val = 0;
    if (MPSSE_TRANSMISSION_LEN == 0) {
        // not doing a transmission currently, so read length in
        // TODO: handle invalid lengths
        MPSSE_TRANSMISSION_LEN = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];

        if (!(MPSSE_CUR_CMD & FTDI_BIT_MODE)) {
            // clock in high byte of length if in byte mode
            MPSSE_TRANSMISSION_LEN |= MPSSE_TX_BUFFER[MPSSE_TX_IDX++] << 8;
        } else {
            // take care of bit transmission right now
            read_val = mpsse_send_bits(MPSSE_TX_BUFFER[MPSSE_TX_IDX++], MPSSE_TRANSMISSION_LEN);
            MPSSE_TRANSMISSION_LEN = 0;
            if (MPSSE_CUR_CMD & FTDI_READ_TDO) {
                MPSSE_RX_BUFFER[MPSSE_RX_BYTES++] = read_val;
            }
            MPSSE_CUR_CMD = 0;
            return;
        }
    }

    // todo: handle invalid command: no write or read
    if (MPSSE_CUR_CMD & (FTDI_WRITE_TDI | FTDI_WRITE_TMS)) {
        read_val = mpsse_send_byte(MPSSE_TX_BUFFER[MPSSE_TX_IDX++]);
    } else {
        read_val = mpsse_send_byte(0);
    }

    if (MPSSE_CUR_CMD & FTDI_READ_TDO) {
        MPSSE_RX_BUFFER[MPSSE_RX_BYTES++] = read_val;
    }

    // we're at the end of the transmission
    if (--MPSSE_TRANSMISSION_LEN == 0) {
        MPSSE_CUR_CMD = 0;
    }

}

void mpsse_handle_special(void)
{
    uint8_t value = 0;
    uint8_t direction = 0;
    switch (MPSSE_CUR_CMD) {
    case FTDI_SET_OPLB:
        // value, direction
        MPSSE_CUR_CMD = 0x00;
        value = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        direction = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        break;
    case FTDI_SET_OPHB:
        // value, direction
        MPSSE_CUR_CMD = 0x00;
        value = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        direction = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
        break;
    case FTDI_READ_IPLB:
        MPSSE_CUR_CMD = 0x00;
        MPSSE_RX_BUFFER[0] = 0x00;
        MPSSE_RX_BYTES = 1;
        MPSSE_TRANSACTION_LOCK = 1;
        MPSSE_TX_IDX++;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        break;
    case FTDI_READ_IPHB:
        MPSSE_CUR_CMD = 0x00;
        MPSSE_RX_BUFFER[0] = 0x00;
        MPSSE_RX_BYTES = 1;
        MPSSE_TRANSACTION_LOCK = 1;
        MPSSE_TX_IDX++;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        break;
    case FTDI_EN_LOOPBACK:
        MPSSE_LOOPBACK_ENABLED = 1;
        MPSSE_CUR_CMD = 0x00;
        MPSSE_TX_IDX++;
        break;
    case FTDI_DIS_LOOPBACK:
        MPSSE_LOOPBACK_ENABLED = 0;
        MPSSE_CUR_CMD = 0x00;
        MPSSE_TX_IDX++;
        break;
    case FTDI_SEND_IMM:
        MPSSE_TRANSACTION_LOCK = 1;
        MPSSE_CUR_CMD = 0x00;
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        MPSSE_TX_IDX++;
        break;
    }
}


void mpsse_vendor_bulk_out_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep)
{
    // we just receive stuff here, then handle in main()
    MPSSE_TX_BYTES = nb_transfered;
    MPSSE_TX_IDX = 0;
    MPSSE_CUR_CMD = 0;
    if (UDD_EP_TRANSFER_OK != status) {
        // restart
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, 512, mpsse_vendor_bulk_out_received);
    }
}

void mpsse_vendor_bulk_in_received(udd_ep_status_t status, iram_size_t nb_transferred, udd_ep_id_t ep)
{
    if (UDD_EP_TRANSFER_OK != status) {
        // I think this is the right thing to do
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
        return;
    }
    for (uint16_t i = 0; i < (MPSSE_RX_BYTES - nb_transferred); i++) {
        // move data to start of buffer
        // simplifies things, plus avoids buffer alignment issues
        MPSSE_RX_BUFFER[i] = MPSSE_RX_BUFFER[nb_transferred + i];
    }
    MPSSE_RX_BYTES -= nb_transferred;
    
    if (MPSSE_RX_BYTES) {
        udd_ep_run(UDI_MPSSE_EP_BULK_IN, 0, MPSSE_RX_BUFFER, MPSSE_RX_BYTES, mpsse_vendor_bulk_in_received);
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
    // return;
    if (MPSSE_TRANSACTION_LOCK) {
        // current doing a send back to PC, so wait until that's done
        return;
    }

    // we're at end of the TX buffer, so read more in
    if (MPSSE_TX_IDX == MPSSE_TX_BYTES) {
        MPSSE_TRANSACTION_LOCK = 1;
        udd_ep_run(UDI_MPSSE_EP_BULK_OUT, 0, MPSSE_TX_BUFFER, 512, mpsse_vendor_bulk_out_received);
        return;
    }

    if (MPSSE_CUR_CMD == 0x00) {
        MPSSE_CUR_CMD = MPSSE_TX_BUFFER[MPSSE_TX_IDX++];
    }

    if (MPSSE_CUR_CMD & 0x80) {
        mpsse_handle_special();
    } else {
        mpsse_handle_transmission();
    }

}
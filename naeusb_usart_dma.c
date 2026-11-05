/*
    NOTE: Also has CDC functions
*/
#include <asf.h>
#include <string.h>
#include "circbuffer.h"
#include "usart_driver.h"
#include "naeusb_default.h"
#include "usart.h"
#include "usb_protocol_cdc.h"
#include "naeusb_usart.h"
#include "naeusb_usart_impl.h"
#include "naeusb_usart_dma.h"
#include "pdc.h"

#ifdef CW_PROG_XMEGA
#include "XPROGNewAE.h"
#endif

#ifdef CW_PROG_AVR
#include "V2Protocol.h"
#endif

#define USART_WVREQ_INIT    0x0010
#define USART_WVREQ_ENABLE  0x0011
#define USART_WVREQ_DISABLE 0x0012
#define USART_WVREQ_NUMWAIT 0x0014
#define USART_WVREQ_NUMWAIT_TX 0x0018
#define USART_WVREQ_XONXOFF 0x0020

#define USART_XON 0x11
#define USART_XOFF 0x13


bool usart_setup_in_received(void);
bool usart_setup_out_received(void);
bool naeusb_cdc_settings_in(void);
void naeusb_cdc_settings_out(void);

int is_usart_rx_overrun(struct dma_state *dst);



/*
    Number of characters in DMA buffer
*/
uint16_t usart_inwait(struct dma_state *dst)
{

    if (dst->is_tx) {
        return pdc_read_tx_counter(dst->pdc) + pdc_read_tx_next_counter(dst->pdc);
    } else {

        if (is_usart_rx_overrun(dst)) {
            return dst->buf_sz; // we had overrun, so buffer is full
        }

        if (!is_dma_wrapped(dst)) {
            // if not wrapped, #waiting chars is just diff between read head and dma ptr
            return usart_dma_ptr(dst) - dst->buf_head;
        } else {
            // if wrapped, #waiting chars is dist from buff head to end, plus distance between buff start and dma_ptr
            return (dst->buf_end - dst->buf_head) + (usart_dma_ptr(dst) - dst->buf_st);
        }
    }
}

uint16_t usart_free_space(struct dma_state *dst)
{
    return dst->buf_sz - usart_inwait(dst);
}

int is_usart_rx_overrun(struct dma_state *dst)
{
    return (usart_is_rx_buf_full(dst->usart) || (dst->dropped > 0));
}

static void setup_dma_state(struct dma_state *dst, uint16_t size)
{
    // dst->buf_st_ptr = dst->buf;
    dst->buf_sz = size;

    dst->buf_head = dst->buf_st;

    dst->buf_end = dst->buf_st + dst->buf_sz;
    dst->dropped = 0;
}

/*
    Setup the DMA and USART for our DMA. Also updates the USART's settings
*/
void setup_usart_dma(struct usart_state *ust)
{
    struct pdc_packet packet;
    struct pdc_packet npacket;
    CURRENT_ERRORS &= ~(CW_ERR_USART_RX | CW_ERR_USART_RX_OVERFLOW | CW_ERR_USART_TX | CW_ERR_USART_TX_OVERFLOW); // clear usart errs
    pdc_disable_transfer(ust->pdc, PERIPH_PTCR_RXTDIS | PERIPH_PTCR_TXTDIS);

    // setup tx
    setup_dma_state(&ust->tx_st, USART_TX_BUF_SZ);
    packet.ul_addr = ust->tx_st.buf_st;
    packet.ul_size = 0;
    pdc_tx_init(ust->tx_st.pdc, &packet, NULL);

    // setup rx
    setup_dma_state(&ust->rx_st, USART_RX_BUF_SZ);
    packet.ul_addr = ust->rx_st.buf_st;
    packet.ul_size = ust->rx_st.buf_sz;
    npacket.ul_addr = ust->rx_st.buf_st;
    npacket.ul_size = 0;
    pdc_rx_init(ust->rx_st.pdc, &packet, &npacket);

    ust->currently_xoff = 0;
    ust->xonxoff_enabled = 0;

    usart_init_rs232(ust->usart, &ust->opts, sysclk_get_cpu_hz());
    usart_enable_tx(ust->usart);
    usart_enable_rx(ust->usart);
    pdc_enable_transfer(ust->pdc, PERIPH_PTCR_RXTEN | PERIPH_PTCR_TXTEN);
    usart_enable_interrupt(ust->usart, UART_IER_RXBUFF);
}

/*
    Verify that RX DMA settings don't go outside of DMA buffer
*/
int check_rx(struct usart_state *ust)
{
    struct dma_state *dst = &ust->rx_st;
    if ((ust->pdc->PERIPH_RPR + ust->pdc->PERIPH_RCR) > ust->rx_st.buf_end) {
        pdc_disable_transfer(ust->pdc, PERIPH_PTCR_RXTDIS);
        CURRENT_ERRORS |= CW_ERR_USART_RX;
        return -1;
    } 
    if ((ust->pdc->PERIPH_RNPR + ust->pdc->PERIPH_RNCR) > ust->rx_st.buf_end) {
        pdc_disable_transfer(ust->pdc, PERIPH_PTCR_RXTDIS);
        CURRENT_ERRORS |= CW_ERR_USART_RX;
        return -1;
    }
    return 0;
}

/*
    Verify that TX DMA settings don't go outside of DMA buffer
*/
int check_tx(struct usart_state *ust)
{
    struct dma_state *dst = &ust->tx_st;
    if ((ust->pdc->PERIPH_TPR + ust->pdc->PERIPH_TCR) > ust->tx_st.buf_end) {
        pdc_disable_transfer(ust->pdc, PERIPH_PTCR_TXTDIS);
        CURRENT_ERRORS |= CW_ERR_USART_TX;
        return -1;
    } 
    if ((ust->pdc->PERIPH_TNPR + ust->pdc->PERIPH_TNCR) > ust->tx_st.buf_end) {
        pdc_disable_transfer(ust->pdc, PERIPH_PTCR_TXTDIS);
        CURRENT_ERRORS |= CW_ERR_USART_TX;
        return -1;
    }
    return 0;
}

/*
    Read len data from ust's rx buffer into buf

    Temporarily disables DMA

    WARNING: Because this function disables DMA briefly, if more than one character is received between
        the disabling and enabling of DMA, those characters can be lost. As such, avoid calling this too often

    Returns the length of data read
*/
uint16_t usart_dma_read(struct usart_state *ust, uint8_t *buf, uint16_t len)
{
    struct dma_state *dst = &ust->rx_st;
    len = min(usart_inwait(dst), len);
    if (!len) return 0;
	irqflags_t flags = cpu_irq_save();
    // pdc_disable_transfer(ust->pdc, PERIPH_PTCR_RXTDIS);
    check_rx(ust);

    if (is_usart_rx_overrun(dst)) {
        // scenario 3: overrun, should be handled by others by setting up correct read head?
        pdc_disable_transfer(ust->pdc, PERIPH_PTCR_RXTDIS);
        uint32_t cur_ptr = usart_dma_ptr(dst);
        dst->buf_head = cur_ptr;
        #ifdef USART_SW_FLOW_CONTROL
        // if we support sw flow control, and it's enabled, let the other device know our buffer is full
        if (ust->xonxoff_enabled) {
            uint8_t xon = USART_XON;
            usart_dma_write(ust, &xon, 1);
        }
        #endif
        pdc_enable_transfer(ust->pdc, PERIPH_PTCR_RXTEN);
    }

    // scenario 0: no wrap
    if (!is_dma_wrapped(dst)) {
        // do read from read head until requested end

        memcpy(buf, dst->buf_head_ptr, len);
        
        // update head
        dst->buf_head += len;

        // there's now more space available at beginning of buffer, so update our next packet
        struct pdc_packet npacket;
        npacket.ul_addr = dst->buf_st;
        npacket.ul_size = dst->buf_head - dst->buf_st;
        pdc_disable_transfer(ust->pdc, PERIPH_PTCR_RXTDIS);
        pdc_rx_init(dst->pdc, NULL, &npacket);
        pdc_enable_transfer(ust->pdc, PERIPH_PTCR_RXTEN);
        check_rx(ust);
    } else {
        uint16_t len_to_end = dst->buf_end - dst->buf_head; //USART0_RX_BUF_END - USART0_DMA_STATE.rx_buf_head;

        // scenario 1: wrapped, read stops before end
        if (len_to_end >= len) {
            // don't read past end of buffer
            memcpy(buf, dst->buf_head_ptr, len);
            dst->buf_head += len;

            // there's more space available in buffer, so update current packet to reflect that.
            // can't write to just CNT via API, so need to touch reg directly
            pdc_disable_transfer(ust->pdc, PERIPH_PTCR_RXTDIS);
            dst->pdc->PERIPH_RCR = dst->buf_head - usart_dma_ptr(dst);
            pdc_enable_transfer(ust->pdc, PERIPH_PTCR_RXTEN);
            check_rx(ust);

        // scenario 2: wrapped, read wraps
        } else {
            // read to end of buffer
            memcpy(buf, dst->buf_head_ptr, len_to_end);

            // update current packet to go to end of buffer
            pdc_disable_transfer(ust->pdc, PERIPH_PTCR_RXTDIS);
            dst->pdc->PERIPH_RCR = dst->buf_end - usart_dma_ptr(dst); //USART0_RX_BUF_END - pdc_read_rx_ptr(pdc);

            // read from beginning of next buffer
            memcpy(buf+len_to_end, dst->buf_st_ptr, len - len_to_end);

            dst->buf_head = dst->buf_st + (len - len_to_end); //USART0_DMA_STATE.rx_buf_head = USART0_RX_BUF_ST + (len - len_to_end);

            // there's now space at beginning of buffer, so update our next packet
            struct pdc_packet npacket;
            npacket.ul_addr = dst->buf_st; //USART0_RX_BUF_ST;
            npacket.ul_size = dst->buf_head - dst->buf_st; //USART0_DMA_STATE.rx_buf_head - USART0_RX_BUF_ST;
            pdc_rx_init(dst->pdc, NULL, &npacket);
            pdc_enable_transfer(ust->pdc, PERIPH_PTCR_RXTEN);
            check_rx(ust);
        }
    }
    dst->dropped = 0;
    pdc_enable_transfer(ust->pdc, PERIPH_PTCR_RXTEN);
	cpu_irq_restore(flags);
    return len;
}

/*
    Read len data from buf into ust's TX buffer.

    Temporarily disables DMA

    Returns the length of data written
*/
uint16_t usart_dma_write(struct usart_state *ust, uint8_t *buf, uint16_t len)
{
    struct dma_state *dst = &ust->tx_st;
    len = min(usart_free_space(dst), len);
    if (!len) return 0;
	irqflags_t flags = cpu_irq_save();
    pdc_disable_transfer(ust->pdc, PERIPH_PTCR_TXTDIS); // need to disable tx to avoid issues with read-write on registers

    if (!is_dma_wrapped(dst)) {
        // do math here with registers?
        uint16_t len_to_end = dst->buf_end - dst->buf_head;//(dst->pdc->PERIPH_TPR + dst->pdc->PERIPH_TCR);//dst->buf_end - dst->buf_head;

        // scenario 1, not wrapped, write stops before end
        if (len_to_end > len) {
            //copy into buffer
            memcpy(dst->buf_head_ptr, buf, len);
            if ((dst->buf_head + len) > dst->buf_end){
                volatile int i = 0;
                while(1);
            }

            // increase buf_head by len
            dst->buf_head += len;

            // increase PDC counter by len
            // pdc_disable_transfer(ust->pdc, PERIPH_PTCR_TXTDIS); // need to disable tx to avoid issues with read-write on registers
            dst->pdc->PERIPH_TCR += len;
            // if (!ust->currently_xoff) {
            //     pdc_enable_transfer(ust->pdc, PERIPH_PTCR_TXTEN);
            // }

            if (check_tx(ust)) {
                volatile int j = 256;
            }

        // scenario 2, not wrapped, write goes past end
        } else {
            // copy to end of buffer
            memcpy(dst->buf_head_ptr, buf, len_to_end);
            if ((dst->buf_head + len_to_end) > dst->buf_end){
                volatile int i = 1;
                while(1);
            }

            // update current count to go to end of buffer
            // pdc_disable_transfer(ust->pdc, PERIPH_PTCR_TXTDIS); // need to disable tx to avoid issues with read-write on registers
            dst->pdc->PERIPH_TCR += len_to_end;

            // copy the rest of the input buffer into the DMA buffer
            memcpy(dst->buf_st_ptr, buf + len_to_end, len - len_to_end);

            // point next packet at beginning at set length to go to end of our write
            struct pdc_packet npacket;
            npacket.ul_addr = dst->buf_st;
            npacket.ul_size = len - len_to_end;
            pdc_tx_init(dst->pdc, NULL, &npacket);
            // if (!ust->currently_xoff) {
            //     pdc_enable_transfer(ust->pdc, PERIPH_PTCR_TXTEN);
            // }

            // point head at that new point
            dst->buf_head = dst->buf_st + len - len_to_end;
            if (check_tx(ust)) {
                volatile int j = 87;
            }
        }
    } else {
        // scenario 3, write buffer wraps over end

        // copy new data into buffer (note that len is valid from min() call above)
        memcpy(dst->buf_head_ptr, buf, len);
        if ((dst->buf_head + len) > dst->buf_end){
            volatile int i = 5;
            while(1);
        }

        // increase buf_head by len
        dst->buf_head += len;

        // increase PDC next counter by len
        // pdc_disable_transfer(ust->pdc, PERIPH_PTCR_TXTDIS); // need to disable tx to avoid issues with read-write on registers
        dst->pdc->PERIPH_TNCR += len;
        // if (!ust->currently_xoff) {
        //     pdc_enable_transfer(ust->pdc, PERIPH_PTCR_TXTEN);
        // }

        if (check_tx(ust)) {
            volatile int j = 12;
        }
    }

    if (!ust->currently_xoff) {
        pdc_enable_transfer(ust->pdc, PERIPH_PTCR_TXTEN);
    }
	cpu_irq_restore(flags);
    return len;
}

void naeusart_register_handlers(void)
{
	for (uint8_t i = 0; i < 4; i++) {
		struct usart_state *driver = get_nth_available_driver(i);
		if (!driver) continue;
		usart_enableIO(driver);
	}
    naeusb_add_in_handler(usart_setup_in_received);
    naeusb_add_out_handler(usart_setup_out_received);
}

void usart_enable_dma(struct      usart_state *ust)
{
    pdc_enable_transfer(ust->pdc, PERIPH_PTCR_RXTEN | PERIPH_PTCR_TXTEN);
}

void usart_disable_dma(struct      usart_state *ust)
{
    pdc_disable_transfer(ust->pdc, PERIPH_PTCR_RXTDIS | PERIPH_PTCR_TXTDIS);
}

// Callback for when we receive CDC data. Empty because handled in main()
void my_callback_rx_notify(uint8_t port)
{
}

// Callback for when we send CDC data. Empty because handled in main()
void my_callback_tx_empty(uint8_t port)
{

}

void my_callback_config(uint8_t port, usb_cdc_line_coding_t * cfg)
{
    struct usart_state *driver = get_nth_available_driver(port);
    if (!driver->cdc_enabled) return; // if CDC not enabled, get out of here

    usart_fill_opts(&driver->opts, cfg->dwDTERate, cfg->bCharFormat, cfg->bParityType, cfg->bDataBits);
    setup_usart_dma(driver);
}

void cdc_disable(uint8_t port)
{
    struct usart_state *driver = get_nth_available_driver(port);
    driver->cdc_os_enabled = 0;
}

bool cdc_enable(uint8_t port)
{
    struct usart_state *driver = get_nth_available_driver(port);
    driver->cdc_os_enabled = 1;
    return true;
}

/*
    Handle the condition where our RX buffer is full

    Basically just mark that we lost data and setup DMA to write continuously (or as much as possible)
*/
void handle_rx_usart_overrun_int(struct usart_state *ust)
{
	uint32_t status;
	status = usart_get_status(ust->usart);

    if (status & US_CSR_RXBUFF) {
        pdc_disable_transfer(ust->pdc, PERIPH_PTCR_RXTDIS);
        struct pdc_packet packet;
        struct pdc_packet npacket;
        CURRENT_ERRORS |= CW_ERR_USART_RX_OVERFLOW;

        // mark that packets have been dropped so that we know we're not trying to preserve data
        // just dump into buffer
        ust->rx_st.dropped = 1;

        #ifdef USART_SW_FLOW_CONTROL
        // if we support sw flow control, and it's enabled, let the other device know our buffer is full
        if (ust->xonxoff_enabled) {
            uint8_t xoff = USART_XOFF;
            usart_dma_write(ust, &xoff, 1);
        }
        #endif

        uint32_t cur_ptr = usart_dma_ptr(&ust->rx_st);

        // if we're at the end of buf, start back at beginning
        if (cur_ptr == ust->rx_st.buf_end) {
            packet.ul_addr = ust->rx_st.buf_st;
        } else {
            // otherwise continue to end of buffer
            packet.ul_addr = cur_ptr;
        }
        packet.ul_size = ust->rx_st.buf_end - packet.ul_addr;

        // make the next packet run to the end of this packet
        npacket.ul_addr = ust->rx_st.buf_st;
        npacket.ul_size = ust->rx_st.buf_sz;
        pdc_rx_init(ust->rx_st.pdc, &packet, &npacket);
        pdc_enable_transfer(ust->pdc, PERIPH_PTCR_RXTEN);
    }
}

// in CDC to avoid running usart_dma_read/write() too often
static uint8_t cdctmp[512];
void handle_cdc_txrx(void)
{
    for (uint8_t i = 0; i < 4; i++) {

        // note: be very careful here, these calls could do weird things
        // need to test well, esp with stream mode
        struct usart_state *driver = get_nth_available_driver(i);
        if (!driver) continue; // skip if driver doesn't exist
        if (!driver->cdc_supported) continue; //skip if CDC not supported
        if (!driver->cdc_os_enabled) continue; //skip if OS isn't expecting CDC to work
        if (!driver->cdc_enabled) continue; //skip if CDC mode not enabled


        /*
            NOTE: For RX, doing usart_dma_read() at higher baud rates can cause us to miss characters
            This is because that function disables the DMA while we do stuff to the register, meaning
            if we get more than one character in that time, it will be lost

            There should be some sort of tradeoff between the length of this delay and characters lost,
            where longer delays will perform better up to a point
        */
        // minimum between chars available, free CDC txt space, and size of cdctmp buf
        uint16_t num_rx = usart_inwait(&driver->rx_st);
        if (num_rx) {
            for (volatile uint32_t j = 0; j < 10000; j++);
            num_rx = min(usart_inwait(&driver->rx_st), min(udi_cdc_multi_get_free_tx_buffer(i), sizeof(cdctmp)));
            num_rx = usart_dma_read(driver, cdctmp, num_rx);
            udi_cdc_multi_write_buf(i, cdctmp, num_rx);
        }

        // minimum between chars free, free tx space, and size of cdctmp buf
        // while (udi_cdc_multi_is_rx_ready(i) && (usart_free_space(&driver->tx_st))) {
        //     uint8_t c = udi_cdc_multi_getc(i);
        //     usart_dma_write(driver, &c, 1);
        // }
        uint16_t num_tx = udi_cdc_multi_get_nb_received_data(i);//usart_free_space(&driver->tx_st);
        if (num_tx) {
            for (volatile uint32_t j = 0; j < 2000; j++);
            num_tx = min(num_tx, min(usart_free_space(&driver->tx_st), sizeof(cdctmp)));
            udi_cdc_multi_read_no_polling(i, cdctmp, num_tx);
            usart_dma_write(driver, cdctmp, num_tx);
        }

        // volatile uint32_t i = 0;
        // for (; i < 5000; i++);
    }
}

void handle_sw_flow_control(void)
{
    #ifdef USART_SW_FLOW_CONTROL
    for (uint8_t i = 0; i < 4; i++) {
        struct usart_state *driver = get_nth_available_driver(i);
        if (!driver) continue; // skip if driver doesn't exist
        if (!driver->xonxoff_enabled) continue;


        uint32_t rx_ptr = usart_dma_ptr(&driver->rx_st);
        while (driver->xonxoff_last_rx != rx_ptr) {

            uint8_t cur_char = *driver->xonxoff_last_rx_ptr;
            if (cur_char == USART_XON) {
                //doxon
                pdc_enable_transfer(driver->pdc, PERIPH_PTCR_TXTEN);
                driver->currently_xoff = 0;
            } else if (cur_char == USART_XOFF) {
                // doxoff
                pdc_disable_transfer(driver->pdc, PERIPH_PTCR_TXTDIS);
                driver->currently_xoff = 1;
            }

            if (driver->xonxoff_last_rx++ > driver->rx_st.buf_end) {
                driver->xonxoff_last_rx = driver->rx_st.buf_st;
            }
        }
    }
    #endif
}
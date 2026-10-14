#include <asf.h>
#include <string.h>
#include "circbuffer.h"
#include "usart_driver.h"
#include "naeusb_default.h"
#include "usart.h"
#include "usb_protocol_cdc.h"
#include "naeusb_usart.h"
#include "naeusb_usart_util.h"
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

static inline uint32_t usart_dma_ptr(struct dma_state *dst)
{
    if (dst->is_tx) {
        return pdc_read_tx_ptr(dst->pdc);
    } else {
        return pdc_read_rx_ptr(dst->pdc);
    }
}

static inline uint32_t usart_dma_nptr(struct dma_state *dst)
{
    if (dst->is_tx) {
        return pdc_read_tx_next_ptr(dst->pdc);
    } else {
        return pdc_read_rx_next_ptr(dst->pdc);
    }
}

static inline char is_dma_wrapped(struct dma_state *dst)
{
    if (dst->is_tx) {
        return dst->pdc->PERIPH_TNCR != 0;
    } else {
        return (dst->buf_head > usart_dma_ptr(dst)) || ((dst->buf_head != dst->buf_st) && (dst->dropped));
    }
}

// 
uint16_t usart_inwait(struct dma_state *dst)
{
    if (is_usart_rx_overrun(dst)) {
        return dst->buf_sz; // we had overrun, so buffer is full
    }

    if (dst->is_tx) {
        if (!is_dma_wrapped(dst)) {
            return dst->buf_head - usart_dma_ptr(dst);
        } else {
            return (dst->buf_end - usart_dma_ptr(dst)) + (dst->buf_head - dst->buf_st);
        }
    } else {
        // if (dst->buf_head == usart_dma_ptr(dst)) {
        //     return dst->buf_sz;
        // }
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
    return USART_BUF_SZ - usart_inwait(dst);
}

int is_usart_rx_overrun(struct dma_state *dst)
{
    return (usart_is_rx_buf_full(dst->usart) || (dst->dropped > 0));
}

static void setup_dma_state(struct dma_state *dst)
{
    dst->buf_st_ptr = dst->buf;
    dst->buf_sz = USART_BUF_SZ;

    dst->buf_head = dst->buf_st;

    dst->buf_end = dst->buf_st + dst->buf_sz;
    dst->dropped = 0;
}

void setup_usart_dma(struct usart_state *ust)
{
    struct pdc_packet packet;
    struct pdc_packet npacket;

    // setup tx
    setup_dma_state(&ust->tx_st);
    packet.ul_addr = ust->tx_st.buf_st;
    packet.ul_size = 0;
    pdc_tx_init(ust->tx_st.pdc, &packet, NULL);

    // setup rx
    setup_dma_state(&ust->rx_st);
    packet.ul_addr = ust->rx_st.buf_st;
    packet.ul_size = ust->rx_st.buf_sz;
    npacket.ul_addr = ust->rx_st.buf_st;
    npacket.ul_size = 0;
    pdc_rx_init(ust->rx_st.pdc, &packet, &npacket);


    usart_init_rs232(ust->usart, &ust->opts, sysclk_get_cpu_hz());
    usart_enable_tx(ust->usart);
    usart_enable_rx(ust->usart);
    pdc_enable_transfer(ust->pdc, PERIPH_PTCR_RXTEN | PERIPH_PTCR_TXTEN);
    usart_enable_interrupt(ust->usart, UART_IER_RXBUFF);
}

void check_rx(struct usart_state *ust)
{
    struct dma_state *dst = &ust->rx_st;
    if ((ust->pdc->PERIPH_RPR + ust->pdc->PERIPH_RCR) > ust->rx_st.buf_end) {
        pdc_disable_transfer(ust->pdc, 0xFFFFFFFFF);
        while(1);
    } 
    if ((ust->pdc->PERIPH_RNPR + ust->pdc->PERIPH_RNCR) > ust->rx_st.buf_end) {
        pdc_disable_transfer(ust->pdc, 0xFFFFFFFFF);
        while(1);
    }
}

// TODO: Check edge cases
uint16_t usart_dma_read(struct usart_state *ust, uint8_t *buf, uint16_t len)
{
    struct dma_state *dst = &ust->rx_st;
    len = min(usart_inwait(dst), len);
    check_rx(ust);

    if (is_usart_rx_overrun(dst)) {
        // scenario 3: overrun, should be handled by others by setting up correct read head?
        uint32_t cur_ptr = usart_dma_ptr(dst);
        dst->buf_head = cur_ptr;
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
        pdc_rx_init(dst->pdc, NULL, &npacket);
        check_rx(ust);
    } else {
        uint16_t len_to_end = dst->buf_end - dst->buf_head; //USART0_RX_BUF_END - USART0_DMA_STATE.rx_buf_head;

        // scenario 1: wrapped, read stops before end
        // TODO: verif that this can't go past the end
        if (len_to_end >= len) {
            // don't read past end of buffer
            memcpy(buf, dst->buf_head_ptr, len);
            dst->buf_head += len;

            // there's more space available in buffer, so update current packet to reflect that.
            // can't write to just CNT via API, so need to touch reg directly
            dst->pdc->PERIPH_RCR = dst->buf_head - usart_dma_ptr(dst);
            check_rx(ust);

        // scenario 2: wrapped, read wraps
        } else {
            // read to end of buffer
            memcpy(buf, dst->buf_head_ptr, len_to_end);

            // update current packet to go to end of buffer
            dst->pdc->PERIPH_RCR = dst->buf_end - usart_dma_ptr(dst); //USART0_RX_BUF_END - pdc_read_rx_ptr(pdc);

            // read from beginning of next buffer
            memcpy(buf+len_to_end, dst->buf_st_ptr, len - len_to_end);

            dst->buf_head = dst->buf_st + (len - len_to_end); //USART0_DMA_STATE.rx_buf_head = USART0_RX_BUF_ST + (len - len_to_end);

            // there's now space at beginning of buffer, so update our next packet
            struct pdc_packet npacket;
            npacket.ul_addr = dst->buf_st; //USART0_RX_BUF_ST;
            npacket.ul_size = dst->buf_head - dst->buf_st; //USART0_DMA_STATE.rx_buf_head - USART0_RX_BUF_ST;
            pdc_rx_init(dst->pdc, NULL, &npacket);
            check_rx(ust);
        }
    }
    dst->dropped = 0;
    return len;
}

// TODO: Check edge cases
void usart_dma_write(struct usart_state *ust, uint8_t *buf, uint16_t len)
{
    struct dma_state *dst = &ust->tx_st;
    len = min(usart_free_space(dst), len);
    if (!is_dma_wrapped(dst)) {
        uint16_t len_to_end = dst->buf_end - dst->buf_head;

        // scenario 1, not wrapped, write stops before end
        if (len_to_end >= len) {
            //copy into buffer
            memcpy(dst->buf_head_ptr, buf, len);

            // increase buf_head by len
            dst->buf_head += len;

            // increase PDC counter by len
            dst->pdc->PERIPH_TCR += len;

        // scenario 2, not wrapped, write goes past end
        } else {
            // copy to end of buffer
            memcpy(dst->buf_head_ptr, buf, len_to_end);

            // update current count to go to end of buffer
            dst->pdc->PERIPH_TCR += len_to_end;

            // copy the rest of the input buffer into the DMA buffer
            memcpy(dst->buf_st_ptr, buf + len_to_end, len - len_to_end);

            // point next packet at beginning at set length to go to end of our write
            struct pdc_packet npacket;
            npacket.ul_addr = dst->buf_st;
            npacket.ul_size = len - len_to_end;
            pdc_tx_init(dst->pdc, NULL, &npacket);

            // point head at that new point
            dst->buf_head = dst->buf_st + len - len_to_end;
        }
    } else {
        // scenario 3, write buffer wraps over end

        // copy new data into buffer (note that len is valid from min() call above)
        memcpy(dst->buf_head_ptr, buf, len);

        // increase buf_head by len
        dst->buf_head += len;

        // increase PDC next counter by len
        dst->pdc->PERIPH_TNCR += len;
    }
}

// naeusart because I wouldn't be surpised if usart_register_handlers collides with something
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

void usart_enable_dma(struct      usart_state *dst)
{

}

void usart_disable_dma(struct      usart_state *dst)
{

}

#define CDC_BUF_SZ 64

void my_callback_rx_notify(uint8_t port)
{
    irqflags_t flags = cpu_irq_save();
    uint8_t tmp[64];
    struct usart_state *ust = get_nth_available_driver(port);
    volatile iram_size_t num_char = min(udi_cdc_multi_get_nb_received_data(port), 64);

    while (num_char >= 0) {
        num_char = min(udi_cdc_multi_get_nb_received_data(port), 64);
        udi_cdc_multi_read_buf(port, tmp, min(num_char, 64));
        usart_dma_write(ust, tmp, num_char);
    }

    cpu_irq_restore(flags);
}

void my_callback_config(uint8_t port, usb_cdc_line_coding_t * cfg)
{

}

void cdc_disable(uint8_t port)
{

}

bool cdc_enable(uint8_t port)
{
    return true;
}

void handle_rx_usart_overrun_int(struct usart_state *ust)
{
	uint32_t status;
	status = usart_get_status(ust->usart);

    if (status & US_CSR_RXBUFF) {
        struct pdc_packet packet;
        struct pdc_packet npacket;

        // mark that packets have been dropped so that we know we're not trying to preserve data
        // just dump into buffer
        ust->rx_st.dropped = 1;

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
    }
}
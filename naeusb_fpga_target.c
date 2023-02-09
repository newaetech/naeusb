#include "naeusb_fpga_target.h"
#include "fpgautil_io.h"
#include "fpga_program.h"
#include "fpgaspi_program.h"
#include "cdce906.h"
#include "tps56520.h"
#include "naeusb_default.h"

#define USB_STATUS_NONE       0
#define USB_STATUS_PARAMWRONG 1
#define USB_STATUS_OK         2
#define USB_STATUS_COMMERR    3
#define USB_STATUS_CSFAIL     4

/* Commands for GPIO config mode */
#define CONFIG_PIN_INPUT     0x01
#define CONFIG_PIN_OUTPUT    0x02
#define CONFIG_PIN_SPI1_MOSI 0x10
#define CONFIG_PIN_SPI1_MISO 0x11
#define CONFIG_PIN_SPI1_SCK  0x12
#define CONFIG_PIN_SPI1_CS   0x13

volatile uint8_t I2C_STATUS = 0;

static uint8_t fpgaspi_data_buffer[64];
static int fpga_spi_databuffer_len = 0;

static uint8_t spi1util_data_buffer[64];
static int spi1util_databuffer_len = 0;

blockep_usage_t blockendpoint_usage = bep_emem;

static uint8_t * ctrlmemread_buf;
static unsigned int ctrlmemread_size;

static uint8_t cdce906_status;
static uint8_t cdce906_data;

uint32_t sam3u_mem[256];

uint32_t bulk_fpga_write_addr = 0;

void main_vendor_bulk_out_received(udd_ep_status_t status,
        iram_size_t nb_transfered, udd_ep_id_t ep);
bool fpga_target_setup_in_received(void);

void ctrl_readmem_bulk(void){
    uint32_t buflen = *(CTRLBUFFER_WORDPTR);	
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    FPGA_setlock(fpga_blockin);

    /* Do memory read */	
    udi_vendor_bulk_in_run(
            (uint8_t *) PSRAM_BASE_ADDRESS + address,
            buflen,
            main_vendor_bulk_in_received
            );	
}

void ctrl_readmem_ctrl(void){
    uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    FPGA_setlock(fpga_ctrlmem);

    /* Do memory read */
    ctrlmemread_buf = (uint8_t *) PSRAM_BASE_ADDRESS + address;

    /* Set size to read */
    ctrlmemread_size = buflen;

    /* Start Transaction */
}

void ctrl_writemem_ctrl_sam3u(void){
    uint32_t buflen = *(CTRLBUFFER_WORDPTR) - 4; // remove the first 4 bytes of the payload who contain the flags
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);
    uint32_t flags = *(CTRLBUFFER_WORDPTR + 2);

    uint8_t * ctrlbuf_payload = (uint8_t *)(CTRLBUFFER_WORDPTR + 3);
    uint8_t * sam3u_mem_b = (uint8_t *) sam3u_mem;

    /* Do memory write */
    for(unsigned int i = 0; i < buflen; i++){
        sam3u_mem_b[i+address] = ctrlbuf_payload[i];
    }

    if ( flags & 0x1 ){ // encryptions have been requested
        uint32_t seed = sam3u_mem[0]; // load the seed at addr 0

        for(unsigned int b = 0; b < (flags >> 16); b++){
            FPGA_setlock(fpga_generic);

            // set the inputs key if needed
            if ((flags >> 1) & 0x1){ // write the key
                for(unsigned int j = 0; j < 16; j++){
                    xram[j+0x400+0x100] = seed >> 24;
                    seed += (seed*seed) | 0x5;
                }
            }
            // set the inputs pt if needed
            if ((flags >> 2) & 0x1){ // write the pts
                for(unsigned int j = 0; j < 16; j++){
                    xram[j+0x400+0x200] = seed >> 24;
                    seed += (seed*seed) | 0x5;
                }
            }
            FPGA_setlock(fpga_unlocked);

            gpio_set_pin_high(FPGA_TRIGGER_GPIO);
            delay_cycles(50);
            gpio_set_pin_low(FPGA_TRIGGER_GPIO);
        }

    }

}

static void ctrl_spi1util(void){
	
    
	switch(udd_g_ctrlreq.req.wValue){
		case 0xA0:
			spi1util_init();			
			break;
			
		case 0xA1:
			spi1util_deinit();
			break;
			
		case 0xA2:
			spi1util_cs_low();
			break;

		case 0xA3:
			spi1util_cs_high();
			break;

		case 0xA4:
			//Catch heartbleed-style error
			if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
				return;
			}

			if (udd_g_ctrlreq.req.wLength > sizeof(fpgaspi_data_buffer)){
				return;
			}
			spi1util_databuffer_len = udd_g_ctrlreq.req.wLength;
			for (int i = 0; i < udd_g_ctrlreq.req.wLength; i++){
				spi1util_data_buffer[i] = spi1util_xferbyte(udd_g_ctrlreq.payload[i]);
			}
			break;

		default:
			break;
	}
}

static void ctrl_progfpgaspi(void){
	
	switch(udd_g_ctrlreq.req.wValue){
		case 0xA0:
			fpgaspi_program_init();			
			break;
			
		case 0xA1:
			fpgaspi_program_deinit();
			break;
			
		case 0xA2:
			fpgaspi_cs_low();
			break;

		case 0xA3:
			fpgaspi_cs_high();
			break;

		case 0xA4:
			//Catch heartbleed-style error
			if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
				return;
			}

			if (udd_g_ctrlreq.req.wLength > sizeof(fpgaspi_data_buffer)){
				return;
			}
			fpga_spi_databuffer_len = udd_g_ctrlreq.req.wLength;
			for (int i = 0; i < udd_g_ctrlreq.req.wLength; i++){
				fpgaspi_data_buffer[i] = fpgaspi_xferbyte(udd_g_ctrlreq.payload[i]);
			}
			break;

		default:
			break;
	}
}

#ifdef CW_PROG_XMEGA
void ctrl_xmega_program_void(void)
{
    XPROGProtocol_Command();
}
#endif

void ctrl_writemem_ctrl(void){
    uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    uint8_t * ctrlbuf_payload = (uint8_t *)(CTRLBUFFER_WORDPTR + 2);

    //printf("Writing to %x, %d\n", address, buflen);

    FPGA_setlock(fpga_generic);

    /* Start Transaction */

    /* Do memory write */
    for(unsigned int i = 0; i < buflen; i++){
        xram[i+address] = ctrlbuf_payload[i];
    }

    FPGA_setlock(fpga_unlocked);
}

void ctrl_writemem_bulk(void){
    //uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    //uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    FPGA_setlock(fpga_blockout);
    bulk_fpga_write_addr = *(CTRLBUFFER_WORDPTR + 1);

    /* Set address */
    //Not required - this is done automatically via the XMEM interface
    //instead of using a "cheater" port.
    udi_vendor_bulk_out_run(
            xram + bulk_fpga_write_addr,
            0xFFFFFFFF,
            NULL);

    /* Transaction done in generic callback */
}

void main_vendor_bulk_in_received(udd_ep_status_t status,
        iram_size_t nb_transfered, udd_ep_id_t ep)
{
    UNUSED(nb_transfered);
    UNUSED(ep);
    if (UDD_EP_TRANSFER_OK != status) {
        return; // Transfer aborted/error
    }	

    if (FPGA_lockstatus() == fpga_blockin){		
        FPGA_setlock(fpga_unlocked);
    }
}

void ctrl_progfpga_bulk(void){
    uint32_t prog_freq = 20E6;
    switch(udd_g_ctrlreq.req.wValue){
        case 0xA0:
            if (udd_g_ctrlreq.req.wLength == 4) {
                prog_freq = *(CTRLBUFFER_WORDPTR);
            }
            fpga_program_setup1(prog_freq);			
            break;

        case 0xA1:
            /* Waiting on data... */
            fpga_program_setup2();
            blockendpoint_usage = bep_fpgabitstream;
            break;

        case 0xA2:
            /* Done */
            blockendpoint_usage = bep_emem;
            break;

        default:
            break;
    }
}

void main_vendor_bulk_out_received(udd_ep_status_t status,
        iram_size_t nb_transfered, udd_ep_id_t ep)
{
    UNUSED(ep);
    if (UDD_EP_TRANSFER_OK != status) {
        // Transfer aborted

        //restart
        // udi_vendor_bulk_out_run(
        //         main_buf_loopback,
        //         sizeof(main_buf_loopback),
        //         main_vendor_bulk_out_received);

        return;
    }

    if (blockendpoint_usage == bep_emem){
        for(unsigned int i = 0; i < nb_transfered; i++){
            xram[bulk_fpga_write_addr++] = main_buf_loopback[i];
        }

        if (FPGA_lockstatus() == fpga_blockout){
            FPGA_setlock(fpga_unlocked);
        }
    } else if (blockendpoint_usage == bep_fpgabitstream) {

        /* Send byte to FPGA - this could eventually be done via SPI */		
        for(unsigned int i = 0; i < nb_transfered; i++){
            fpga_program_sendbyte(main_buf_loopback[i]);
        }

        // FPGA_CCLK_LOW();
    }

    //printf("BULKOUT: %d bytes\n", (int)nb_transfered);

    // udi_vendor_bulk_out_run(
    //         main_buf_loopback,
    //         sizeof(main_buf_loopback),
    //         main_vendor_bulk_out_received);
}

static void ctrl_cdce906_cb(void)
{
    //Catch heartbleed-style error
    if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
        return;
    }

    cdce906_status = USB_STATUS_NONE;

    if (udd_g_ctrlreq.req.wLength < 3){
        cdce906_status = USB_STATUS_PARAMWRONG;
        return;
    }

    cdce906_status = USB_STATUS_COMMERR;
    if (udd_g_ctrlreq.payload[0] == 0x00){
        if (cdce906_read(udd_g_ctrlreq.payload[1], &cdce906_data)){
            cdce906_status = USB_STATUS_OK;
        }

    } else if (udd_g_ctrlreq.payload[0] == 0x01){
        if (cdce906_write(udd_g_ctrlreq.payload[1], udd_g_ctrlreq.payload[2])){
            cdce906_status = USB_STATUS_OK;
        }
    } else {
        cdce906_status = USB_STATUS_PARAMWRONG;
        return;
    }
}

static uint8_t vccint_status = 0;
static uint16_t vccint_setting = 1000;

static void ctrl_vccint_cb(void)
{
    //Catch heartbleed-style error
    if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
        return;
    }

    vccint_status = USB_STATUS_NONE;

    if ((udd_g_ctrlreq.payload[0] ^ udd_g_ctrlreq.payload[1] ^ 0xAE) != (udd_g_ctrlreq.payload[2])){
        vccint_status = USB_STATUS_PARAMWRONG;
        return;
    }

    if (udd_g_ctrlreq.req.wLength < 3){
        vccint_status = USB_STATUS_CSFAIL;
        return;
    }

    uint16_t vcctemp = (udd_g_ctrlreq.payload[0]) | (udd_g_ctrlreq.payload[1] << 8);
    if ((vcctemp < 600) || (vcctemp > 1200)){
        vccint_status = USB_STATUS_PARAMWRONG;
        return;
    }

    vccint_status = USB_STATUS_COMMERR;

    if (tps56520_set(vcctemp)){
        vccint_setting = vcctemp;
        vccint_status = USB_STATUS_OK;
    }

    return;
}

static void ctrl_fpgaioutil(void) {
	
    if (udd_g_ctrlreq.req.wLength != 2){
        return;
    }

    int pin = udd_g_ctrlreq.payload[0];
    int config = udd_g_ctrlreq.payload[1];

#if USB_DEVICE_PRODUCT_ID == 0xC310
    if ((pin < 0) || (pin > 106)){
#else
	if ((pin < 0) || (pin > 95)){
#endif
        return;
    }

	switch(udd_g_ctrlreq.req.wValue){
		case 0xA0: /* Configure IO Pin */
            switch(config)
            {
                case CONFIG_PIN_INPUT:
                    gpio_configure_pin(pin, PIO_INPUT);
                    break;
                case CONFIG_PIN_OUTPUT:
                    gpio_configure_pin(pin, PIO_OUTPUT_1);
                    break;
                case CONFIG_PIN_SPI1_MOSI:
                    if(pin_spi1_mosi > -1){
                        gpio_configure_pin(pin_spi1_mosi, PIO_DEFAULT);
                    }
                    gpio_configure_pin(pin, PIO_OUTPUT_0);
                    pin_spi1_mosi = pin;
                    break;
                case CONFIG_PIN_SPI1_MISO:
                    if(pin_spi1_miso > -1){
                        gpio_configure_pin(pin_spi1_miso, PIO_DEFAULT);
                    }
                    gpio_configure_pin(pin, PIO_INPUT);
                    pin_spi1_miso = pin;
                    break;
                case CONFIG_PIN_SPI1_SCK:
                    if(pin_spi1_sck > -1){
                        gpio_configure_pin(pin_spi1_sck, PIO_DEFAULT);
                    }
                    gpio_configure_pin(pin, PIO_OUTPUT_0);
                    pin_spi1_sck = pin;
                    break;
                case CONFIG_PIN_SPI1_CS:
                    if(pin_spi1_cs > -1){
                        gpio_configure_pin(pin_spi1_cs, PIO_DEFAULT);
                    }
                    gpio_configure_pin(pin, PIO_OUTPUT_1);
                    pin_spi1_cs = pin;                    
                    break;
                default:
                    //oops?
                    gpio_configure_pin(pin, PIO_DEFAULT);
                    break;
            }
			break;
			
		case 0xA1: /* Release IO Pin */
			//todo?
            gpio_configure_pin(pin, PIO_DEFAULT);
			break;

        case 0xA2: /* Set IO Pin state */
            if (config == 0){
                gpio_set_pin_low(pin);
            }

            if (config == 1){
                gpio_set_pin_high(pin);
            }
            break;

		default:
			break;
	}
}

bool fpga_target_setup_in_received(void)
{
    switch(udd_g_ctrlreq.req.bRequest){
        case REQ_MEMREAD_CTRL:
            udd_g_ctrlreq.payload = ctrlmemread_buf;
            udd_g_ctrlreq.payload_size = ctrlmemread_size;
            ctrlmemread_size = 0;

            // if (FPGA_lockstatus() == fpga_ctrlmem){
            //     FPGA_setlock(fpga_unlocked);
            // }

            return true;
            break;

        case REQ_FPGA_STATUS:
            respbuf[0] = FPGA_ISDONE();
            respbuf[1] = FPGA_INITB_STATUS();
            respbuf[2] = 0;
            respbuf[3] = 0;
            udd_g_ctrlreq.payload = respbuf;
            udd_g_ctrlreq.payload_size = 4;
            return true;
            break;
#ifdef CW_PROG_XMEGA
        case REQ_XMEGA_PROGRAM:
            return XPROGProtocol_Command();
            break;
#endif

        case REQ_CDCE906:
            respbuf[0] = cdce906_status;
            respbuf[1] = cdce906_data;
            udd_g_ctrlreq.payload = respbuf;
            udd_g_ctrlreq.payload_size = 2;
            return true;
            break;

        case REQ_VCCINT:
            respbuf[0] = vccint_status;
            respbuf[1] = (uint8_t)vccint_setting;
            respbuf[2] = (uint8_t)(vccint_setting >> 8);
            udd_g_ctrlreq.payload = respbuf;
            udd_g_ctrlreq.payload_size = 3;
            return true;
            break;	

		case REQ_FPGASPI_PROGRAM:						
			if (udd_g_ctrlreq.req.wLength > sizeof(fpgaspi_data_buffer))
            {
                return false;
            }
			udd_g_ctrlreq.payload = fpgaspi_data_buffer;
			udd_g_ctrlreq.payload_size = udd_g_ctrlreq.req.wLength;
			return true;
			break;

        case FREQ_FPGASPI1_XFER:
 			if (udd_g_ctrlreq.req.wLength > sizeof(spi1util_data_buffer))
            {
                return false;
            }
			udd_g_ctrlreq.payload = spi1util_data_buffer;
			udd_g_ctrlreq.payload_size = udd_g_ctrlreq.req.wLength;
			return true;
			break;    
			
		case REQ_FPGAIO_UTIL:
			0;
			int pin;
			pin = udd_g_ctrlreq.req.wValue & 0xFF;
			respbuf[0] = gpio_pin_is_high(pin);
			udd_g_ctrlreq.payload = respbuf;
			udd_g_ctrlreq.payload_size = 1;
			return true;
			break;

        default:
            return false;
    }

}

#ifndef kill_fpga_power
#define kill_fpga_power()
#endif

#ifndef enable_fpga_power
#define enable_fpga_power()
#endif

void fpga_target_sam_cfg_out(void)
{
	switch (udd_g_ctrlreq.req.wValue & 0xFF) {
	    case 0x04:
	    gpio_configure_pin(PIN_PCK0, PIO_OUTPUT_0);
	    break;

	    /* Turn on FPGA Clock */
	    case 0x05:
	    gpio_configure_pin(PIN_PCK0, PIN_PCK0_FLAGS);
	    break;

	    /* Toggle trigger pin */
	    case 0x06:
	    gpio_set_pin_high(FPGA_TRIGGER_GPIO);
	    delay_cycles(250);
	    gpio_set_pin_low(FPGA_TRIGGER_GPIO);
	    break;
	        
	    /* Turn target power off */
	    case 0x07:
	    kill_fpga_power();
	    break;
	        
	    /* Turn target power on */
	    case 0x08:
	    enable_fpga_power();
	    break;
	}
}

bool fpga_target_setup_out_received(void)
{
    blockendpoint_usage = bep_emem;
    switch(udd_g_ctrlreq.req.bRequest){
		case REQ_SAM_CFG:
			0;
			uint16_t wVal = udd_g_ctrlreq.req.wValue & 0xFF;
			if ((wVal > 0x03) && (wVal < 0x10)) {
				udd_g_ctrlreq.callback = fpga_target_sam_cfg_out;
				return true;
			} else {
				return false;
			}
			break;
        /* Memory Read */
        case REQ_MEMREAD_BULK:
            udd_g_ctrlreq.callback = ctrl_readmem_bulk;
            return true;
        case REQ_MEMREAD_CTRL:
            udd_g_ctrlreq.callback = ctrl_readmem_ctrl;
            return true;	

            /* Memory Write */
        case REQ_MEMWRITE_BULK:
            udd_g_ctrlreq.callback = ctrl_writemem_bulk;
            return true;

        case REQ_MEMWRITE_CTRL:
            udd_g_ctrlreq.callback = ctrl_writemem_ctrl;
            return true;		

        case REQ_MEMWRITE_CTRL_SAMU3:
            udd_g_ctrlreq.callback = ctrl_writemem_ctrl_sam3u;
            return true;		

            /* Send bitstream to FPGA */
        case REQ_FPGA_PROGRAM:
            udd_g_ctrlreq.callback = ctrl_progfpga_bulk;
            return true;

            /* XMEGA Programming */
#ifdef CW_PROG_XMEGA
        case REQ_XMEGA_PROGRAM:
            /*
               udd_g_ctrlreq.payload = xmegabuffer;
               udd_g_ctrlreq.payload_size = min(udd_g_ctrlreq.req.wLength,	sizeof(xmegabuffer));
               */
            udd_g_ctrlreq.callback = ctrl_xmega_program_void;
            return true;
#endif

        case REQ_CDCE906:
            udd_g_ctrlreq.callback = ctrl_cdce906_cb;
            return true;

            /* VCC-INT Setting */
        case REQ_VCCINT:
            udd_g_ctrlreq.callback = ctrl_vccint_cb;
            return true;

		/* Send SPI commands to FPGA-attached SPI flash */
		case REQ_FPGASPI_PROGRAM:
			udd_g_ctrlreq.callback = ctrl_progfpgaspi;
			return true;

        /* IO Util Setup */
        case REQ_FPGAIO_UTIL:
            udd_g_ctrlreq.callback = ctrl_fpgaioutil;
            return true;

        /* Bit-Banged SPI1 */
        case FREQ_FPGASPI1_XFER:
            udd_g_ctrlreq.callback = ctrl_spi1util;
            return true;

        default:
            return false;
    }					
}

void fpga_target_register_handlers(void)
{
    naeusb_add_in_handler(fpga_target_setup_in_received);
    naeusb_add_out_handler(fpga_target_setup_out_received);
}
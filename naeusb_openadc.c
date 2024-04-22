#include "naeusb_openadc.h"
#define REQ_MEMREAD_BULK 0x10
#define REQ_MEMWRITE_BULK 0x11
#define REQ_MEMREAD_CTRL 0x12
#define REQ_MEMWRITE_CTRL 0x13

#define REQ_FPGA_STATUS 0x15
#define REQ_FPGA_PROGRAM 0x16

#define REQ_FPGA_RESET 0x25

blockep_usage_t blockendpoint_usage = bep_emem;

static uint8_t * ctrlmemread_buf;
static unsigned int ctrlmemread_size;

void main_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);
void main_vendor_bulk_out_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep);

void openadc_progfpga_bulk(void){
	uint32_t prog_freq = 10E6;
    switch(udd_g_ctrlreq.req.wValue & 0xFF){
    case 0xA0:
        
        if (udd_g_ctrlreq.req.wLength == 4) {
            prog_freq = *(CTRLBUFFER_WORDPTR);
        }
        fpga_program_setup1(prog_freq);
        break;

    case 0xA1:
        /* Waiting on data... */
        fpga_program_setup2();
        // blockendpoint_usage = bep_fpgabitstream;
        break;

    case 0xA2:
        /* Done */
        // blockendpoint_usage = bep_emem;
        fpga_program_finish();
        break;

    case 0xB0:
        if (udd_g_ctrlreq.req.wLength == 4) {
            prog_freq = *(CTRLBUFFER_WORDPTR);
        }

        //nRST gets passed through on FPGA as well, we set this high and hope that is
        //correct for target.
        #ifdef PIN_TARG_NRST_GPIO
        gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT));
        gpio_set_pin_high(PIN_TARG_NRST_GPIO);
        #endif
        fpga_program_spi_setup1(prog_freq);
        break;

    case 0xB1:
        blockendpoint_usage = bep_fpgabitstreamspi;
        break;
    
    case 0xB2:
        /* Done */
        blockendpoint_usage = bep_emem;
        //Can't set this back to input until after FPGA stops listening, for now we ignore it
        //gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_INPUT | PIO_DEFAULT));
        break;
    default:
        break;
    }
}

void openadc_readmem_bulk(void)
{
    uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    FPGA_releaselock();
    while(!FPGA_setlock(fpga_blockin));

    FPGA_setaddr(address);
    if  (!udi_vendor_bulk_in_run(
        (uint8_t *) PSRAM_BASE_ADDRESS,
        buflen,
        main_vendor_bulk_in_received
        )) {
            //abort
        }
}

void openadc_writemem_bulk(void)
{
    //uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    // TODO: see block in
    FPGA_releaselock();
    while(!FPGA_setlock(fpga_blockout));

    /* Set address */
    FPGA_setaddr(address);
	udd_ep_abort(UDI_VENDOR_EP_BULK_OUT);

    /* Transaction done in generic callback */
    udi_vendor_bulk_out_run(
        main_buf_loopback,
        sizeof(main_buf_loopback),
        main_vendor_bulk_out_received);

}

void openadc_readmem_ctrl(void)
{
    uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    FPGA_releaselock();
    while(!FPGA_setlock(fpga_ctrlmem));

    /* Set address */
    FPGA_setaddr(address);

    /* Do memory read */
    ctrlmemread_buf = (uint8_t *) PSRAM_BASE_ADDRESS;

    /* Set size to read */
    ctrlmemread_size = buflen;
    // udi_vendor_bulk_out_run(
    //     xram,
    //     0xFFFFFFFF,
    //     NULL);

    /* Start Transaction */
    
}

void openadc_writemem_ctrl(void)
{
    uint32_t buflen = *(CTRLBUFFER_WORDPTR);
    uint32_t address = *(CTRLBUFFER_WORDPTR + 1);

    uint8_t * ctrlbuf_payload = (uint8_t *)(CTRLBUFFER_WORDPTR + 2);

    //printf("Writing to %x, %d\n", address, buflen);

    FPGA_releaselock();
    while(!FPGA_setlock(fpga_generic));

    /* Set address */
    FPGA_setaddr(address);

    /* Start Transaction */

    /* Do memory write */
    for(unsigned int i = 0; i < buflen; i++){
        xram[i] = ctrlbuf_payload[i];
    }

    FPGA_releaselock();

}

void main_vendor_bulk_in_received(udd_ep_status_t status,
                                  iram_size_t nb_transfered, udd_ep_id_t ep)
{
    UNUSED(nb_transfered);
    UNUSED(ep);
    FPGA_releaselock();
    if (UDD_EP_TRANSFER_OK != status) {
        return; // Transfer aborted/error
    }

    if (FPGA_lockstatus() == fpga_blockin){
        FPGA_setlock(fpga_unlocked);
    }
}

void main_vendor_bulk_out_received(udd_ep_status_t status,
                                   iram_size_t nb_transfered, udd_ep_id_t ep)
{
    UNUSED(ep);
    FPGA_releaselock();
    if (UDD_EP_TRANSFER_OK != status) {
        // Transfer aborted

        //restart
        // udi_vendor_bulk_out_run(
        //     main_buf_loopback,
        //     sizeof(main_buf_loopback),
        //     main_vendor_bulk_out_received);

        return;
    }

    if (blockendpoint_usage == bep_emem){
        for(unsigned int i = 0; i < nb_transfered; i++){
            xram[i] = main_buf_loopback[i];
        }

        if (FPGA_lockstatus() == fpga_blockout){
            FPGA_releaselock();
        }
    } else if (blockendpoint_usage == bep_fpgabitstream){

        /* Send byte to FPGA - this could eventually be done via SPI */
        // TODO: is this dangerous?
        for(unsigned int i = 0; i < nb_transfered; i++){
            fpga_program_sendbyte(main_buf_loopback[i]);
        }
#if FPGA_USE_BITBANG
        FPGA_CCLK_LOW();
#endif
    } else if (blockendpoint_usage == bep_fpgabitstreamspi) {
        for(unsigned int i = 0; i < nb_transfered; i++){
            fpga_program_spi_sendbyte(main_buf_loopback[i]);
        }
    }

    //printf("BULKOUT: %d bytes\n", (int)nb_transfered);

    udi_vendor_bulk_out_run(
        main_buf_loopback,
        sizeof(main_buf_loopback),
        main_vendor_bulk_out_received);
}

bool openadc_setup_in_received(void)
{
    switch(udd_g_ctrlreq.req.bRequest){
    case REQ_MEMREAD_CTRL:
        udd_g_ctrlreq.payload = ctrlmemread_buf;
        udd_g_ctrlreq.payload_size = ctrlmemread_size;
        ctrlmemread_size = 0;

        FPGA_releaselock();
        return true;
        break;

    case REQ_FPGA_STATUS:
        respbuf[0] = FPGA_ISDONE();
        respbuf[1] = 0;
        respbuf[2] = 0;
        respbuf[3] = 0;
        udd_g_ctrlreq.payload = respbuf;
        udd_g_ctrlreq.payload_size = 4;
        return true;
        break;
    }
    return false;
}
bool openadc_setup_out_received(void)
{
    blockendpoint_usage = bep_emem;
    switch(udd_g_ctrlreq.req.bRequest){
        /* Memory Read */
    case REQ_MEMREAD_BULK:
        if (FPGA_setlock(fpga_usblocked)){
            udd_g_ctrlreq.callback = openadc_readmem_bulk;
            return true;
        }
        break;
    case REQ_MEMREAD_CTRL:
        if (FPGA_setlock(fpga_usblocked)){
            udd_g_ctrlreq.callback = openadc_readmem_ctrl;
            return true;
        }
        break;

        /* Memory Write */
    case REQ_MEMWRITE_BULK:
        if (FPGA_setlock(fpga_usblocked)){
            udd_g_ctrlreq.callback = openadc_writemem_bulk;
            return true;
        }
        break;


    case REQ_MEMWRITE_CTRL:
        if (FPGA_setlock(fpga_usblocked)){
            udd_g_ctrlreq.callback = openadc_writemem_ctrl;
            return true;
        }
        break;

    case REQ_FPGA_PROGRAM:
        udd_g_ctrlreq.callback = openadc_progfpga_bulk;
        return true;

    default:
        return false;
    }

    return false;
}

void openadc_register_handlers(void)
{
    FPGA_releaselock();
    naeusb_add_in_handler(openadc_setup_in_received);
    naeusb_add_out_handler(openadc_setup_out_received);
}

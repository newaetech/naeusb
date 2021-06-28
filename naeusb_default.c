#include "naeusb_default.h"
#include "usart_driver.h"


void naeusb_sam_cfg_out(void)
{
    switch(udd_g_ctrlreq.req.wValue & 0xFF)
    {
        /* Turn on slow clock */
    case SAM_SLOW_CLOCK_ON:
        osc_enable(OSC_MAINCK_XTAL);
        osc_wait_ready(OSC_MAINCK_XTAL);
        pmc_switch_mck_to_mainck(CONFIG_SYSCLK_PRES);
        break;

        /* Turn off slow clock */
    case SAM_SLOW_CLOCK_OFF:
        pmc_switch_mck_to_pllack(CONFIG_SYSCLK_PRES);
        break;

        /* Jump to ROM-resident bootloader */
    case SAM_ENTER_BOOTLOADER:
        /* Turn off connected stuff */
        //board_power(0);

        /* Clear ROM-mapping bit. */
        efc_perform_command(EFC0, EFC_FCMD_CGPB, 1);

        /* Disconnect USB (will kill connection) */
        udc_detach();

        /* With knowledge that I will rise again, I lay down my life. */
        while (RSTC->RSTC_SR & RSTC_SR_SRCMP);
        RSTC->RSTC_CR |= RSTC_CR_KEY(0xA5) | RSTC_CR_PERRST | RSTC_CR_PROCRST;
        while(1);
        break;
        /* Disconnect USB (will kill stuff) */

        /* Make the jump */
        break;
    case SAM_RESET:
        udc_detach();
        while (RSTC->RSTC_SR & RSTC_SR_SRCMP);
        RSTC->RSTC_CR |= RSTC_CR_KEY(0xA5) | RSTC_CR_PERRST | RSTC_CR_PROCRST;
        while(1);
        break;
        
    case SAM_RELEASE_LOCK: // use in case of pipe error emergency
        FPGA_releaselock();
        break;

        /* Oh well, sucks to be you */
    default:
        break;
    }
}

bool naeusb_fw_version_in(void)
{
    respbuf[0] = FW_VER_MAJOR;
    respbuf[1] = FW_VER_MINOR;
    respbuf[2] = FW_VER_DEBUG;
    udd_g_ctrlreq.payload = respbuf;
    udd_g_ctrlreq.payload_size = 3;
    return true;

}

static const char BUILD_DATE[] = __DATE__;
bool naeusb_build_date_in(void)
{
    strncpy(respbuf, BUILD_DATE, 100);
    udd_g_ctrlreq.payload = respbuf;
    udd_g_ctrlreq.payload_size = strlen(respbuf);
}

volatile bool cdc_settings_change[UDI_CDC_PORT_NB] = {1};
volatile bool enable_cdc_transfer[UDI_CDC_PORT_NB] = {0};
extern volatile bool usart_x_enabled[UDI_CDC_PORT_NB];

void naeusb_cdc_settings_out(void)
{
    for (uint8_t i = 0; i < UDI_CDC_PORT_NB; i++) {
        if (udd_g_ctrlreq.req.wValue & (1 << i)) {
            cdc_settings_change[i] = 1;
        } else {
            cdc_settings_change[i] = 0;
        }
    }
}

bool naeusb_cdc_settings_in(void)
{
    for (uint8_t i = 0; i < UDI_CDC_PORT_NB; i++) {
        respbuf[i] = cdc_settings_change[i];
    }
    udd_g_ctrlreq.payload = respbuf;
    udd_g_ctrlreq.payload_size = UDI_CDC_PORT_NB;
    return true;

}

bool cdc_enable(uint8_t port)
{
    enable_cdc_transfer[port] = true;
    return true;
}

void cdc_disable(uint8_t port)
{
    enable_cdc_transfer[port] = false;
}

uint8_t uart_buf[512] = {0};
void my_callback_rx_notify(uint8_t port)
{
    //iram_size_t udi_cdc_multi_get_nb_received_data
    
    if (enable_cdc_transfer[port] && usart_x_enabled[port]) {
        iram_size_t num_char = udi_cdc_multi_get_nb_received_data(port);
        while (num_char > 0) {
            num_char = (num_char > 512) ? 512 : num_char;
            udi_cdc_multi_read_buf(port, uart_buf, num_char);
            for (uint16_t i = 0; i < num_char; i++) {
                usart_driver_putchar(USART_TARGET, NULL, uart_buf[i]);
            }
            num_char = udi_cdc_multi_get_nb_received_data(port);
        }
    }
}

void my_callback_config(uint8_t port, usb_cdc_line_coding_t * cfg)
{
    if (enable_cdc_transfer[port] && cdc_settings_change[port]) {
        usart_x_enabled[port] = true;
        sam_usart_opt_t usartopts;

        if (cfg->bDataBits < 5)
            return;
        if (cfg->bCharFormat > 2)
            return;
    
        usartopts.baudrate = cfg->dwDTERate;
        usartopts.channel_mode = US_MR_CHMODE_NORMAL;
        usartopts.stop_bits = ((uint32_t)cfg->bCharFormat) << 12;
        usartopts.char_length = ((uint32_t)cfg->bDataBits - 5) << 6;
        switch(cfg->bParityType) {
            case CDC_PAR_NONE:
            usartopts.parity_type = US_MR_PAR_NO;
            break;
            case CDC_PAR_ODD:
            usartopts.parity_type = US_MR_PAR_ODD;
            break;
            case CDC_PAR_EVEN:
            usartopts.parity_type = US_MR_PAR_EVEN;
            break;
            case CDC_PAR_MARK:
            usartopts.parity_type = US_MR_PAR_MARK;
            break;
            case CDC_PAR_SPACE:
            usartopts.parity_type = US_MR_PAR_SPACE;
            break;
            default:
            return;
        }
        if (port == 0)
        {
            //completely restart USART - otherwise breaks tx or stalls
            sysclk_enable_peripheral_clock(ID_USART0);
            init_circ_buf(&usb_usart_circ_buf);
            init_circ_buf(&tx0buf);
            init_circ_buf(&rx0buf);
            usart_init_rs232(USART0, &usartopts,  sysclk_get_cpu_hz());
            
            usart_enable_rx(USART0);
            usart_enable_tx(USART0);
            
            usart_enable_interrupt(USART0, UART_IER_RXRDY);
            
            gpio_configure_pin(PIN_USART0_RXD, PIN_USART0_RXD_FLAGS);
            gpio_configure_pin(PIN_USART0_TXD, PIN_USART0_TXD_FLAGS);
            irq_register_handler(USART0_IRQn, 5);
        }
    }
        
}

bool naeusb_setup_out_received(void)
{
    switch (udd_g_ctrlreq.req.bRequest) {
        case REQ_SAM_CFG:
            udd_g_ctrlreq.callback = naeusb_sam_cfg_out;
            return true;
            break;
        case REQ_CDC_SETTINGS_EN:
            udd_g_ctrlreq.callback = naeusb_cdc_settings_out;
            return true;
            break;
    }
    return false;
}

bool naeusb_setup_in_received(void)
{
    switch (udd_g_ctrlreq.req.bRequest) {
        case REQ_FW_VERSION:
            return naeusb_fw_version_in();
            break;
        case REQ_BUILD_DATE:
            return naeusb_build_date_in();
            break;
        case REQ_CDC_SETTINGS_EN:
            return naeusb_cdc_settings_in();
            break;
    }
    return false;
}

void naeusb_register_handlers(void)
{
    naeusb_add_in_handler(naeusb_setup_in_received);
    naeusb_add_out_handler(naeusb_setup_out_received);
}
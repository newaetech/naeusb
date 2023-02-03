#include "naeusb_default.h"
#include "usart_driver.h"
#include <string.h>


#ifndef RSTC_CR_KEY_PASSWD
#define RSTC_CR_KEY_PASSWD RSTC_CR_KEY(0xA5)
#endif

uint8_t LED_SETTING = 0;
uint16_t CURRENT_ERRORS = 0;

bool naeusb_sam_status_in(void)
{
    return false;
}

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
        board_power(0);

        /* Clear ROM-mapping bit. */
        efc_perform_command(EFC0, EFC_FCMD_CGPB, 1);

        /* Disconnect USB (will kill connection) */
        udc_detach();

        /* With knowledge that I will rise again, I lay down my life. */
        while (RSTC->RSTC_SR & RSTC_SR_SRCMP);
        //RSTC->RSTC_CR |= RSTC_CR_KEY(0xA5) | RSTC_CR_PERRST | RSTC_CR_PROCRST;
		RSTC->RSTC_CR |= RSTC_CR_KEY_PASSWD | RSTC_CR_PERRST | RSTC_CR_PROCRST;
        while(1);
        break;
        /* Disconnect USB (will kill stuff) */

        /* Make the jump */
        break;
    case SAM_RESET:
        udc_detach();
        while (RSTC->RSTC_SR & RSTC_SR_SRCMP);
        RSTC->RSTC_CR |= RSTC_CR_KEY_PASSWD | RSTC_CR_PERRST | RSTC_CR_PROCRST;
        while(1);
        break;
        
    case SAM_RELEASE_LOCK: // use in case of pipe error emergency
	#if USB_DEVICE_PRODUCT_ID != 0xACE0
        FPGA_releaselock();
	#endif
        break;

    case SAM_LED_SETTINGS:
        LED_SETTING = (udd_g_ctrlreq.req.wValue >> 8) & 0xFF;
        #ifdef LED1_GPIO
            if (LED_SETTING == 0) {
                LED_Off(LED1_GPIO);
            }
        #endif
        break;

    case SAM_CLEAR_ERRORS:
        CURRENT_ERRORS = 0;
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
static const char BUILD_TIME[] = __TIME__;
bool naeusb_build_date_in(void)
{
    strncpy(respbuf, BUILD_TIME, 64);
    respbuf[sizeof(BUILD_TIME) - 1] = ' ';
    strncpy(respbuf + sizeof(BUILD_TIME), BUILD_DATE, 64 - sizeof(BUILD_TIME));
    udd_g_ctrlreq.payload = respbuf;
    udd_g_ctrlreq.payload_size = strlen(respbuf);
    return true;
}

bool naeusb_status_in(void)
{
    respbuf[0] = CURRENT_ERRORS & 0xFF;
    respbuf[1] = CURRENT_ERRORS >> 8;
    respbuf[2] = LED_SETTING;
    udd_g_ctrlreq.payload = respbuf;
    udd_g_ctrlreq.payload_size = 3;
    return true;
}


bool naeusb_setup_out_received(void)
{
    switch (udd_g_ctrlreq.req.bRequest) {
        case REQ_SAM_CFG:
            udd_g_ctrlreq.callback = naeusb_sam_cfg_out;
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
        case REQ_SAM_STATUS:
            return naeusb_status_in();
            break;
    }
    return false;
}

void naeusb_register_handlers(void)
{
    naeusb_add_in_handler(naeusb_setup_in_received);
    naeusb_add_out_handler(naeusb_setup_out_received);
}
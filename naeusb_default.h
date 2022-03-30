#pragma once
#include <asf.h>
#include "naeusb.h"

#define REQ_SAM_CFG       0x22
#define REQ_SAM_STATUS 0x22

    #define SAM_SLOW_CLOCK_ON   0x01
    #define SAM_SLOW_CLOCK_OFF  0x02
    #define SAM_ENTER_BOOTLOADER 0x03
    #define SAM_RESET           0x10
    #define SAM_RELEASE_LOCK    0x11

    // void naeusb_sam_cfg_out(void);

#define REQ_FW_VERSION      0x17

    // bool naeusb_fw_version_in(void);

#define REQ_BUILD_DATE      0x40

    // bool naeusb_build_date_in(void);


    // void naeusb_cdc_settings_out(void);
    // bool naeusb_cdc_settings_in(void);
void naeusb_register_handlers(void);
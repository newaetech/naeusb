#pragma once
#include <asf.h>
#include "naeusb.h"

// Ctrl Write
#define REQ_SAM_CFG       0x22 // Changes SAM3U config/settings

// Config/Settings (wValue & 0xFF):
    #define SAM_SLOW_CLOCK_ON   0x01 // Slows SAM3U clock for AVR Programming
    #define SAM_SLOW_CLOCK_OFF  0x02 // Puts clock back to normal
    #define SAM_ENTER_BOOTLOADER 0x03 // Enters bootloader mode, requiring new firmware
    #define SAM_RESET           0x10 // Resets SAM3U
    #define SAM_RELEASE_LOCK    0x11 // Releases FPGA lock. Used to fix some pipe errors

    #define SAM_LED_SETTINGS    0x12 // Changes SAM3U Error LED settings
    #define SAM_CLEAR_ERRORS    0x13 // Clears SAM3U Errors (Just buffer overrun)
        #define CW_LED_DEFAULT_SETTING 0x00 // Default = Unused
        #define CW_LED_DEBUG_SETTING 0x01 // Blink LEDs
        #define CW_LED_ERR_SETTING 0x02 // Blink upon error


// Ctrl Read
#define REQ_SAM_STATUS 0x22 // Reads SAM3U config/settings
#define REQ_FW_VERSION      0x17 // Reads sam3u firmware version
#define REQ_BUILD_DATE      0x40 // Reads sam3u firmware build time/date

extern uint8_t LED_SETTING;
extern uint16_t CURRENT_ERRORS;

#define CW_ERR_USART_RX_OVERFLOW (1 << 0)
#define CW_ERR_USART_TX_OVERFLOW (1 << 1)

void naeusb_register_handlers(void); // call to register common USB handler functions (should be called in main)
#pragma once
/**COMMANDS**/
// Standard commands
// Write flash

// Enter programming mode
#define NUVO_CMD_CONNECT 0xae
// Get Device ID
#define NUVO_CMD_GET_DEVICEID 0xb1
// Write flash
#define NUVO_CMD_PAGE_ERASE 0xD5
// Exit programming mode
#define NUVO_CMD_RESET 0xad

// ICP Specific
// Read flash
#define NUVO_CMD_READ_FLASH 0xa5
// Get Unique ID
#define NUVO_CMD_GET_UID 0xb2
// Get Company ID
#define NUVO_CMD_GET_CID 0xb3
// Get Unique Company ID
#define NUVO_CMD_GET_UCID 0xb4
// Mass erase
#define NUVO_CMD_MASS_ERASE 0xD6
// Puts the target into ICP mode, but doesn't initialize the PGM.
#define NUVO_CMD_ENTER_ICP_MODE 0xe7
// Takes the target out of ICP mode, but doesn't deinitialize the PGM.
#define NUVO_CMD_EXIT_ICP_MODE 0xe8
// Takes chip out of and then puts it back into ICP mode
#define NUVO_CMD_REENTER_ICP 0xe9
// For getting the configuration bytes to read at consistent times during an ICP reentry.
#define NUVO_CMD_REENTRY_GLITCH 0xea
// Write to flash
#define NUVO_CMD_WRITE_FLASH 0xed
// Set the programming time between bytes
#define NUVO_SET_PROG_TIME 0xec
// Set the erase time for a page
#define NUVO_SET_PAGE_ERASE_TIME 0xee
// Set the mass erase time.
#define NUVO_SET_MASS_ERASE_TIME 0xef
// Set the post mass erase time.
#define NUVO_SET_POST_MASS_ERASE_TIME 0xe3

// ChipWhisperer specific
#define NUVO_GET_RAMBUF 0xe4
#define NUVO_SET_RAMBUF 0xe5
#define NUVO_GET_STATUS 0xe6

#define NUVO_ERR_OK 0
#define NUVO_ERR_FAILED 1
#define NUVO_ERR_INCORRECT_PARAMS 2
#define NUVO_ERR_COLLISION 3
#define NUVO_ERR_TIMEOUT 4
#define NUVO_ERR_WRITE_FAILED 5
#define NUVO_ERR_READ_FAILED 6

// page size
#define NU51_PAGE_SIZE 128
#define NU51_PAGE_MASK 0xFF80


// // ** ISP commands, Not used by ICP **
// #define NUVO_CMD_GET_BANDGAP 0xb5
// #define NUVO_CMD_SYNC_PACKNO 0xa4
// #define NUVO_CMD_UPDATE_APROM 0xa0
// #define NUVO_CMD_RUN_APROM 0xab
// #define NUVO_CMD_RUN_LDROM 0xac    
// #define NUVO_CMD_RESEND_PACKET 0xFF
// #define NUVO_CMD_GET_FWVER 0xa6
// #define NUVO_CMD_ERASE_ALL 0xa3 // This erases just the APROM instead of doing a mass erase
// #define NUVO_CMD_GET_FLASHMODE 0xCA
// #define NUVO_CMD_UPDATE_CONFIG 0xa1
// #define NUVO_CMD_READ_CONFIG 0xa2

// // ** Unsupported by N76E003 **
// // Dataflash commands (when a chip has the ability to deliniate between data and program flash)
// #define NUVO_CMD_UPDATE_DATAFLASH 0xC3
// // SPI flash commands
// #define NUVO_CMD_ERASE_SPIFLASH 0xD0
// #define NUVO_CMD_UPDATE_SPIFLASH 0xD1
// // CAN commands
// #define NUVO_CAN_CMD_READ_CONFIG 0xA2000000
// #define NUVO_CAN_CMD_RUN_APROM 0xAB000000
// #define NUVO_CAN_CMD_GET_DEVICEID 0xB1000000

// // Deprecated, no ISP programmer uses these
// #define NUVO_CMD_READ_CHECKSUM 0xC8
// #define NUVO_CMD_WRITE_CHECKSUM 0xC9
// #define NUVO_CMD_SET_INTERFACE 0xBA
// // The modes returned by CMD_GET_FLASHMODE
// #define NUVO_APMODE 1
// #define NUVO_LDMODE 2

// // ISP packet constants
// #define NUVO_PKT_CMD_START 0
// #define NUVO_PKT_CMD_SIZE 4
// #define NUVO_PKT_SEQ_START 4
// #define NUVO_PKT_SEQ_SIZE 4
// #define NUVO_PKT_HEADER_END 8

// #define NUVO_PACKSIZE 64

// #define NUVO_INITIAL_UPDATE_PKT_START 16 // PKT_HEADER_END + 8 bytes for addr and len
// #define NUVO_INITIAL_UPDATE_PKT_SIZE 48

// #define NUVO_SEQ_UPDATE_PKT_START PKT_HEADER_END
// #define NUVO_SEQ_UPDATE_PKT_SIZE 56

// #define NUVO_DUMP_PKT_CHECKSUM_START PKT_HEADER_END
// #define NUVO_DUMP_PKT_CHECKSUM_SIZE 0       // disabled for now
// #define NUVO_DUMP_DATA_START PKT_HEADER_END //(DUMP_PKT_CHECKSUM_START + DUMP_PKT_CHECKSUM_SIZE)
// #define NUVO_DUMP_DATA_SIZE 56              //(PACKSIZE - DUMP_DATA_START)

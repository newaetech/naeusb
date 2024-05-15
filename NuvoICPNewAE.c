#define INCLUDE_FROM_NUVOICP_C 1
#include "NuvoICPNewAE.h"
#undef INCLUDE_FROM_NUVOICP_C
#include "NuvoProgCommon.h"
#include "n51_icp.h"
#include <stdint.h>
#include <string.h>

#define NUVOICP_BUF_SIZE 256
#define STATUS_DATA_START 3
// Request payload params start at this index
#define PDATA_START 0

/* Status of last executed command */
uint8_t N51_Status;

uint32_t unpack_u32(uint8_t *buf, uint32_t offset)
{
  return (buf[offset + 3] << 24) | (buf[offset + 2] << 16) | (buf[offset + 1] << 8) | buf[offset];
}

bool _check_param_len(uint8_t len)
{
  if (udd_g_ctrlreq.req.wLength < len + PDATA_START)
  {
    N51_Status = NUVO_ERR_INCORRECT_PARAMS;
    return false;
  }
  N51_Status = NUVO_ERR_OK;
  return true;
}


bool NuvoICP_Protocol_Command(void)
{
  static uint8_t status_payload[32];
  status_payload[0] = udd_g_ctrlreq.req.wValue & 0xff;
  static uint16_t nuvoicp_status_payload_size = 0;

  static uint8_t NuvoICP_rambuf[NUVOICP_BUF_SIZE];
  uint8_t offset;

  switch (status_payload[0])
  {
  case NUVO_CMD_WRITE_FLASH:
    NuvoICP_WriteMemory(NuvoICP_rambuf, &status_payload[STATUS_DATA_START]);
    nuvoicp_status_payload_size = 2;
    break;

  case NUVO_CMD_PAGE_ERASE:
    NuvoICP_Page_Erase(NuvoICP_rambuf);
    break;

  case NUVO_CMD_MASS_ERASE:
    NuvoICP_Mass_Erase();
    break;

  case NUVO_CMD_READ_FLASH:
    NuvoICP_ReadMemory(NuvoICP_rambuf);
    break;

  case NUVO_CMD_RESET:
    NuvoICP_LeaveProgMode();
    break;

  case NUVO_CMD_CONNECT:
    NuvoICP_EnterProgMode();
    break;

  case NUVO_CMD_GET_DEVICEID:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_GET_DEVICEID, &status_payload[STATUS_DATA_START]);
    break;

  case NUVO_CMD_GET_PID:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_GET_PID, &status_payload[STATUS_DATA_START]);
    break;

  case NUVO_CMD_GET_UID:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_GET_UID,  &status_payload[STATUS_DATA_START]);
    break;

  case NUVO_CMD_GET_CID:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_GET_CID, &status_payload[STATUS_DATA_START]);
    break;

  case NUVO_CMD_GET_UCID:
    nuvoicp_status_payload_size = NuvoICP_GetParam(NUVO_CMD_GET_UCID, &status_payload[STATUS_DATA_START]);
    break;
  case NUVO_GET_RAMBUF:
    offset = (udd_g_ctrlreq.req.wValue >> 8) & 0xff;
    if ((offset + udd_g_ctrlreq.req.wLength) > NUVOICP_BUF_SIZE)
    {
      // nice try!
      return false;
    }

    udd_g_ctrlreq.payload = NuvoICP_rambuf + offset;
    udd_g_ctrlreq.payload_size = udd_g_ctrlreq.req.wLength;
    return true;
    break;

  // Write data to intername RAM buffer
  case NUVO_SET_RAMBUF:
    N51_Status = NUVO_ERR_OK;
    offset = (udd_g_ctrlreq.req.wValue >> 8) & 0xff;
    if ((offset + udd_g_ctrlreq.req.wLength) > NUVOICP_BUF_SIZE)
    {
      // nice try!
      N51_Status = NUVO_ERR_INCORRECT_PARAMS;
      return false;
    }

    memcpy(NuvoICP_rambuf + offset, udd_g_ctrlreq.payload,
           udd_g_ctrlreq.req.wLength);
    return true;
    break;

  case NUVO_GET_STATUS:
    status_payload[1] = N51_Status;
    status_payload[2] = 0;
    udd_g_ctrlreq.payload = status_payload;
    udd_g_ctrlreq.payload_size = nuvoicp_status_payload_size + STATUS_DATA_START;
    nuvoicp_status_payload_size = 0;
    return true;
    break;

  case NUVO_CMD_ENTER_ICP_MODE:
  {
    if (!_check_param_len(1)) {
      return false;
    }
    uint32_t ret = N51ICP_enter_icp_mode(udd_g_ctrlreq.payload[PDATA_START]);
    nuvoicp_status_payload_size = 4;
    status_payload[STATUS_DATA_START] = ret & 0xff;
    status_payload[STATUS_DATA_START + 1] = (ret >> 8) & 0xff;
    status_payload[STATUS_DATA_START + 2] = (ret >> 16) & 0xff;
    status_payload[STATUS_DATA_START + 3] = (ret >> 24) & 0xff;
  } break;

  case NUVO_CMD_EXIT_ICP_MODE:
    N51_Status = NUVO_ERR_OK;
    N51ICP_exit_icp_mode();
    break;
  
  case NUVO_CMD_REENTER_ICP:
    NuvoICP_Reentry();
    break;

  case NUVO_CMD_REENTRY_GLITCH:
    N51_Status = NUVO_ERR_OK;
    NuvoICP_Reentry_glitch();
    break;

  case NUVO_SET_PROG_TIME:
    if (!_check_param_len(8)){
      return false;
    }
    N51ICP_set_program_time(unpack_u32(udd_g_ctrlreq.payload, PDATA_START), unpack_u32(udd_g_ctrlreq.payload, PDATA_START+4));
    break;
  
  case NUVO_SET_PAGE_ERASE_TIME:
    if (!_check_param_len(8)){
      return false;
    }
    N51ICP_set_page_erase_time(unpack_u32(udd_g_ctrlreq.payload, PDATA_START), unpack_u32(udd_g_ctrlreq.payload, PDATA_START+4));
    break;

  case NUVO_SET_MASS_ERASE_TIME:
    if (!_check_param_len(8)){
      return false;
    }
    N51ICP_set_mass_erase_time(unpack_u32(udd_g_ctrlreq.payload, PDATA_START), unpack_u32(udd_g_ctrlreq.payload, PDATA_START+4));
    break;

  default:
    break;
  }

  return false;
}

void NuvoICP_Reentry(void)
{
  if (!_check_param_len(12)){
    return;
  }
  uint32_t delay1 = unpack_u32(udd_g_ctrlreq.payload, PDATA_START);
  uint32_t delay2 = unpack_u32(udd_g_ctrlreq.payload, PDATA_START + 4);
  uint32_t delay3 = unpack_u32(udd_g_ctrlreq.payload, PDATA_START + 8);
  N51ICP_reentry(delay1, delay2, delay3);
}

void NuvoICP_Reentry_glitch(void)
{
  if (!_check_param_len(16)){
    return;
  }
  uint32_t delay1 = unpack_u32(udd_g_ctrlreq.payload, PDATA_START);
  uint32_t delay2 = unpack_u32(udd_g_ctrlreq.payload, PDATA_START + 4);
  uint32_t delay3 = unpack_u32(udd_g_ctrlreq.payload, PDATA_START + 8);
  uint32_t delay4 = unpack_u32(udd_g_ctrlreq.payload, PDATA_START + 12);

  N51ICP_reentry_glitch(delay1, delay2, delay3, delay4);
}

void NuvoICP_EnterProgMode(void)
{
  uint8_t res = N51ICP_init();
  if (res < 0) // negative error codes
  {
    N51_Status = res * -1;
  }
  else if (res == 0)
  {
    N51_Status = NUVO_ERR_OK;
  } else {
    N51_Status = res;
  }
}

void NuvoICP_LeaveProgMode(void)
{
  uint8_t leave_reset_high = 0; // default to high_z
  // parameter is optional
  if (udd_g_ctrlreq.req.wLength >= 1 + PDATA_START) {
    leave_reset_high = udd_g_ctrlreq.payload[PDATA_START];
  }
  N51ICP_deinit(leave_reset_high);
  N51_Status = NUVO_ERR_OK;
}

void NuvoICP_Mass_Erase(void)
{
  N51ICP_mass_erase();
  N51_Status = NUVO_ERR_OK;
}

// expects an 32-bit address and a 16-bit length
bool NuvoICP_WriteMemory(uint8_t *buf, uint8_t *status_buf_data)
{
  status_buf_data[0] = 0;
  status_buf_data[1] = 0;
  if (!_check_param_len(6)){
    return false;
  }

  uint32_t Address = (udd_g_ctrlreq.payload[PDATA_START+3] << 24) |
                      (udd_g_ctrlreq.payload[PDATA_START+2] << 16) |
                      (udd_g_ctrlreq.payload[PDATA_START+1] << 8) |
                      (udd_g_ctrlreq.payload[PDATA_START]);
  uint16_t Length = udd_g_ctrlreq.payload[PDATA_START+4] |
                    (udd_g_ctrlreq.payload[PDATA_START+5] << 8);

  if (Length > NUVOICP_BUF_SIZE)
  {
    Length = NUVOICP_BUF_SIZE;
  }
  uint16_t checksum = 0;
  uint32_t addr_written = N51ICP_write_flash(Address, Length, buf);
  for (uint16_t i = 0; i < Length; i++)
  {
    checksum += buf[i];
  }
  // pack them into the status_buf_data
  status_buf_data[0] = checksum & 0xff;
  status_buf_data[1] = (checksum >> 8) & 0xff;
  if (addr_written != Address + Length){
    N51_Status = NUVO_ERR_WRITE_FAILED;
    return false;
  }

  N51_Status = NUVO_ERR_OK;
  return true;
}

// expects a 32-bit address
void NuvoICP_Page_Erase(uint8_t * NuvoICP_rambuf){
  if (!_check_param_len(4)){
    return;
  }
  uint32_t Address = (udd_g_ctrlreq.payload[PDATA_START+3] << 24) | 
                      (udd_g_ctrlreq.payload[PDATA_START+2] << 16) | 
                      (udd_g_ctrlreq.payload[PDATA_START+1] << 8) | 
                      (udd_g_ctrlreq.payload[PDATA_START]);
  N51ICP_page_erase(Address);
}

/**
 * @brief Get the value of a parameter
 * 
 * @param cmd The command to get the value of
 * @param buf The buffer to store the value in
 * @returns The length of the value
*/
uint16_t NuvoICP_GetParam(uint8_t cmd, uint8_t *buf)
{
  N51_Status = NUVO_ERR_OK;
  uint32_t value = 0;
  bool in_val = false;
  uint16_t val_length = 0;
  switch (cmd)
  {
    case NUVO_CMD_GET_DEVICEID:
      value = N51ICP_read_device_id();
      in_val = true;
      break;
    case NUVO_CMD_GET_UID:
      N51ICP_read_uid(buf);
      val_length = 12;
      break;
    case NUVO_CMD_GET_CID:
      value = N51ICP_read_cid();
      in_val = true;
      break;
    case NUVO_CMD_GET_UCID:
      N51ICP_read_ucid(buf);
      val_length = 16;
      break;
    case NUVO_CMD_GET_PID:
      value = N51ICP_read_pid();
      in_val = true;
      break;
  }
  if (in_val)
  {
    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
    val_length = 4;
  }

  N51_Status = NUVO_ERR_OK;
  return val_length;
}

void NuvoICP_ReadMemory(uint8_t *buf)
{
  if (!_check_param_len(6)){
    return;
  }
  uint32_t Address = (udd_g_ctrlreq.payload[PDATA_START+3] << 24) |
                      (udd_g_ctrlreq.payload[PDATA_START+2] << 16) |
                      (udd_g_ctrlreq.payload[PDATA_START+1] << 8) |
                      (udd_g_ctrlreq.payload[PDATA_START]);
  uint16_t Length = udd_g_ctrlreq.payload[PDATA_START+4] |
                    (udd_g_ctrlreq.payload[PDATA_START+5] << 8);
  if (Length > NUVOICP_BUF_SIZE)
  {
    Length = NUVOICP_BUF_SIZE;
  }

  uint32_t addr_read = N51ICP_read_flash(Address, Length, buf);
  if (addr_read != Address + Length){
    N51_Status = NUVO_ERR_READ_FAILED;
    return;
  }
  N51_Status = NUVO_ERR_OK;
}
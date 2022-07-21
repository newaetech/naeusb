/*
 Copyright (c) 2015 NewAE Technology Inc. All rights reserved.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asf.h>
#include "usb_xmem.h"

/* Access pointer for FPGA Interface */
uint8_t volatile *xram = (uint8_t *) PSRAM_BASE_ADDRESS;

static volatile fpga_lockstatus_t _fpga_locked = fpga_unlocked;

int FPGA_setlock(fpga_lockstatus_t lockstatus)
{
  int ret = 0;
  cpu_irq_enter_critical();
  if (_fpga_locked == fpga_unlocked)
  {
    ret = 1;
    _fpga_locked = lockstatus;
  }
  cpu_irq_leave_critical();
  return ret;
}

void FPGA_releaselock(void)
{
  _fpga_locked = fpga_unlocked;
}

fpga_lockstatus_t FPGA_lockstatus(void)
{
  return _fpga_locked;
}

int try_enter_cs(void)
{
  // Try to get the lock
  cpu_irq_enter_critical();
  if(FPGA_setlock(fpga_generic))
    return 1;

  // If we didn't get it, revert back
  cpu_irq_leave_critical();
  return 0;
}

void exit_cs(void)
{
  FPGA_releaselock();
  cpu_irq_leave_critical();
}
#ifndef PIN_EBI_USB_SPARE1
#define PIN_EBI_USB_SPARE1 FPGA_ALE_GPIO
#endif

#ifdef FPGA_ADDR_PORT
void FPGA_setaddr(uint32_t addr)
{
	#if (USB_DEVICE_PRODUCT_ID == 0xACE5) || (USB_DEVICE_PRODUCT_ID == 0xC610)
	//husky
	  FPGA_ADDR_PORT->PIO_ODSR = (FPGA_ADDR_PORT->PIO_ODSR & 0x40) | (addr & 0x3F) | ((addr & 0xC0) << 1);
	#else
			pio_sync_output_write(FPGA_ADDR_PORT, addr);
	#endif

			gpio_set_pin_low(FPGA_ALE_GPIO);
			gpio_set_pin_high(FPGA_ALE_GPIO);
}
#else
void FPGA_setaddr(uint32_t addr)
{}
#endif

/*
Read four bytes from a given register, return as 32-bit number.

"Unsafe" as doesn't check/modify locking status.
*/
uint32_t unsafe_readuint32(uint16_t fpgaaddr)
{
  uint32_t data;

  FPGA_setaddr(fpgaaddr);
  data = *xram;
  data |= *(xram+1) << 8;
  data |= *(xram+2) << 16;
  data |= *(xram+3) << 24;
  return data;
}

uint32_t safe_readuint32(uint16_t fpgaaddr)
{
  //TODO - This timeout to make GUI responsive in case of USB errors, but data will be invalid
  uint32_t timeout = 10000;
  do{
    timeout--;
    if(timeout == 0){return 0xffffffff;};
  }while(!try_enter_cs());
  uint32_t data;

  FPGA_setaddr(fpgaaddr);
  data = *xram;
  data |= *(xram+1) << 8;
  data |= *(xram+2) << 16;
  data |= *(xram+3) << 24;
  exit_cs();
  return data;
}



// Read numBytes bytes from memory
void unsafe_readbytes(uint16_t fpgaaddr, uint8_t* data, int numBytes)
{
  FPGA_setaddr(fpgaaddr);

  for(int i = 0; i < numBytes; i++)
  {
    data[i] = *(xram+i);
  }
}

// Safely read bytes from memory by disabling interrupts first
// Blocks until able to read
void safe_readbytes(uint16_t fpgaaddr, uint8_t* data, int numBytes)
{
  //TODO - This timeout to make GUI responsive in case of USB errors, but data will be invalid
  uint32_t timeout = 10000;
  do{
    timeout--;
    if(timeout == 0){*data = 0xFF; return;};
  }while(!try_enter_cs());

  FPGA_setaddr(fpgaaddr);

  for(int i = 0; i < numBytes; i++)
  {
    data[i] = *(xram+i);
  }
  exit_cs();
}

// Write 4 bytes to memory
void unsafe_writebytes(uint16_t fpgaaddr, uint8_t* data, int numBytes)
{
  FPGA_setaddr(fpgaaddr);

  for(int i = 0; i < numBytes; i++)
  {
    *(xram+i) = data[i];
  }
}

//Set timing for normal mode
void smc_normaltiming(void){
  smc_set_setup_timing(SMC, 0, SMC_SETUP_NWE_SETUP(0)
                       | SMC_SETUP_NCS_WR_SETUP(1)
                       | SMC_SETUP_NRD_SETUP(1)
                       | SMC_SETUP_NCS_RD_SETUP(1));
  smc_set_pulse_timing(SMC, 0, SMC_PULSE_NWE_PULSE(1)
                       | SMC_PULSE_NCS_WR_PULSE(1)
                       | SMC_PULSE_NRD_PULSE(3)
                       | SMC_PULSE_NCS_RD_PULSE(1));
  smc_set_cycle_timing(SMC, 0, SMC_CYCLE_NWE_CYCLE(2)
                       | SMC_CYCLE_NRD_CYCLE(4));
  smc_set_mode(SMC, 0, SMC_MODE_READ_MODE | SMC_MODE_WRITE_MODE
               | SMC_MODE_DBW_BIT_8);
}

void smc_fasttiming(void){
  // fast reads, for streaming only: 32 bytes in 12 cycles
  smc_set_setup_timing(SMC, 0, SMC_SETUP_NWE_SETUP(0)
                       | SMC_SETUP_NCS_WR_SETUP(1)
                       | SMC_SETUP_NRD_SETUP(1)
                       | SMC_SETUP_NCS_RD_SETUP(1));
  smc_set_pulse_timing(SMC, 0, SMC_PULSE_NWE_PULSE(1)
                       | SMC_PULSE_NCS_WR_PULSE(1)
                       | SMC_PULSE_NRD_PULSE(1)
                       | SMC_PULSE_NCS_RD_PULSE(1));
  smc_set_cycle_timing(SMC, 0, SMC_CYCLE_NWE_CYCLE(2)
                       | SMC_CYCLE_NRD_CYCLE(2));
  smc_set_mode(SMC, 0, SMC_MODE_READ_MODE | SMC_MODE_WRITE_MODE
               | SMC_MODE_DBW_BIT_8);


}

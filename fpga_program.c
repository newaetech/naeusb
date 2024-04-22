/*
  Xilinx Spartan 6 FPGA Programming Routines

  Copyright (c) 2014-2015 NewAE Technology Inc.  All rights reserved.
  Author: Colin O'Flynn, <coflynn@newae.com>

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
#include "fpga_program.h"
#include "spi.h"
#include "pdc.h"
#include "usb_xmem.h"
#include "usb.h"

extern blockep_usage_t blockendpoint_usage;
#define PROG_BUF_SIZE 1024

COMPILER_WORD_ALIGNED volatile uint8_t fpga_prog_buf[PROG_BUF_SIZE];
void fpga_prog_bulk_out_received(udd_ep_status_t status,
                                   iram_size_t nb_transfered, udd_ep_id_t ep)
{
    if (UDD_EP_TRANSFER_OK != status) {
        // Transfer aborted
        return;
    }

    for(unsigned int i = 0; i < nb_transfered; i++){
        fpga_program_sendbyte(fpga_prog_buf[i]);
    }
	udi_vendor_bulk_out_run(
			fpga_prog_buf,
			PROG_BUF_SIZE,
			fpga_prog_bulk_out_received);
}

void fpga_program_spi_setup1(uint32_t prog_freq)
{
	#if AVRISP_USEUART
	usart_spi_opt_t spiopts;
	spiopts.baudrate = prog_freq;
	spiopts.char_length = US_MR_CHRL_8_BIT;
	spiopts.channel_mode = US_MR_CHMODE_NORMAL;
	spiopts.spi_mode = SPI_MODE_0;
	
	sysclk_enable_peripheral_clock(AVRISP_USART_ID);
	usart_init_spi_master(AVRISP_USART, &spiopts, sysclk_get_cpu_hz());
	gpio_configure_pin(AVRISP_MISO_GPIO, AVRISP_MISO_FLAGS);
	gpio_configure_pin(AVRISP_MOSI_GPIO, AVRISP_MOSI_FLAGS);
	gpio_configure_pin(AVRISP_SCK_GPIO, AVRISP_SCK_FLAGS);
	usart_enable_tx(AVRISP_USART);
	usart_enable_rx(AVRISP_USART);
	#else
	spi_enable_clock(SPI);
	spi_reset(SPI);
	spi_set_master_mode(SPI);
	spi_disable_mode_fault_detect(SPI);
	spi_disable_loopback(SPI);

	spi_set_clock_polarity(SPI, 0, 0);
	spi_set_clock_phase(SPI, 0, 1);
	spi_set_baudrate_div(SPI, 0, spi_calc_baudrate_div(prog_freq, sysclk_get_cpu_hz()));

	spi_enable(SPI);

	gpio_configure_pin(SPI_MOSI_GPIO, SPI_MOSI_FLAGS);
	gpio_configure_pin(SPI_SPCK_GPIO, SPI_SPCK_FLAGS);
	#endif

}

void fpga_program_spi_sendbyte(uint8_t databyte)
{
	#if AVRISP_USEUART
	usart_putchar(AVRISP_USART, databyte);
	#else
	spi_write(SPI, databyte, 0, 0);
	#endif
}

/* FPGA Programming: Init pins, set to standby state */
void fpga_program_init(void)
{
    FPGA_NPROG_SETUP();
    FPGA_NPROG_HIGH();
}

/* FPGA Programming Step 1: Erase FPGA, setup SPI interface */
void fpga_program_setup1(uint32_t prog_freq)
{
	/* Init - set program low to erase FPGA */
	FPGA_NPROG_LOW();

	#if FPGA_USE_BITBANG
	FPGA_CCLK_SETUP();
	FPGA_DO_SETUP();

	#elif FPGA_USE_USART
	usart_spi_opt_t spiopts;
	spiopts.baudrate = prog_freq;
	spiopts.char_length = US_MR_CHRL_8_BIT;
	spiopts.channel_mode = US_MR_CHMODE_NORMAL;
	spiopts.spi_mode = SPI_MODE_0;
	sysclk_enable_peripheral_clock(FPGA_PROG_USART_ID);
	usart_init_spi_master(FPGA_PROG_USART, &spiopts, sysclk_get_cpu_hz());

    FPGA_DO_SETUP();
    FPGA_CCLK_SETUP();
	
	usart_enable_tx(FPGA_PROG_USART);
	#else

	spi_enable_clock(SPI);
	spi_reset(SPI);
	spi_set_master_mode(SPI);
	spi_disable_mode_fault_detect(SPI);
	spi_disable_loopback(SPI);

	spi_set_clock_polarity(SPI, 0, 0);
	spi_set_clock_phase(SPI, 0, 1);
	spi_set_baudrate_div(SPI, 0, spi_calc_baudrate_div(prog_freq, sysclk_get_cpu_hz()));

	spi_enable(SPI);

	gpio_configure_pin(SPI_MOSI_GPIO, SPI_MOSI_FLAGS);
	gpio_configure_pin(SPI_SPCK_GPIO, SPI_SPCK_FLAGS);
	#endif
}

/* FPGA Programming Step 2: Prepare FPGA for receiving programming data */
void fpga_program_setup2(void)
{
    FPGA_NPROG_HIGH();
	udd_ep_abort(UDI_VENDOR_EP_BULK_OUT);

	// udi_vendor_bulk_out_run(
	// 		main_buf_loopback,
	// 		sizeof(main_buf_loopback),
	// 		main_vendor_bulk_out_received);
	udi_vendor_bulk_out_run(
			fpga_prog_buf,
			PROG_BUF_SIZE,
			fpga_prog_bulk_out_received);
	// blockendpoint_usage = bep_fpgabitstream;
}

//For debug only
//uint32_t fpga_total_bs_len;

/* FPGA Programming Step 3: Send data until done */
void fpga_program_sendbyte(uint8_t databyte)
{
	//For debug only
    //fpga_total_bs_len++;
		
	#if FPGA_USE_BITBANG
	for(unsigned int i=0; i < 8; i++){
		FPGA_CCLK_LOW();
		
		if (databyte & 0x01){
			FPGA_DO_HIGH();
			} else {
			FPGA_DO_LOW();
		}
		
		FPGA_CCLK_HIGH();
		databyte = databyte >> 1;
	}
	#elif FPGA_USE_USART
	usart_putchar(FPGA_PROG_USART, databyte);
	#else
	spi_write(SPI, databyte, 0, 0);
	#endif
}

void main_vendor_bulk_out_received(udd_ep_status_t status,
                                   iram_size_t nb_transfered, udd_ep_id_t ep);

extern uint8_t main_buf_loopback[MAIN_LOOPBACK_SIZE];
void fpga_program_finish(void)
{
	// blockendpoint_usage = bep_emem;
	udd_ep_abort(UDI_VENDOR_EP_BULK_OUT);
	udi_vendor_bulk_out_run(
			main_buf_loopback,
			sizeof(main_buf_loopback),
			main_vendor_bulk_out_received);
}
#include <asf.h>
#include "fpga_selectmap.h"
#include "fpga_program.h"
#include "naeusb_default.h"
#define PULSE_TIME SETUP_TIME*2
#define CYCLE_TIME (4*SETUP_TIME)

void setup_fpga_rw(void);

void fpga_selectmap_setup1(uint8_t bytemode, uint16_t SETUP_TIME)
{
    //make sure usart pins are high-z
	FPGA_NPROG_LOW();
    gpio_configure_pin(PIN_FPGA_CCLK_GPIO, PIO_INPUT);
    gpio_configure_pin(PIN_FPGA_DO_GPIO, PIO_INPUT);

    gpio_configure_pin(PIN_EBI_DATA_BUS_D0, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D1, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D2, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D3, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D4, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D5, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D6, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D7, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D8, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D9, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D10, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D11, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D12, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D13, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D14, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D15, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_NRD, PIO_INPUT);
    gpio_configure_pin(PIN_EBI_NWE, PIN_EBI_NWE_FLAGS);
    gpio_configure_pin(PIN_EBI_NCS0, PIN_EBI_NCS0_FLAGS);

    #ifdef PIN_CFG_RDRW
    gpio_configure_pin(PIN_CFG_RDRW, PIN_CFG_RDRW_PIO_PC1_FLAGS);
    #endif

	pmc_enable_periph_clk(ID_SMC);
	smc_set_setup_timing(SMC, 0, SMC_SETUP_NWE_SETUP(SETUP_TIME)
	| SMC_SETUP_NCS_WR_SETUP(2)
	| SMC_SETUP_NRD_SETUP(2)
	| SMC_SETUP_NCS_RD_SETUP(3));
	smc_set_pulse_timing(SMC, 0, SMC_PULSE_NWE_PULSE(PULSE_TIME)
	| SMC_PULSE_NCS_WR_PULSE(6)
	| SMC_PULSE_NRD_PULSE(6)
	| SMC_PULSE_NCS_RD_PULSE(6));
	smc_set_cycle_timing(SMC, 0, SMC_CYCLE_NWE_CYCLE(CYCLE_TIME)
	| SMC_CYCLE_NRD_CYCLE(12));

    if (bytemode == 0) {
        smc_set_mode(SMC, 0, SMC_MODE_DBW_BIT_8 | SMC_MODE_WRITE_MODE_NWE_CTRL);
    } else {
        smc_set_mode(SMC, 0, SMC_MODE_DBW_BIT_16 | SMC_MODE_WRITE_MODE_NWE_CTRL);
    }
}

/* FPGA Programming Step 2: Prepare FPGA for receiving programming data */
void fpga_selectmap_setup2(void)
{
    FPGA_NPROG_HIGH();
    gpio_configure_pin(PIN_FPGA_CCLK_GPIO, PIO_INPUT);
    gpio_configure_pin(PIN_FPGA_DO_GPIO, PIO_INPUT);
}

void fpga_selectmap_setup3(void)
{
    gpio_configure_pin(PIN_EBI_DATA_BUS_D0, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D1, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D2, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D3, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D4, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D5, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D6, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D7, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D8, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D9, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D10, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D11, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D12, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D13, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D14, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_DATA_BUS_D15, PIN_EBI_DATA_BUS_FLAG1);
    gpio_configure_pin(PIN_EBI_NRD, PIN_EBI_NRD_FLAGS);
    gpio_configure_pin(PIN_EBI_NWE, PIN_EBI_NWE_FLAGS);
    gpio_configure_pin(PIN_EBI_NCS0, PIN_EBI_NCS0_FLAGS);

    setup_fpga_rw(); // needs to be defined elsewhere. For Luna, this is in naeusb_luna.c
}
// done by DMA
// void fpga_selectmap_sendbyte(uint8_t databyte)
// {

// }
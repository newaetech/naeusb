#include "fpga_selectmap.h"
#include "naeusb_default.h"

void fpga_selectmap_setup1(uint32_t prog_freq)
{

}

/* FPGA Programming Step 2: Prepare FPGA for receiving programming data */
void fpga_selectmap_setup2(void)
{
    FPGA_NPROG_HIGH();
}

void fpga_selectmap_sendbyte(uint8_t databyte)
{

}
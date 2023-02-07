#ifndef _FPGA_SELECTMAP_H
#define _FPGA_SELECTMAP_H

void fpga_selectmap_setup1(uint32_t prog_freq);
void fpga_selectmap_setup2(void);
void fpga_selectmap_sendbyte(uint8_t databyte);

#endif
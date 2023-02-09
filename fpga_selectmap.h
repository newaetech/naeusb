#ifndef _FPGA_SELECTMAP_H
#define _FPGA_SELECTMAP_H

void fpga_selectmap_setup1(uint8_t bytemode, uint16_t SETUP_TIME);
void fpga_selectmap_setup2(void);
// void fpga_selectmap_sendbyte(uint8_t databyte);

// void fpga_selectmap_bulk_setup(void);
// void fpga_selectmap_bulk_callback(void);
void fpga_selectmap_setup3(void);

#endif
/*
 Copyright (c) 2015-2016 NewAE Technology Inc. All rights reserved.

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
#include "tps56520.h"
#include "i2c_util.h"

#define TPS56520_ADDR 0x34

uint8_t TPS_CONNECTED = 0;

unsigned char checkoddparity(unsigned char p);

/* Is current byte odd-parity already? */
unsigned char checkoddparity(unsigned char p)
{
	p = p ^ (p >> 4 | p << 4);
	p = p ^ (p >> 2);
	p = p ^ (p >> 1);
	return p & 1;
}

/* Init the TPS56520 chip, set to 1.00V output */
bool tps56520_init(void)
{
	for(int retry = 3; retry > 0; retry--){
		if(tps56520_set(1100)){
			return true;
		}
	}
	
	return false;
}

bool tps56520_detect(void)
{
	return twi_probe(TWI0, TPS56520_ADDR);
}

/* Set voltage in mV for FPGA VCC_INT Voltage */
bool tps56520_set(uint16_t mv_output)
{
	/* Validate output voltage is in range */
	if ((mv_output < 600) || (mv_output > 1800)){
		return false;
	}
	
	/* Avoid frying FPGA */
	if (mv_output > 1200){
		return false;
	}
	
	uint8_t setting = (mv_output - 600) / 10;
	
	if (!checkoddparity(setting)){
		setting |= 1<<7;
	}

	if (i2c_write(TPS56520_ADDR, 0, &setting, 1)) {
		return false;
	}
	
	uint8_t volt_read;

	if (i2c_read(TPS56520_ADDR, 0, &volt_read, 1)) {
		return false;
	}
	
	if (volt_read == setting){
		return true;
	}

	return false;
}
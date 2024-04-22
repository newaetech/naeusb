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
#include "cdce906.h"
#include "i2c_util.h"

#define CDCE906_ADDR 0x69

// volatile uint8_t I2C_LOCK = 0;

/* Init the CDCE906 chip, set offline */
bool cdce906_init(void)
{
	i2c_setup();

	uint8_t data = 0;

	/* Read addr 0 */
	if (cdce906_read(0, &data) != 0){
		return false;
	}

	/* Check vendor ID matches expected */
	if ((data & 0x0F) == 0x01){
		return true;
	}

	return false;
}

int cdce906_write(uint8_t addr, uint8_t data)
{
	/* bit 7 of reg address set to 1 = byte mode*/
	return i2c_write(CDCE906_ADDR, addr | 0x80, &data, 1);
}

int cdce906_read(uint8_t addr, uint8_t * data)
{
	/* bit 7 of reg address set to 1 = byte mode*/
	return i2c_read(CDCE906_ADDR, addr | 0x80, data, 1);
}
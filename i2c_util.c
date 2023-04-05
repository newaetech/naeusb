#include <asf.h>
#include "i2c_util.h"

#ifndef TWI
#define TWI TWI0
#endif

volatile uint8_t I2C_LOCK = 0;
void i2c_setup(void)
{
	gpio_configure_pin(PIN_I2C_SDA, PIN_I2C_SDA_FLAGS);
	gpio_configure_pin(PIN_I2C_SCL, PIN_I2C_SCL_FLAGS);

    I2C_LOCK = 0;

	twi_master_options_t opt = {
		.speed = 50000,
		.chip  = 0x00
	};

	twi_master_setup(TWI, &opt);
}

void i2c_reset(void)
{
    twi_reset(TWI);
}

int i2c_write(uint8_t chip_addr, uint8_t reg_addr, void *data, uint8_t len)
{
    if (I2C_LOCK) {
        return -10;
    }
	I2C_LOCK = 1;
	twi_package_t packet_write = {
		.addr         = {reg_addr},      // TWI slave memory address data
		.addr_length  = 1,    // TWI slave memory address data size
		.chip         = chip_addr,      // TWI slave bus address
		.buffer       = data, // transfer data source buffer
		.length       = len  // transfer data size (bytes)
	};

    int rtn = 0;
	if (rtn = twi_master_write(TWI, &packet_write), rtn == TWI_SUCCESS){
		I2C_LOCK = 0;
		return 0;
	} else {
        // reset just in case
        i2c_reset();
		I2C_LOCK = 0;
		return rtn;
	}
}

int i2c_read(uint8_t chip_addr, uint8_t reg_addr, void *data, uint8_t len)
{
	if (I2C_LOCK) {
		return -10;
	}
	I2C_LOCK = 1;
	twi_package_t packet_read = {
		.addr         = {reg_addr},      // TWI slave memory address data
		.addr_length  = 1,    // TWI slave memory address data size
		.chip         = chip_addr,      // TWI slave bus address
		.buffer       = data,        // transfer data destination buffer
		.length       = len,                    // transfer data size (bytes)
	};

    int rtn = 0;
	if(rtn = twi_master_read(TWI, &packet_read), rtn == TWI_SUCCESS){
		I2C_LOCK = 0;
		return 0;
	} else {
		//reset just in case
		i2c_reset();
		I2C_LOCK = 0;
		return rtn;
	}
}
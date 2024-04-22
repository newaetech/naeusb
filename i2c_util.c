#include <asf.h>
#include "i2c_util.h"

#ifndef TWI
#define TWI TWI0
#endif

static volatile uint8_t I2C_LOCK = 0;
static volatile uint8_t I2C_SETUP = 0;
void i2c_setup(void)
{
	gpio_configure_pin(PIN_I2C_SDA, PIN_I2C_SDA_FLAGS);
	gpio_configure_pin(PIN_I2C_SCL, PIN_I2C_SCL_FLAGS);

    I2C_LOCK = 0;
	if (I2C_SETUP) return;

	// limited to 400kHz
	twi_master_options_t opt = {
		.speed = 100E3,
		.chip  = 0x00
	};

	twi_master_setup(TWI, &opt);
	I2C_SETUP = 1;
}

void i2c_reset(void)
{
	twi_master_options_t opt = {
		.speed = 100E3,
		.chip  = 0x00
	};
    twi_reset(TWI);
	twi_master_setup(TWI, &opt);
    I2C_LOCK = 0;
	// I2C_SETUP = 0;
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

int i2c_is_locked(void)
{
	return I2C_LOCK;
}

int raw_i2c_read(twi_package_t *packet)
{
	uint32_t rtn = 0;
	if (I2C_LOCK) {
		return -10;
	}
	I2C_LOCK = 1;
	rtn = twi_master_read(TWI0, packet);
	I2C_LOCK = 0;
	return rtn;
}

int raw_i2c_send(twi_package_t *packet)
{
	uint32_t rtn = 0;
	if (I2C_LOCK) {
		return -10;
	}
	I2C_LOCK = 1;
	rtn = twi_master_write(TWI0, packet);
	I2C_LOCK = 0;
	return rtn;
}
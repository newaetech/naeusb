#include <asf.h>
#include "circbuffer.h"
#include "usart_driver.h"
#include "naeusb_default.h"
#include "usart.h"
#include "usb_protocol_cdc.h"
#include "naeusb_usart.h"

#ifdef CW_TARGET_SPI

void spi1util_toggleclk(uint8_t cycles, bool mosi_status)
{
    if (mosi_status){
        gpio_set_pin_high(SPI_MOSI_GPIO);
        gpio_configure_pin(SPI_MOSI_GPIO, (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT));
    } else {
        gpio_set_pin_low(SPI_MOSI_GPIO);
        gpio_configure_pin(SPI_MOSI_GPIO, (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT));
    }
    gpio_set_pin_low(SPI_SPCK_GPIO);
	gpio_configure_pin(SPI_SPCK_GPIO, (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT));

    while(cycles--){
        gpio_set_pin_low(SPI_SPCK_GPIO);
        delay_cycles(1);
        gpio_set_pin_high(SPI_SPCK_GPIO);
        delay_cycles(1);
        gpio_set_pin_low(SPI_SPCK_GPIO);
    }
    
	gpio_configure_pin(SPI_MOSI_GPIO, SPI_MOSI_FLAGS);
	gpio_configure_pin(SPI_SPCK_GPIO, SPI_SPCK_FLAGS);
}

void spi1util_init(uint32_t prog_freq)
{
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
	gpio_configure_pin(SPI_MISO_GPIO, SPI_MISO_FLAGS);
}

void spi1util_deinit(void)
{
    ;
}

uint8_t spi1util_data_buffer[64];
int spi1util_databuffer_len = 0;

uint8_t spi1util_xferbyte(uint8_t b){
    uint16_t data = b;
    uint8_t ignored;
    spi_write(SPI, data, 0, 0);
    spi_read(SPI, &data, &ignored);
    return data;
}

void ctrl_spi1util(void) {
	
    uint32_t prog_freq = 100E3;
	switch(udd_g_ctrlreq.req.wValue){
		case 0xA0:
            if (udd_g_ctrlreq.req.wLength == 4) {
                prog_freq = *(CTRLBUFFER_WORDPTR);
            }
			spi1util_init(prog_freq);			
			break;
			
		case 0xA1:
			spi1util_deinit();
			break;
			
		case 0xA2: //Not implemented here, done via Python GPIO
			//spi1util_cs_low();
			break;

		case 0xA3: //Not implemented here, done via Python GPIO
			//spi1util_cs_high();
			break;

		case 0xA4:
			//Catch heartbleed-style error
			if (udd_g_ctrlreq.req.wLength > udd_g_ctrlreq.payload_size){
				return;
			}

			if (udd_g_ctrlreq.req.wLength > sizeof(spi1util_data_buffer)){
				return;
			}
			spi1util_databuffer_len = udd_g_ctrlreq.req.wLength;
			for (int i = 0; i < udd_g_ctrlreq.req.wLength; i++){
				spi1util_data_buffer[i] = spi1util_xferbyte(udd_g_ctrlreq.payload[i]);
			}

			break;
        
        case 0xA5:
            //nrst low (required due to sharing of nRST/SPI mode with programming)
            gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT));
            gpio_set_pin_low(PIN_TARG_NRST_GPIO);
            break;
        
        case 0xA6:
            //nrst high (required due to sharing of nRST/SPI mode with programming)
            gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT));
            gpio_set_pin_high(PIN_TARG_NRST_GPIO);
            break;
        
        case 0xA7:
            //nrst high-z (required due to sharing of nRST/SPI mode with programming)
            gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_INPUT | PIO_DEFAULT));
            break;
        
        case 0xA8:
            //Toggle SPCK pin as needed
            if(udd_g_ctrlreq.req.wLength == 2){
                spi1util_toggleclk(udd_g_ctrlreq.payload[0], udd_g_ctrlreq.payload[1]);
            }
            break;

		default:
			break;
	}
}
#endif
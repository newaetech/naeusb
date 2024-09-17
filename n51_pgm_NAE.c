#include "n51_pgm.h"

#include <asf.h>
#include "board.h"
#include "usart.h"
#include "sysclk.h"
#include "gpio.h"
#include "ioport.h"
#include "delay.h"

static uint8_t initialized = 0;

int N51PGM_init(void)
{
  /** TODO: Ensure that we are in AVR ISP Mode 
   * i.e. CW_IOROUTE_ADDR[5] is set to |= 0x01
  */
	gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_OUTPUT_0 | PIO_DEFAULT));
	gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_OUT_FLAGS);
	gpio_configure_pin(PIN_PDIDRX_GPIO, PIN_PDIDRX_FLAGS);
	gpio_configure_pin(PIN_PDIC_GPIO, PIN_PDIC_OUT_FLAGS);
#ifdef PIN_PDIDWR_GPIO
	gpio_set_pin_high(PIN_PDIDWR_GPIO);
	gpio_set_pin_high(PIN_PDICWR_GPIO);
#endif

  gpio_set_pin_low(PIN_PDIDTX_GPIO);
  gpio_set_pin_low(PIN_PDIC_GPIO);
  gpio_set_pin_low(PIN_TARG_NRST_GPIO);
  initialized = 1;
  return 0;
}

void gpio_set_pin(uint32_t pin, int val){
  if(val){
    gpio_set_pin_high(pin);
  }else{
    gpio_set_pin_low(pin);
  }
}

void N51PGM_set_dat(uint8_t val)
{
  gpio_set_pin(PIN_PDIDTX_GPIO, val);
}

uint8_t N51PGM_get_dat(void)
{
  return gpio_pin_is_high(PIN_PDIDRX_GPIO) ? 1 : 0;
}

void N51PGM_set_rst(uint8_t val)
{
  gpio_set_pin(PIN_TARG_NRST_GPIO, val);
}

void N51PGM_set_clk(uint8_t val)
{
  gpio_set_pin(PIN_PDIC_GPIO, val);
}

void N51PGM_set_trigger(uint8_t val)
{
  /** TODO: does nothing for now until I can figure out how to trigger GPIO4 from here**/
}

void N51PGM_dat_dir(uint8_t state)
{
  if(state){
    gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_OUT_FLAGS);
  }else{
    gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_IN_FLAGS);
  }
}

uint8_t N51PGM_is_init(){
  return initialized;
}

void N51PGM_deinit(uint8_t leave_reset_high)
{
  /* Tristate all pins */
	gpio_configure_pin(PIN_PDIC_GPIO, PIN_PDIC_IN_FLAGS);
	gpio_configure_pin(PIN_PDIDRX_GPIO, PIN_PDIDRX_FLAGS);
	gpio_configure_pin(PIN_PDIDTX_GPIO, PIN_PDIDTX_IN_FLAGS);

  if (leave_reset_high) {
    gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_OUTPUT_1 | PIO_DEFAULT));
    gpio_set_pin_high(PIN_TARG_NRST_GPIO);
  } else {
    gpio_configure_pin(PIN_TARG_NRST_GPIO, (PIO_TYPE_PIO_INPUT | PIO_DEFAULT));
  }
  initialized = 0;
}

uint32_t N51PGM_usleep(uint32_t usec)
{
  delay_us(usec);
  return usec;
}

// Not used by the ICP functions
uint64_t N51PGM_get_time(void){
  return 0;
}

void device_print(const char * msg){
}

# naeusb
USB library and build system for ChipWhisperer devices

Also includes other shared driver code (usart_driver, fpga_program code, etc.)

## Building

To build, navigate to the directory above this one (i.e. `cd ..`) and run `make`.

The makefile supports multiple jobs (`-j` flag) and supports the following options:

* `VERBOSE`: If not "false", output the full build command for each file. Defaults to "false"
* `GCCCOLOURS`: If "yes", force gcc to always output coloured text. If auto, leave it up to gcc. Otherwise, do not print colours. Defaults to "yes".

## Programming

Running `make program` will attempt to erase and program the device with the newly built firmware.

## Using the build system

1. Add this repository as a submodule in the project's firmware directory
1. Create a makefile in your project's firmware directory with the following lines:
    1. `TARGET = <X>`, where `<X>` is your target's name (e.g. ChipWhisperer-Husky)
    1. `SRC += <SRCFILES>` where `<SRCFILES>` are your custom source files (e.g. main.c, others)
    1. `SRC += <NAEUSBFILES>` where `<NAEUSBFILES>` are any source files you need from this directory
    1. `LINKERFILE = <LINKERFILE>` where `<LINKERFILE>` is the linker file you need to use
    1. `OPT=<X>` where `<X>` is the optimization level
    1. `include naeusb/makefile.cw`
1. Add a target entry for your target in `makefile.cw`
1. If you need to need to add a new HAL, create a new folder and put the files in there
1. The following files are required to be in the project's firmware directory
    1. conf_board.h
    1. conf_clock.h
    1. conf_sleepmgr.h
    1. conf_uart_serial.h
    1. conf_usb.h
    1. naeusb_board_config.h

## REQ_SAM3U_CFG

wValue & 0xFF:

1. 0xFx: Device specific config
1. 0x4x: MPSSE config
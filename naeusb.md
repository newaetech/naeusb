# NAEUSB

## Quick Overview

This repository contains common files and the build system for the various ChipWhisperer
capture and FPGA target devices, as well as some other NewAE products. This includes most
USB code, various programming protocols (AVR, XMEGA, etc.), FPGA programming code, and more.

It also has driver code for each device from Microchip in the respective device folder (`sam3u_hal`, `sam3x_hal`, `sam4s_hal`).

## USB Overview

### USB Basics

For more info, check https://www.beyondlogic.org/usbnutshell/usb1.shtml

USB is a communication specification, commonly used to send data between a computer
and a peripheral device (mouse, keyboard, flash memory, ChipWhisperer, etc.). In USB, all communication
is initiated by the host, which in ChipWhisperer's case is a PC running ChipWhisperer software. 
If the host wants to send data, it will setup an OUT transfer. If it wants to receive data, 
it will setup an IN transfer. From there, it is up to the device to either handle the data, or send
the data that the host wants.

USB also has various types of transfers which are useful in different situations. ChipWhisperer makes
use of two types. The first is control, which is good for small data transfers. The second is bulk,
which is useful for large data transfers. For ChipWhisperer, we use control transfers most of the time,
but switch to bulk when we need to do large transfers. For example, we might use a control transfer
to initiate FPGA reprogramming, then use bulk transfers to send the actual bitstream. Basically we
care about:

* Control IN
* Control OUT
* Bulk IN
* Bulk OUT

Control transfers are always done on endpoint 0, while bulk transfers are done on their own endpoint.
This is typically `X` for OUT and `Y | 0x80`, where `X` and `Y` are larger than zero and less than the
number of endpoints available on a device. Multiple endpoints are available, so you could, for example,
setup 2 bulk OUT endpoints and 1 bulk IN endpoint. ChipWhisperer devices usually have 1 bulk OUT and 1 bulk
IN endpoint for general communication and may have additional ones available for other uses, such as for
MPSSE (JTAG/SWD debugging) or CDC (USB serial, COM port, /dev/ttyACM0, etc.)

### Control Transfers

To make it easy to have both common and device specific control transfer handlers,
naeusb uses an array of function points to handle control requests. A new function
pointer can be added with `naeusb_add_out_handler()` and `naeusb_add_in_handler()`.

The function `main_setup_out_received()` and `main_setup_in_received()` are automatically 
called when we get a control OUT or control IN request, respectively. They call each function
added by `naeusb_add_out_handler()` or `naeusb_add_in_handler()`, respectively. This is done until
either one of the functions called returns `true`, in which case the function returns `true` and the
transaction succeeds, or they all return `false`, and the transaction fails (this results in a 
PIPE error on the host). The priority by which these handler functions are called is that the first added
handler is lowest priority, and the last added handler is highest priority. For example, if we have two
IN handlers, `handle_in_0()` and `handle_in_1()` added in that order, `handle_in_1()` will be called first.
If that function returns `true`, `handle_in_0()` will not be called.

When handling control transfers, all the relevant data is contained in the global variable 
`udd_g_ctrlreq`, with the fields:

 * `udd_g_ctrlreq.req.bRequest` - 1 byte containing the command to run. Examples include requesting the firmware version,
 setting up FPGA programming, or erasing firmware
 * `udd_g_ctrlreq.payload` - 0 or more bytes containing the data transferred by the transaction. Can be, for example,
the build date of the firmware, data to send to the FPGA, or UART data. For IN requests, payload is data sent from
the ChipWhisperer to the host. For OUT requests, payload is data sent from the host to the ChipWhisperer and is 
available 
 * `udd_g_ctrlreq.payload_size` - the length of the payload in bytes
 * `udd_g_ctrlreq.req.wValue` - 2 bytes of additional data. Usually used to specify additional settings to the handler.
 This value is always set by the host, so it's useful for sending small amounts of data during OUT requests.
 * `udd_g_ctrlreq.req.wLength` - 2 bytes describing the length requested by the host
 * `udd_g_ctrlreq.req.wIndex` - 2 bytes describing the index of the transaction
 * `udd_g_ctrlreq.callback` - A callback function which handles OUT transactions

IN requests are usually handled entirely by the handler, while OUT requests usually set `udd_g_ctrlreq.callback`, which will
handle the data that comes in. A 128 long buffer, `respbuf` is available for IN requests, while `ctrlbuffer` is used to store
the data received by OUT requests.

For example:

```C
bool my_in_handler(void)
{
    switch (udd_g_ctrlreq.req.bRequest) {
        case 0x00: // FW verison request
            respbuf[0] = FW_VER_0;
            respbuf[1] = FW_VER_1;
            respbuf[2] = FW_VER_2;
            udd_g_ctrlreq.payload = respbuf;
            udd_g_ctrlreq.payload_size = 3;
            return true;
        case 0x01: // example FPGA read, doesn't work like this in actual CW code
            udd_g_ctrlreq.payload = FPGA_MEM_ADDR + (udd_g_ctrlreq.req.wValue); //wValue as offset
            udd_g_ctrlreq.payload_size = udd_g_ctrlreq.req.wLength;
            return true;
    }
    return false;
}

uint16_t fpga_write_offset;
void fpga_write(void)
{
    for (uint16_t i = 0; i < udd_g_ctrlreq.req.payload_size; i++) {
        FPGA_MEM_ADDR[i + udd_g_ctrlreq.req.wValue] = ctrlbuffer[i]; // or ctrlbuffer[i]
    }
}

bool my_out_handler(void)
{
    // udd_g_ctrlreq.payload = ctrlbuffer; //done by function that calls my_out_handler()
    switch (udd_g_ctrlreq.req.bRequest) {
        case 0x01: //example FPGA write, doesn't work like this in actual CW code
            udd_g_ctrlreq.callback = fpga_write;
            return true;
    }
    return false;

}
```

### Bulk Transfers

Bulk transfers work a bit differently, behaving more like control OUT transfers in that
a memory address is setup and a callback function is assigned to handle the data transfer.

This is done via functions - `udi_vendor_bulk_in_run()` and `udi_vendor_bulk_out_run()`,
which take the following arguments:

* `uint8_t *buffer` - pointer to buffer to store data/read from
* `iram_size_t buf_size` - size of buffer
* `udd_callback_trans_t callback` - function pointer to bulk transfer handler

These functions are always setup for the standard ChipWhisperer bulk endpoints. `udd_ep_run()`
can be used to setup a bulk transfer on other endpoints, which takes the following arguments:

* `uint8_t ENDPOINT_NUM` - endpoint number to setup transfer on
* `uint8_t short_packet` - set to 0
* `uint8_t *buffer` - pointer to buffer to store data/read from
* `iram_size_t buf_size` - size of buffer
* `udd_callback_trans_t callback` - function pointer to bulk transfer handler

Bulk transfers can be cancelled by calling `udd_ep_abort(ENDPOINT_NUM)`.

On ChipWhisperer, bulk transfer parameters are usually setup by a control transfer beforehand,
specifying the address and amount of data to write/read. An OUT transfer is always available,
with the address and data length being handled in the callback and `udd_vendor_bulk_out_run()` being 
called in `callback()` to setup a new transfer. IN transfers are not setup beforehand, with `udi_vendor_bulk_in_run()` 
being called in the setup control transfer, but not in `callback()`.

### naeusb.c/h

Basic USB functions (what to do when suspended, resume, sof, etc.), as well as the 
the code related to calling and registering control transfer handlers.

### naeusb_default.c/h

Normal helpful control transfer handlers that can handle things like firmware version requests,
SAM3U configuration, and error reporting.

### naeusb_fpga_target.c/h

Handlers related to FPGA target configuration and communication (CW305, CW310, etc.)

### naeusb_mpsse.c/h

Handlers related to MPSSE (JTAG and SWD)

### naeusb_openadc.c/h

Handlers related to FPGA based capture boards (Lite, Pro, Husky)

### naeusb_usart.c/h

Handlers related to USART communication
# WCID

**NOTE:** Much of the information for this page comes from the [libwdi WCID Devices page](https://github.com/pbatard/libwdi/wiki/WCID-Devices) and the [MS OS 2.0 Descriptor Spec](https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-os-2-0-descriptors-specification). This page differs from the WCID Devices page as 
that page follows the earlier MS OS 1.0 Descriptor Spec.

A very annoying part of USB devices on Windows is that for custom vendor interfaces
(the sort of thing you use libusb/pyusb to communicate with), Windows will
not automatically assign a driver to the device. Traditionally, this meant
that the end user had to manually install a driver to use the device. This is
in contrast with Linux, which automatically assigns a generic USB driver to
devices, making them usable out of the box (ignoring permission issues,
which is its own can of worms).

Luckily, Microsoft has provided two main ways to
have devices automatically install or use a generic USB driver:

1. Go through WHQL, allowing users to automatically get your driver
through Windows Update
1. Have the USB device tell Windows that it wants to use the generic WinUSB driver

This page will focus on the latter, which is cheaper, easier, and sufficient for
most applications. It is worth noting that only WinUSB is built into Windows,
meaning that, if your device requires other drivers, you must use
the first method for this process to be fully automatic.

## USB 2.1 and BOS Descriptors

To get this all setup, we'll have to look at a part of USB
that is usually handled by whatever USB stack you're using (we're using MicroChip's ASF 4 USB Stack).
If your stack doesn't already support USB 2.1/BOS descriptors, you'll need to modify your USB
stack so that it does.

As mentioned earlier, USB devices can tell Windows that they would
like to be assigned the WINUSB driver. This is initiated
via the BOS Descriptor, a standard USB request that is 
used to get OS specific device information from a USB device. This is where
we run into our first hurdle: this request didn't make it into the USB spec until
USB 3.0 - there's no BOS descriptor in USB 2.0. The way we, as a USB 2.0 device,
get around this is by indicating that we are a USB 2.1 device instead 
of a USB 2.0 device. More specifically, this is done via the bcdUSB field.

### USB 2.1???

If you've dealt with USB for any length of time, you've probably heard
of a number of USB specifications put out over the years:
USB 1.0, USB 1.1, USB 2.0, USB 3.0, USB 3.1, etc.

Something you've likely never heard of is USB 2.1. The reason for
that is fairly simple: there's no USB 2.1 specification and, as such,
no USB 2.1 compliant devices. Instead, if a device identifies as a 
USB 2.1 device, it means it supports some of the new USB 3.0 requests.
In our case, if we identify as a USB 2.1 device, Windows
will still request a BOS descriptor from our device, allowing us
to tell Windows to use WinUSB for our device.

### The BOS Descriptor

As part of the enumeration of a USB 2.1 device, Windows will request a BOS
Descriptor (`bmRequestType == 0x80`, `bRequest == 0x06`, `wValue == 0x0F`)

The following C struct describes the BOS descriptor data:

```C

struct MS_BOS_DESCRIPTOR {
    uint8_t bLength; // to bNumPlatDesc: 0x05
    uint8_t bRequestType; //0x0F
    uint8_t wTotalLen[2]; // sizeof(struct MS_BOS_DESCRIPTOR)
    uint8_t bNumPlatDesc; // The number of platform descriptors - just 1 needed for WINUSB

    uint8_t bDescLen; // len to end of struct
    uint8_t bDescType; //0x10
    uint8_t bCapabilityType; //0x05
    uint8_t bReserved; //0x00

    /*
.MSGUID =  {0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, \
            0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F}, \
    */
    uint8_t MSGUID[16]; 

    uint8_t dwWinVersion[4]; //0x06030000 for Win 8.1 and above

    uint8_t wOSDescLen[2]; // sizeof(struct MS_OS_DESC_SET_HEADER)
    uint8_t bRequest; // bRequest used to get MS_OS_DESC_SET_HEADER
    uint8_t bSendRequest; //0x00
};
```

we'll only be using `uint8_t` fields here to avoid endianness and
struct padding issues.

##  MS OS Descriptor Set

Once the OS has requested and received the MS_BOS_DESCRIPTOR, the OS will request
an MS OS Descriptor set (`bRequestType == 0xC0`, `bRequest = MS_BOS_DESCRIPTOR::bRequest`).

The MS OS Descriptor Set is divided into different sections. These sections
are combined into a single packet as described using the following
structs:

### Descriptor Set Header

The header for the descriptor set has the following format
and is always required

```C
struct MS_OS_DESC_SET_HEADER {
    uint8_t wLength[2]; // 0x0A
    uint8_t wDescriptorType[2];  // 0x0000
    uint8_t dwWindowsVersion[4]; //0x06030000
    uint8_t wTotalLength[2]; //sizeof(struct MS_OS_DESC_SET_HEADER) including MS_FUNC_SUBSET_HEADER
    struct MS_FUNC_SUBSET_HEADER FUNC[N]; // the rest of the header
};
```

where N is the number of interfaces you want to apply the WinUSB driver to, which may differ
from the total number of interfaces that your device has. For example, if you have 2 vendor interfaces
that you want to apply the WinUSB driver to and 1 CDC interface, N=2.

### Function Subset Header

This header is used to indicate which interface to apply the following
feature information. If our device has multiple interfaces,
we can use this header to specify which interface is our custom vendor
interface (aka which interface the WINUSB driver should be used for):

```C
struct MS_FUNC_SUBSET_HEADER {
    uint8_t wLength[2]; // 0x08
    uint8_t wDescriptorType[2]; //0x02
    uint8_t bFirstInterface; // interface number
    uint8_t bReserved; //0x00
    uint8_t wSubsetLength[2]; //sizeof(struct MS_FUNC_SUBSET_HEADER) including MS_COMP_ID_FEAT_DESC
    struct MS_COMP_ID_FEAT_DESC FEAT;
};
```

Each interface that you want to apply the WinUSB driver to requires its own MS_FUNC_SUBSET_HEADER.
bFirstInterface is the interface number that you want to apply the WinUSB driver to.

If the device only has a single interface, this header must be skipped:

```C
struct MS_FUNC_SUBSET_HEADER {
    struct MS_COMP_ID_FEAT_DESC FEAT;
};
```

### Feature Description

In our case, the feature description is used to
specify that WINUSB should be used. This is done
by setting the CompatibleID field to "WINUSB":

```C
struct MS_COMP_ID_FEAT_DESC {
    uint8_t wLength[2]; // 0x14
    uint8_t wDescriptorType[2]; //0x03
    uint8_t CompatibleID[8]; //{'W', 'I', 'N', 'U', 'S', 'B', '\0', 0x00} for WINUSB
    uint8_t SubCompatibleID[8]; //0x00
    struct MS_REG_PROP_DESC_GUID GUID;
};
```

### GUID Description

In order to have Windows use WINUSB for our device, we need
to write a GUID (Globally Unique Identifier) into the Windows registry.
We can do this by sending a GUID descriptor, the format of which is
shown below:

```C
struct MS_REG_PROP_DESC_GUID {
    uint8_t wLength[2]; // sizeof(struct MS_REG_PROP_DESC_GUID)
    uint8_t wDescriptorType[2]; //0x04
    uint8_t wStringType[2]; // 0x07 - UTF-16 encoded null terminated strings
    uint8_t wPropertyNameLength[2]; // sizeof(struct MS_DEV_GUID_NAME)
    struct MS_DEV_GUID_NAME PropertyName; // covered below
    uint8_t wPropertyDataLength[2]; // sizeof (struct MS_DEV_INT_GUID)
    struct MS_DEV_INT_GUID PropertyData; // covered below
};
```


#### The GUID

All strings for the GUID are formatted as null terminated UTF-16, meaning
each character is a minimum of two characters. If
only ASCII characters are required, they can be
formatted as the ASCII character in questions, followed by
a zero. For example, `"DeviceInterfaceGUIDs"` would be:

```C
uint8_t reg_name[] = { \
	'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00,\
	'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00,\
	'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,\
};
```

`MS_DEV_GUID_NAME` is the name of the field that will be entered
into the registry. If your device has one interface, it must be
"DeviceInterfaceGUID", while if it has more than one interface,
it must be "DeviceInterfaceGUIDs".

If the device has one interface `MS_DEV_INT_GUID` should be a
single null terminated GUID, which takes the following
form: `"{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"`,
which `x` is a hexadecimal character (`'0'` through `'9'` and `'A'` through `'F'`).

An example GUID would be:

```C
uint8_t GUID[] = { \
'{', 0x00, \
'A', 0x00, 'B', 0x00, '1', 0x00, '2', 0x00, \
'A', 0x00, 'B', 0x00, '1', 0x00, '2', 0x00, \
'-', 0x00, \
'A', 0x00, 'B', 0x00, '1', 0x00, '2', 0x00, \
'-', 0x00, \
'A', 0x00, 'B', 0x00, '1', 0x00, '2', 0x00, \
'-', 0x00, \
'A', 0x00, 'B', 0x00, '1', 0x00, '2', 0x00, \
'-', 0x00, \
'A', 0x00, 'B', 0x00, '1', 0x00, '2', 0x00, \
'A', 0x00, 'B', 0x00, '1', 0x00, '2', 0x00, \
'A', 0x00, 'B', 0x00, '1', 0x00, '2', 0x00, \
'}', 0x00, 0x00, 0x00 \
}; // "{AB12AB12-AB12-AB12-AB12-AB12AB12AB12}"
```

If a device has more than one interface, then
`MS_DEV_INT_GUID` should be one or more null terminated GUIDs, followed by
an additional null terminator. The example GUID
from above would suffice, so long as an additional null terminator
(`{0x00, 0x00}`) is added at the end.

**NOTE:** The WCID Devices page says `MS_DEV_INT_GUID` should have
more than one GUID if the device has more than one interface and libusb
does report the GUID as malformed. However, it does still work with a single GUID,
whereas if you don't use "DeviceInterfaceGUIDs", it won't work at all.

## ChipWhisperer Example

ChipWhisperer's common firmware files (https://github.com/newaetech/naeusb)
contains the definitions required for the BOS Descriptor
and the MS OS Descriptor, as well as macros to generate them, in `naeusb_os_desc.h`.

A custom `udc.c` file which ensures the BCD USB is set to 2.1,
and implements the BOS and MS OS Descriptor responses is also included
in the repo.

## Troubleshooting

Unfortunately, there's a lot that can go wrong here and you don't get a lot of
feedback when things do go wrong. [usbtreeview](https://www.uwe-sieber.de/usbtreeview_e.html) is
a great utility for working with USB on Windows, as Device Manager gives you very little information.

If you mess up any of the descriptor structs above (wrong values, wrong lengths, missing descriptors)
Windows will likely tell you that your descriptor is wrong.

### LIBUSB_ERROR_NOT_FOUND

If you try to open your USB device, or try to access something that requires your device to be open, like
a serial number, and you get this error, the most common cause is that your GUID is malformed. Unfortunately,
libusb doesn't seem to pick this up (even though it will pick up if you only supply one GUID on
a device with multiple interfaces), so you're on your own to check this.

Common ways that your GUID can be malformed include not being the right length/format, or using
lower case letters instead of upper case letters.

Another symptom of this issue that you can see if you set `LIBUSB_DEBUG=4`, you won't see a driver being set 
for your device. The driver being set looks as follows:

```
[482.429517] [000049d8] libusb: debug [get_api_type] driver(s): WINUSB
[482.429647] [000049d8] libusb: debug [get_api_type] matched driver name against WinUSB
[482.429755] [000049d8] libusb: debug [winusb_get_device_list] setting composite interface for [91]:
[482.429853] [000049d8] libusb: debug [set_composite_interface] interface[0] = \\?\USB#VID_2B3E&PID_ACE2&MI_00#7&2413F3D9&0&0000#{CAF5AA1C-A69A-4995-ABC2-2AE57A51ADE9}
```

## Miscellaneous

### libusbK and libusb0

It is possible to use the MS OS Descriptor to have Windows automatically
use libusbK or libusb0 drivers for devices instead. These drivers, however,
are not included with Windows, meaning they must be installed before
Windows can have devices use them.

The easiest way to do this is via [Zadig](https://zadig.akeo.ie/),
which can force devices to use WinUSB, libusbK, or libusb0. Once this is
done for a single device, these other drivers should become 
available for any devices that request them.

See the [libwdi WCID Devices page](https://github.com/pbatard/libwdi/wiki/WCID-Devices) for
more information.

### 
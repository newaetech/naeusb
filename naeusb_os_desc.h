#pragma once

#define U162ARR(U16) {(U16) & 0xFF, (U16) >> 8}

struct MS_DEV_INT_GUID {
    uint8_t data[0x4E];
};

#ifndef USB_DEVICE_NB_INTERFACE
#define USB_DEVICE_NB_INTERFACE 1
#endif

#define MAKE_DEV_INT_GUID(INTERFACE_NUM) \
{.data = {'{', 0x00, \
INTERFACE_NUM+'0', 0x00, 'A', 0x00, 'C', 0x00, 'E', 0x00, \
'2', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, \
'-', 0x00, \
'2', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, \
'-', 0x00, \
'2', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, \
'-', 0x00, \
'2', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, \
'-', 0x00, \
((USB_DEVICE_PRODUCT_ID >> 12 ) & 0x07) + '0', 0x00, ((USB_DEVICE_PRODUCT_ID >> 8 ) & 0x07) + '0', 0x00, \
((USB_DEVICE_PRODUCT_ID >> 4 ) & 0x07) + '0', 0x00, ((USB_DEVICE_PRODUCT_ID >> 0 ) & 0x07) + '0', 0x00, \
INTERFACE_NUM + '0', 0x00, 'A', 0x00, 'C', 0x00, 'E', 0x00, \
'2', 0x00, 'B', 0x00, '3', 0x00, 'E', 0x00, \
'}', 0x00, 0x00, 0x00 \
}}

#if USB_DEVICE_NB_INTERFACE > 1

struct MS_DEV_GUID_NAME {
    uint8_t data[0x2A];
};

#define MAKE_DEV_GUID_NAME \
{.data = {\
	'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00,\
	'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00,\
	'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 's', 0x00, 0x00, 0x00,\
}}

#define MAKE_NULL_TERM .wNullTerminator={0x00, 0x00}
#define MS_NULL_TERM_SIZE  2

#else

struct MS_DEV_GUID_NAME {
    uint8_t data[0x28];
};

#define MAKE_DEV_GUID_NAME \
{.data = {\
	'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00,\
	'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00,\
	'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 0x00, 0x00,\
}}

#define MAKE_NULL_TERM 
#define MS_NULL_TERM_SIZE 0
#endif

struct MS_REG_PROP_DESC_GUID {
    uint8_t wLength[2];
    uint8_t wDescriptorType[2];
    uint8_t wStringType[2];
    uint8_t wPropertyNameLength[2];
    struct MS_DEV_GUID_NAME PropertyName;
    uint8_t wPropertyDataLength[2];
    struct MS_DEV_INT_GUID PropertyData;

    #if USB_DEVICE_NB_INTERFACE > 1
    uint8_t wNullTerminator[2];
    #endif
};


#define MAKE_PROP_DESC_GUID(INT) \
{ \
.wLength= U162ARR (sizeof(struct MS_REG_PROP_DESC_GUID)), \
.wDescriptorType = U162ARR(0x04), \
.wStringType = U162ARR(0x07), \
.wPropertyNameLength = U162ARR(sizeof(struct MS_DEV_GUID_NAME)), \
.PropertyName = MAKE_DEV_GUID_NAME, \
.wPropertyDataLength = U162ARR(sizeof(struct MS_DEV_INT_GUID) + MS_NULL_TERM_SIZE), \
.PropertyData = MAKE_DEV_INT_GUID(INT), \
MAKE_NULL_TERM \
}

struct MS_COMP_ID_FEAT_DESC {
    uint8_t wLength[2];
    uint8_t wDescriptorType[2];
    uint8_t CompatibleID[8];
    uint8_t SubCompatibleID[8];
    struct MS_REG_PROP_DESC_GUID GUID;
};

#define MAKE_FEAT_DESC(INT) \
{ \
.wLength=U162ARR(0x14), \
.wDescriptorType=U162ARR(0x03), \
.CompatibleID={'W', 'I', 'N', 'U', 'S', 'B', '\0', 0x00}, \
.SubCompatibleID={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, \
.GUID=MAKE_PROP_DESC_GUID(INT) \
}


#if USB_DEVICE_NB_INTERFACE > 1
struct MS_FUNC_SUBSET_HEADER {
    uint8_t wLength[2];
    uint8_t wDescriptorType[2];
    uint8_t bFirstInterface;
    uint8_t bReserved;
    uint8_t wSubsetLength[2];
    struct MS_COMP_ID_FEAT_DESC FEAT;
};

#define MAKE_FUNC_SUBSET_HEADER(INT) \
{ \
.wLength=U162ARR(0x08), \
.wDescriptorType=U162ARR(0x02), \
.bFirstInterface=INT, \
.bReserved=0x00, \
.wSubsetLength=U162ARR(sizeof(struct MS_FUNC_SUBSET_HEADER)), \
.FEAT=MAKE_FEAT_DESC(INT) \
}
#else
struct MS_FUNC_SUBSET_HEADER {
    struct MS_COMP_ID_FEAT_DESC FEAT;
};

#define MAKE_FUNC_SUBSET_HEADER(X) \
{ \
.FEAT=MAKE_FEAT_DESC(0) \
}
#endif

struct MS_OS_DESC_SET_HEADER {
    uint8_t wLength[2];
    uint8_t wDescriptorType[2];
    uint8_t dwWindowsVersion[4];
    uint8_t wTotalLength[2];
#if USB_DEVICE_NB_INTERFACE > 1
    struct MS_FUNC_SUBSET_HEADER FUNC[2];
#else
    struct MS_FUNC_SUBSET_HEADER FUNC[1];
#endif
};

#if USB_DEVICE_NB_INTERFACE > 1
#define MAKE_OS_DESC_SET_HEADER \
{ \
.wLength=U162ARR(0x0A), \
.wDescriptorType=U162ARR(0x00), \
.dwWindowsVersion={0x00, 0x00, 0x03, 0x06}, \
.wTotalLength=U162ARR(sizeof(struct MS_OS_DESC_SET_HEADER)), \
.FUNC = {MAKE_FUNC_SUBSET_HEADER(0), MAKE_FUNC_SUBSET_HEADER(1)} \
}
#else
#define MAKE_OS_DESC_SET_HEADER \
{ \
.wLength=U162ARR(0x0A), \
.wDescriptorType=U162ARR(0x00), \
.dwWindowsVersion={0x00, 0x00, 0x03, 0x06}, \
.wTotalLength=U162ARR(sizeof(struct MS_OS_DESC_SET_HEADER)), \
.FUNC = {MAKE_FUNC_SUBSET_HEADER(0)} \
}
#endif

static struct MS_OS_DESC_SET_HEADER MS_OS_DESC = MAKE_OS_DESC_SET_HEADER;

struct MS_BOS_DESCRIPTOR {
    uint8_t bLength; //5 always
    uint8_t bRequestType;
    uint8_t wTotalLen[2];
    uint8_t bNumPlatDesc;

    uint8_t bDescLen;
    uint8_t bDescType; //0x10
    uint8_t bCapabilityType; //0x05
    uint8_t bReserved;

    uint8_t MSGUID[16];

    uint8_t dwWinVersion[4];

    uint8_t wOSDescLen[2];
    uint8_t bRequest;
    uint8_t bSendRequest; //0x00
};

#define MAKE_BOS_DESC \
{ \
.bLength = 0x05,\
.bRequestType = 0x0F, \
.wTotalLen = U162ARR(sizeof(struct MS_BOS_DESCRIPTOR)), \
.bNumPlatDesc = 0x01, \
.bDescLen = 0x1C, \
.bDescType = 0x10, \
.bCapabilityType = 0x05, \
.bReserved = 0x00, \
.MSGUID =  {0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, \
            0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F}, \
.dwWinVersion = {0x00, 0x00, 0x03, 0x06}, \
.wOSDescLen = U162ARR(sizeof(struct MS_OS_DESC_SET_HEADER)), \
.bRequest=0x01, \
.bSendRequest=0x00 \
}

static struct MS_BOS_DESCRIPTOR MS_BOS_PACKET = MAKE_BOS_DESC;

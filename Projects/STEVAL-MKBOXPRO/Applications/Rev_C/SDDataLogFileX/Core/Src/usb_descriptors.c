/* USB descriptors for SDDataLogFileX CDC ACM device.
 *
 * Single CDC interface — virtual COM port for debug printf streaming.
 * Compiled in only when STBOX1_ENABLE_USB_CDC is set.
 */
#include "stbox1_config.h"

#if STBOX1_ENABLE_USB_CDC

#include "tusb.h"

/* USB Vendor / Product IDs.
 * Using TinyUSB's pid.codes-allocated test VID/PID for now (0xCAFE / 0x4001 =
 * CDC class). For a production product release these would be replaced with
 * real ywesee VID/PID — until then the device shows up as "TinyUSB CDC" on
 * the host but enumerates correctly. macOS / Linux don't need a driver
 * (built-in CDC ACM); Windows ≥10 also auto-attaches usbser.sys.
 */
#define USB_VID   0xCafeu
#define USB_PID   0x4001u
#define USB_BCD   0x0200u  /* USB 2.0 */

/* ---------------------------------------------------------------------------
 * Device descriptor
 * --------------------------------------------------------------------------*/
tusb_desc_device_t const desc_device = {
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = USB_BCD,

  /* Microsoft IAD-style composite. Matches AppleUSBCDC's
   * "CDCCompositeDevice Misc" personality directly (bDeviceClass=239 +
   * bDeviceSubClass=2 + bDeviceProtocol=1). Pure CDC class (0x02) only
   * triggered partial-attach on macOS Sequoia. The IAD form is what
   * Arduino/STM32duino/etc use successfully. */
  .bDeviceClass       = TUSB_CLASS_MISC,             /* 0xEF */
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,         /* 0x02 */
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,            /* 0x01 */
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor           = USB_VID,
  .idProduct          = USB_PID,
  .bcdDevice          = 0x0100,

  .iManufacturer      = 0x01,
  .iProduct           = 0x02,
  .iSerialNumber      = 0x03,

  .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

/* ---------------------------------------------------------------------------
 * Configuration descriptor (one CDC interface)
 * --------------------------------------------------------------------------*/
enum {
  ITF_NUM_CDC = 0,
  ITF_NUM_CDC_DATA,
  ITF_NUM_TOTAL
};

/* Standard IAD-included CDC ACM descriptor. With bDeviceClass = 0xEF
 * (MISC) at the device level + IAD in the config descriptor, this matches
 * AppleUSBCDC's "CDCCompositeDevice Misc" personality and is the form
 * Arduino, STM32duino, ChibiOS et al. ship — known to work on macOS
 * including Sequoia 15+. */
#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82

uint8_t const desc_fs_configuration[] = {
  /* Config: itf count, string idx, total len, attr (0x80 = bus-powered),
   * max power 100 mA. */
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 100),

  /* CDC: itf, string idx, EP notify addr, EP notify size, EP OUT, EP IN,
   * EP size. 64 B EP size = full speed bulk max. The macro emits an IAD
   * prefix that's required by macOS to attach AppleUSBCDC. */
  TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, EPNUM_CDC_NOTIF, 8, EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return desc_fs_configuration;
}

/* ---------------------------------------------------------------------------
 * String descriptors
 * --------------------------------------------------------------------------*/
char const * string_desc_arr[] = {
  (const char[]){ 0x09, 0x04 }, /* 0: language id (English US, 0x0409) */
  "ywesee GmbH",                /* 1: Manufacturer */
  "SensorTile.box PRO Debug",   /* 2: Product */
  "0123456789ABCDEF",           /* 3: Serial — placeholder, not unique per
                                 *    chip yet. The host doesn't need a real
                                 *    one unless multiple boxes are plugged
                                 *    in simultaneously. */
  "STBoxPro CDC",               /* 4: CDC interface name */
};

static uint16_t _desc_str[32];

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  uint8_t chr_count;

  if (index == 0) {
    memcpy(&_desc_str[1], string_desc_arr[0], 2);
    chr_count = 1;
  } else {
    if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;

    const char *str = string_desc_arr[index];
    chr_count = (uint8_t) strlen(str);
    if (chr_count > 31) chr_count = 31;

    /* Convert ASCII string into UTF-16. */
    for (uint8_t i = 0; i < chr_count; i++) {
      _desc_str[1 + i] = str[i];
    }
  }

  /* First byte = total length, second byte = type (string descriptor = 0x03). */
  _desc_str[0] = (TUSB_DESC_STRING << 8) | (2u * chr_count + 2u);
  return _desc_str;
}

#endif /* STBOX1_ENABLE_USB_CDC */

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

  /* Pure CDC ACM device (no other classes). On macOS Sequoia 15+ the
   * built-in CDC ACM driver doesn't fully attach (partial probe, ioreg
   * shows AppleUSBCDCCompositeDevice as `!matched`), so /dev/tty.usbmodem
   * never appears. We work around this with Utilities/usb_console.py
   * which uses libusb directly + sudo to claim the bulk-IN endpoint.
   *
   * Why CDC instead of vendor-specific: Sequoia *silently blocks*
   * vendor-class accessories (no "Allow accessory" prompt) — they don't
   * even show up in system_profiler. CDC at least shows up and triggers
   * the prompt; the kernel driver's partial attach is annoying but
   * pyusb-with-sudo handles it. Net: CDC works, vendor doesn't. */
  .bDeviceClass       = TUSB_CLASS_CDC,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,
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

/* Hand-rolled CDC ACM config descriptor WITHOUT the IAD prefix that
 * TUD_CDC_DESCRIPTOR includes. IAD (Interface Association Descriptor) is
 * only valid when bDeviceClass = 0xEF (MISC); we set bDeviceClass = 0x02
 * (CDC) so IAD is a spec violation that some hosts (notably macOS Sequoia)
 * silently reject — the device enumerates at the device-descriptor level
 * but never advances to SET_CONFIGURATION because the host treats the
 * config descriptor as malformed.
 *
 * Structure (66 - 8 = 58 bytes after dropping IAD):
 *   - 9  Communications Interface (class 2 / sub 2 / proto 1)
 *   - 5  CDC Header functional
 *   - 5  CDC Call Management functional
 *   - 4  CDC ACM functional
 *   - 5  CDC Union functional
 *   - 7  Notification endpoint (EP 0x81 IN, interrupt, 8 B)
 *   - 9  Data Interface (class 0x0A)
 *   - 7  Bulk OUT endpoint (EP 0x02, 64 B)
 *   - 7  Bulk IN  endpoint (EP 0x82, 64 B)
 * Total: 58 bytes
 */
#define CDC_DESC_LEN_NO_IAD 58
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + CDC_DESC_LEN_NO_IAD)

#define EPNUM_CDC_NOTIF   0x81
#define EPNUM_CDC_OUT     0x02
#define EPNUM_CDC_IN      0x82

uint8_t const desc_fs_configuration[] = {
  /* Config: itf count, string idx, total len, attr (0x80 = bus-powered, no
   * remote wakeup), max power 100 mA. */
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 100),

  /* Communications Class Interface: class 2 (CDC), subclass 2 (ACM),
   * protocol 1 (AT commands), iInterface = 4 (string "STBoxPro CDC").
   *
   * NOTE: With device-class = 0xFF (vendor) we keep the CDC interfaces
   * intact for hosts (Linux, Windows) that *can* attach a CDC driver.
   * macOS sees vendor at the device level and ignores them. */
  9, TUSB_DESC_INTERFACE, ITF_NUM_CDC, 0, 1,
  TUSB_CLASS_CDC, CDC_COMM_SUBCLASS_ABSTRACT_CONTROL_MODEL, CDC_COMM_PROTOCOL_ATCOMMAND, 4,

  /* CDC Header functional descriptor — bcdCDC = 0x0120 (CDC 1.20). */
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_HEADER, 0x20, 0x01,

  /* CDC Call Management functional — bmCapabilities = 0 (no call mgmt),
   * bDataInterface = ITF_NUM_CDC + 1 (the data interface number). */
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_CALL_MANAGEMENT, 0x00, (uint8_t)(ITF_NUM_CDC + 1),

  /* CDC ACM functional — bmCapabilities = 0x06: support GetLineCoding,
   * SetLineCoding, SetControlLineState + Send_Break. */
  4, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_ABSTRACT_CONTROL_MANAGEMENT, 0x06,

  /* CDC Union functional — control + data interfaces. */
  5, TUSB_DESC_CS_INTERFACE, CDC_FUNC_DESC_UNION, ITF_NUM_CDC, (uint8_t)(ITF_NUM_CDC + 1),

  /* Notification endpoint: address EP 0x81 (IN), interrupt transfer,
   * 8 B max packet, 16 ms polling interval. */
  7, TUSB_DESC_ENDPOINT, EPNUM_CDC_NOTIF, TUSB_XFER_INTERRUPT, 0x08, 0x00, 16,

  /* Data Class Interface: class 0x0A (CDC Data), 0 sub, 0 proto. */
  9, TUSB_DESC_INTERFACE, (uint8_t)(ITF_NUM_CDC + 1), 0, 2, TUSB_CLASS_CDC_DATA, 0, 0, 0,

  /* Bulk OUT endpoint: address EP 0x02, bulk transfer, 64 B. */
  7, TUSB_DESC_ENDPOINT, EPNUM_CDC_OUT, TUSB_XFER_BULK, 0x40, 0x00, 0,

  /* Bulk IN endpoint: address EP 0x82, bulk transfer, 64 B. */
  7, TUSB_DESC_ENDPOINT, EPNUM_CDC_IN,  TUSB_XFER_BULK, 0x40, 0x00, 0,
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

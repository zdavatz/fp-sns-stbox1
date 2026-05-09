/* TinyUSB project configuration for SDDataLogFileX on STM32U585 (SensorTile.box PRO).
 *
 * USB FS device class = CDC ACM (virtual COM port). The whole stack is gated
 * behind STBOX1_ENABLE_USB_CDC in stbox1_config.h; with the flag off, no
 * TinyUSB source compiles in (Makefile filters them) so the firmware footprint
 * is byte-identical to the no-USB build.
 */
#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* MCU and family --- DWC2 OTG_FS on STM32U5. */
#define CFG_TUSB_MCU              OPT_MCU_STM32U5
#define CFG_TUSB_RHPORT0_MODE     OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED

/* OS abstraction. We drive tud_task() from a dedicated ThreadX thread so the
 * none-OS variant is fine: TinyUSB uses simple atomic flags for IPC and our
 * service loop calls tud_task() repeatedly with a short sleep. */
#define CFG_TUSB_OS               OPT_OS_NONE

/* Quiet by default; flip to 1/2/3 if a regression needs USB-stack tracing.
 * Note: TinyUSB debug printf would re-enter our hooked __io_putchar over USB
 * and recursively re-enter the stack, so debug must stay off here. */
#define CFG_TUSB_DEBUG            0

/* Memory placement attributes (defaults are fine for STM32U5). */
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN        __attribute__((aligned(4)))

/* Device classes ---------------------------------------------------------- */
#define CFG_TUD_ENABLED           1
#define CFG_TUD_ENDPOINT0_SIZE    64

#define CFG_TUD_CDC               1
#define CFG_TUD_MSC               0
#define CFG_TUD_HID               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0

/* CDC FIFO sizes. TX wants enough room for a full burst of printf without
 * blocking the producer (e.g. start-of-session diagnostic dump from
 * ble_sync_thread). RX is small because we don't currently consume host->dev
 * traffic; a future shell would bump this. */
#define CFG_TUD_CDC_RX_BUFSIZE    256
#define CFG_TUD_CDC_TX_BUFSIZE    4096

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */

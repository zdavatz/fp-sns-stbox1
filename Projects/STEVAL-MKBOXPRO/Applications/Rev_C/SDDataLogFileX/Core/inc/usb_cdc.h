/* USB CDC ACM debug console for SDDataLogFileX.
 *
 * Brings up the STM32U585 OTG_FS peripheral as a virtual COM port so a host
 * PC sees the box as /dev/tty.usbmodem* (macOS), /dev/ttyACM* (Linux), or
 * COMxx (Windows). Hooks __io_putchar so STBOX1_PRINTF and the standard
 * printf() route to USB.
 *
 * Use case: live-debug the BLE bringup. UART4 is owned by GPS, so SD error
 * log was the only diagnostic — and it requires power-cycling + pulling the
 * card. With USB CDC the chip can stream diagnostic lines while running.
 *
 * Gated on STBOX1_ENABLE_USB_CDC. When the flag is 0 the entire stack
 * (TinyUSB + descriptors + this file) is excluded from the build by the
 * Makefile, and the strong __io_putchar override is gone too — so the
 * firmware footprint matches a build without USB CDC entirely.
 */
#ifndef USB_CDC_H
#define USB_CDC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "stbox1_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#if STBOX1_ENABLE_USB_CDC

/* Configure the USB peripheral hardware: enable VDDUSB power island, route
 * HSI48 to the OTG_FS clock input, configure PA11/PA12 as USB DM/DP, enable
 * the OTG_FS RCC clock, and call tud_init(0) to start the TinyUSB device
 * stack. Must run before tx_kernel_enter() because the OTG_FS NVIC line
 * fires from boot once tud_init has armed it.
 *
 * Safe to call when no USB cable is plugged in — the stack stays idle until
 * VBUS is detected and a host begins enumeration.
 */
/* Returns true if tud_rhport_init succeeded, false otherwise. The caller
 * (main.c) should signal failure with a distinctive LED pattern so a host
 * lacking the USB device knows whether to suspect cable/host or firmware. */
bool UsbCdc_Init(void);

/* Spawn the ThreadX service thread (priority 13, between fx_thread at 12 and
 * ble_sync_thread at 14). The thread loops `tud_task(); tx_thread_sleep(1);`
 * which advances TinyUSB's state machine — handles control transfers,
 * flushes TX FIFO, accepts RX. Must be called from App_ThreadX_Init().
 *
 * memory_ptr: the same TX_BYTE_POOL passed to App_ThreadX_Init.
 * Returns TX_SUCCESS or a TX_* error code.
 */
unsigned int UsbCdc_ThreadX_Init(void *memory_ptr);

/* Push a NUL-terminated string to the USB CDC TX FIFO. Non-blocking: if the
 * host hasn't claimed the FIFO drain (no terminal open, or terminal stalled),
 * any overflow bytes are silently dropped so the caller (e.g. fx_thread mid
 * SD-write) never stalls behind the USB. Returns the number of bytes
 * actually queued.
 */
size_t UsbCdc_WriteString(const char *s);

/* Push a raw buffer to the USB CDC TX FIFO. Same non-blocking semantics as
 * UsbCdc_WriteString. Useful for binary diagnostic markers. */
size_t UsbCdc_Write(const void *buf, size_t len);

#else  /* STBOX1_ENABLE_USB_CDC */

/* Stubs so call sites can compile without their own #if guards. */
static inline bool UsbCdc_Init(void) { return false; }
static inline unsigned int UsbCdc_ThreadX_Init(void *p) { (void)p; return 0u; }
static inline size_t UsbCdc_WriteString(const char *s) { (void)s; return 0u; }
static inline size_t UsbCdc_Write(const void *b, size_t n) { (void)b; (void)n; return 0u; }

#endif /* STBOX1_ENABLE_USB_CDC */

#ifdef __cplusplus
}
#endif

#endif /* USB_CDC_H */

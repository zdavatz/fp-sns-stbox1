/**
  ******************************************************************************
  * @file    wlc.h
  * @brief   BlueNRG-LP OTP programmer (Wireless LE Controller flasher) port.
  *
  * Reverse-engineered from ST's `SensorTile.boxPRO.bin` — the factory firmware
  * that performs first-boot OTP burning. See top-level CLAUDE.md "BlueNRG-LP
  * OTP / WLC flashing" for context.
  *
  * Build flag: STBOX1_ENABLE_WLC in stbox1_config.h (default 0). With the flag
  * off, WLC_CheckAndProgram() compiles to a no-op stub returning
  * WLC_STATUS_DISABLED. Default-off because the code is untested on real
  * hardware as of writing — the manual setup procedure documented in
  * README.md ("First-time BlueNRG-LP OTP setup") still works and is the
  * recommended path for field testers.
  *
  * Wire protocol (over the same SPI1 link the HCI stack uses, manually
  * CS-toggled, no HCI framing):
  *   WRITE: 0xFA + 4-byte big-endian addr + N data bytes
  *   READ-cmd:  opcode-byte + N receive bytes
  *   READ-addr: 0x05 + 4-byte big-endian addr + N receive bytes
  *   REG-WRITE: 2-byte big-endian reg + N data bytes (no 0xFA prefix)
  *
  * Algorithm summary (FUN_08068f50):
  *   1. Probe operation mode (must == 1, "iload bootloader")
  *   2. Read vega_chip_info — verify cut id 0x05; check cfg/patch IDs vs
  *      target 0x50e0 / 0x3657. If both match: nothing to do.
  *   3. Stage the relevant patch+config blob from `dtm.bin` into RAM at
  *      0x50000 (chunked 0x80 byte writes with read-back verify, 3 retries)
  *   4. Boot chip from RAM; read RAM FW version
  *   5. Disable iload, write OTP control regs (0x146=0x00C0, 0x145=0x40),
  *      wait 300 ms
  *   6. OTP write loop: 4 KB pages, write to 0x51000+; per page write
  *      length to reg 0x14c, stage to RAM at OTP target, kick programming
  *      via reg 0x145=0x01, poll PE3 GPIO for completion (max 20 s),
  *      reset status via reg 0x12=0x10
  *   7. Verify cfg/patch IDs again
  *
  * dtm.bin layout (from FUN_08068bc0/FUN_08068f50 offsets):
  *   0x0000..0x153f — ignored / header / signature
  *   0x1540..0x21ef — RAM bootloader image (0xc50 bytes)  → upload to 0x50000
  *   0x21f0..0x23ef — OTP CONFIG image (0x200 bytes)      → upload to 0x51000
  *   0x23f0..0x318f — OTP PATCH image  (0xda0 bytes)      → upload to 0x51200
  *
  * The 8-byte trailer 0xA7 0xFF 0xFF 0x00 0x58 0xFF 0x00 0x00 is appended in
  * RAM after both patch+config; it's a fingerprint/version block the chip
  * consults during validation.
  ******************************************************************************
  */

#ifndef WLC_H
#define WLC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32u5xx_hal.h"
#include <stdint.h>
#include <stddef.h>

typedef enum {
  WLC_STATUS_DISABLED       = 0,  /* STBOX1_ENABLE_WLC == 0 */
  WLC_STATUS_NOT_REQUIRED   = 1,  /* Chip already programmed (cfg+patch match) */
  WLC_STATUS_PROGRAMMED     = 2,  /* OTP just burned successfully this boot */
  WLC_STATUS_PROBE_FAILED   = 3,  /* Couldn't talk to chip in iload mode */
  WLC_STATUS_BAD_DTM        = 4,  /* dtm.bin missing or malformed */
  WLC_STATUS_PROG_FAILED    = 5,  /* OTP burn returned an error */
  WLC_STATUS_VERIFY_FAILED  = 6,  /* Burn succeeded but post-read mismatch */
} wlc_status_t;

/**
 * @brief Probe the BlueNRG-LP, check OTP fingerprint, program if missing.
 *
 * Caller responsibilities: SPI1 + GPIOs (PA2 CS, PD4 RST, PB11 IRQ) must
 * already be initialised — re-uses the same `hci_tl_spi_init` /
 * `hci_tl_spi_reset` infrastructure that the BLE stack uses. The function
 * does NOT call `bluetooth_init` or any HCI-stack code. After a successful
 * return, the chip is in normal mode and ready for the BLE stack to take
 * over via `bluetooth_init`.
 *
 * @param dtm_buf   Pointer to a buffer holding the full `dtm.bin` (typically
 *                  ~200 KB; we only read offsets 0x1540..0x318f).
 * @param dtm_size  Length of dtm_buf in bytes; must be >= 0x3190.
 * @return          See wlc_status_t.
 */
wlc_status_t WLC_CheckAndProgram(const uint8_t *dtm_buf, size_t dtm_size);

/**
 * @brief One-line status string for logging. Always non-NULL.
 */
const char *WLC_StatusToString(wlc_status_t s);

#ifdef __cplusplus
}
#endif
#endif /* WLC_H */

/**
  ******************************************************************************
  * @file    wlc.c
  * @brief   BlueNRG-LP OTP programmer (Wireless LE Controller) — port of the
  *          reverse-engineered code from ST's `SensorTile.boxPRO.bin`.
  *
  * Build-gated on STBOX1_ENABLE_WLC (default 0). When 0, the public API
  * compiles to a stub returning WLC_STATUS_DISABLED so a non-WLC build
  * stays byte-identical.
  *
  * Status: untested on real hardware. The protocol is reconstructed from
  * disassembly; one-by-one calibration against an actual unprogrammed box
  * is required before this can replace the manual DFU+dtm.bin procedure.
  ******************************************************************************
  */

#include "wlc.h"
#include "stbox1_config.h"

#if STBOX1_ENABLE_WLC

#include "ble_spi.h"
#include "hci_tl_interface.h"
#include "main.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ---------- Wire-protocol opcodes (FUN_08068916, FUN_080689c2 etc.) ------- */
#define WLC_OP_WRITE_MEM     0xFAU  /* 0xFA + 4B BE addr + data */
#define WLC_OP_READ_CMD      0x02U  /* opcode-byte read (mode/version/status) */
#define WLC_OP_READ_ADDR     0x05U  /* 0x05 + 4B BE addr + nbytes -> rx */
#define WLC_OP_READ_INFO     0x00U  /* opcode 0 → 14-byte vega_chip_info */

/* ---------- BlueNRG-LP register addresses (decoded from DAT_*) ------------ */
#define WLC_REG_BOOT          0x2001C218U   /* DAT_08069338 */
#define WLC_REG_CPU_RESET     0x2001C014U   /* DAT_08069320 */
#define WLC_REG_BOOT_MODE     0x2001C00CU   /* DAT_08069310 */
#define WLC_REG_ILOAD         0x2001C176U   /* DAT_0806936C */
#define WLC_REG_CHIP_EXTRA    0x2001C002U   /* DAT_08069348 */

/* OTP-programming control registers (16-bit reg-write framing) */
#define WLC_OTP_REG_CTRL_AB   0x0146U       /* write 0x00C0 */
#define WLC_OTP_REG_CTRL_C    0x0145U       /* write 0x40 to enable, 0x01 to kick */
#define WLC_OTP_REG_LEN       0x014CU       /* per-page length */
#define WLC_OTP_REG_STATUS    0x0012U       /* write 0x10 to clear */

/* ---------- Memory layout in the BlueNRG-LP --------------------------------*/
#define WLC_RAM_BASE          0x00050000U
#define WLC_OTP_BASE          0x00051000U
#define WLC_OTP_PAGE_SIZE     0x1000U       /* 4 KB pages */
#define WLC_RAM_FW_SIZE       0x0C50U       /* 3152 B bootloader-RAM image */
#define WLC_CFG_SIZE          0x0200U       /* 512 B config */
#define WLC_PATCH_SIZE        0x0DA0U       /* 3488 B patch */
#define WLC_TRAILER_SIZE      8U
#define WLC_BOTH_PAYLOAD_SIZE (WLC_CFG_SIZE + WLC_PATCH_SIZE + WLC_TRAILER_SIZE)
                                            /* 0xFA8 = 4008 bytes when both
                                               cfg+patch must be flashed */
#define WLC_OTP_AVAILABLE     0x3F40U       /* hard chip ceiling (FUN_08068f50
                                               line 90: `if (0x3f40 < uVar4)`).
                                               Distinct from the staging size. */
#define WLC_STAGING_SIZE      0x1000U       /* 4 KB — large enough for any of
                                               the three cases (cfg-only,
                                               patch-only, both); rounded up
                                               to one OTP page so the page
                                               loop runs exactly once. */

/* dtm.bin offsets (FUN_08068bc0 + FUN_08068f50 baseline+0x1540 etc.) */
#define WLC_DTM_RAM_FW_OFF    0x1540U
#define WLC_DTM_CFG_OFF       0x21F0U       /* RAM_FW_OFF + RAM_FW_SIZE */
#define WLC_DTM_PATCH_OFF     0x23F0U       /* CFG_OFF   + CFG_SIZE   */
#define WLC_DTM_MIN_SIZE      (WLC_DTM_PATCH_OFF + WLC_PATCH_SIZE)

/* Identity targets (FUN_08068f50 0x50e0 / 0x3657 / cut 5) */
#define WLC_TARGET_CUT_ID     0x05U
#define WLC_TARGET_CFG_ID     0x50E0U
#define WLC_TARGET_PATCH_ID   0x3657U

/* Pre-formed 8-byte trailer that goes after CFG+PATCH in RAM
   (DAT_0806945C = 0xFFFF00A7 little-endian, DAT_08069460 = 0x0000FF58) */
static const uint8_t WLC_TRAILER[WLC_TRAILER_SIZE] = {
  0xA7, 0x00, 0xFF, 0xFF,
  0x58, 0xFF, 0x00, 0x00,
};

/* ---------- Tunables ------------------------------------------------------ */
#define WLC_CHUNK_SIZE        0x80U        /* 128 B max per SPI burst (FUN_08068b18/b78) */
#define WLC_RAM_VERIFY_RETRY  3U           /* read-back retries (FUN_08068bc0) */
#define WLC_OTP_POLL_MAX      400U         /* × 50 ms = 20 s per page (FUN_08068c94) */
#define WLC_PRE_OTP_DELAY_MS  300U
#define WLC_PER_PAGE_DELAY_MS 10U
#define WLC_REG_WRITE_DELAY   100U         /* HAL_Delay(100) in FUN_08068a10 */
#define WLC_SYNC_DELAY        50U          /* FUN_08068aac, FUN_080688ac */
#define WLC_RST_LOW_MS        5U
#define WLC_RST_BOOT_MS       150U

/* GPIO pin the original FW polled for OTP-write-done. The reverse-engineered
   code reads bit 3 of GPIOE->IDR (DAT_08069304 = 0x42021000, mask 0x08).
   That maps to PE3. We expose a #define so a future board variant can
   override it. */
#ifndef WLC_OTP_DONE_PORT
#define WLC_OTP_DONE_PORT     GPIOE
#define WLC_OTP_DONE_PIN      GPIO_PIN_3
#endif

/* CS / RST helpers — we drive the same pins hci_tl_interface.c uses. */
static inline void wlc_cs_low(void)  { HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_RESET); }
static inline void wlc_cs_high(void) { HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET); }

/* Logger — routed to the SD error log when available, silently dropped
   otherwise (during pre-RTOS calls before fx_thread is up). */
static void wlc_log(const char *msg)
{
  ErrorLog_Write(msg);
}

static void wlc_logf(const char *fmt, ...)
{
  char buf[80];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  ErrorLog_Write(buf);
}

/* ---------- Layer 1: raw SPI byte exchange -------------------------------- */

/* Send a packet (buf[0..len-1]) over SPI with CS toggled. Discards rx. */
static int wlc_spi_send(const uint8_t *buf, uint16_t len)
{
  static uint8_t scratch[256];
  uint16_t off = 0;
  while (off < len) {
    uint16_t chunk = (uint16_t)((len - off) > sizeof(scratch) ? sizeof(scratch) : (len - off));
    wlc_cs_low();
    int rc = BleSpi_SendRecv((uint8_t *)(buf + off), scratch, chunk);
    wlc_cs_high();
    if (rc != 0) return -1;
    off = (uint16_t)(off + chunk);
  }
  return 0;
}

/* Send opcode byte + read N bytes back. CS held low across both phases. */
static int wlc_spi_cmd_recv(uint8_t opcode, uint8_t *rxbuf, uint16_t len)
{
  uint8_t tx_dummy = 0x00;
  uint8_t txop = opcode;
  uint8_t rx_unused;

  wlc_cs_low();
  int rc = BleSpi_SendRecv(&txop, &rx_unused, 1);
  if (rc == 0) {
    for (uint16_t i = 0; i < len; i++) {
      rc = BleSpi_SendRecv(&tx_dummy, &rxbuf[i], 1);
      if (rc != 0) break;
    }
  }
  wlc_cs_high();
  return rc;
}

/* ---------- Layer 2: WLC primitives (port of FUN_080689c2 etc.) ----------- */

/* Generic memory write: 0xFA + 4-byte BE addr + data (FUN_080689c2). */
static int wlc_write_mem(uint32_t addr, const uint8_t *data, uint16_t len)
{
  uint8_t hdr[5];
  hdr[0] = WLC_OP_WRITE_MEM;
  hdr[1] = (uint8_t)((addr >> 24) & 0xFF);
  hdr[2] = (uint8_t)((addr >> 16) & 0xFF);
  hdr[3] = (uint8_t)((addr >>  8) & 0xFF);
  hdr[4] = (uint8_t)( addr        & 0xFF);

  /* CS held low across header + payload — single transaction. */
  static uint8_t scratch[5 + WLC_CHUNK_SIZE];
  if (len > WLC_CHUNK_SIZE) return -1;
  memcpy(scratch, hdr, 5);
  if (data && len) memcpy(scratch + 5, data, len);
  return wlc_spi_send(scratch, (uint16_t)(5 + len));
}

/* OTP register write: 16-bit BE reg + data (FUN_08068a10). */
static int wlc_write_reg(uint16_t reg, const uint8_t *data, uint16_t len)
{
  static uint8_t scratch[2 + 16];
  if (len > 16) return -1;
  scratch[0] = (uint8_t)((reg >> 8) & 0xFF);
  scratch[1] = (uint8_t)( reg       & 0xFF);
  memcpy(scratch + 2, data, len);
  int rc = wlc_spi_send(scratch, (uint16_t)(2 + len));
  if (rc == 0) HAL_Delay(WLC_REG_WRITE_DELAY);  /* FUN_08068a10 settle */
  return rc;
}

/* Cmd-byte read: opcode + N bytes (FUN_08068a90, FUN_08068916 op=2). */
static int wlc_read_cmd(uint8_t opcode, uint8_t *rxbuf, uint16_t len)
{
  return wlc_spi_cmd_recv(opcode, rxbuf, len);
}

/* Address-read: 0x05 + 4-byte BE addr + N bytes returned (FUN_08068a5e). */
static int wlc_read_addr(uint32_t addr, uint8_t *rxbuf, uint16_t len)
{
  uint8_t hdr[5];
  uint8_t rx_unused[5];
  hdr[0] = WLC_OP_READ_ADDR;
  hdr[1] = (uint8_t)((addr >> 24) & 0xFF);
  hdr[2] = (uint8_t)((addr >> 16) & 0xFF);
  hdr[3] = (uint8_t)((addr >>  8) & 0xFF);
  hdr[4] = (uint8_t)( addr        & 0xFF);

  wlc_cs_low();
  int rc = BleSpi_SendRecv(hdr, rx_unused, 5);
  if (rc == 0) {
    uint8_t tx_dummy = 0x00;
    for (uint16_t i = 0; i < len; i++) {
      rc = BleSpi_SendRecv(&tx_dummy, &rxbuf[i], 1);
      if (rc != 0) break;
    }
  }
  wlc_cs_high();
  return rc;
}

/* Sync byte to wake/idle the chip (FUN_08068aac).
   Original sends 1 byte to BOOT_MODE_REG, waits 50 ms, runs sync trailer. */
static int wlc_sync(void)
{
  uint8_t one = 0x01;
  int rc = wlc_write_mem(WLC_REG_BOOT_MODE, &one, 1);
  HAL_Delay(WLC_SYNC_DELAY);
  return rc;
}

/* ---------- Layer 3: chunked I/O (FUN_08068b18 / FUN_08068b78) ------------ */

static int wlc_write_chunked(const uint8_t *src, uint16_t len, uint32_t addr)
{
  uint16_t off = 0;
  while (off < len) {
    uint16_t chunk = (uint16_t)((len - off) > WLC_CHUNK_SIZE ? WLC_CHUNK_SIZE : (len - off));
    if (wlc_write_mem(addr + off, src + off, chunk) != 0) return -1;
    off = (uint16_t)(off + chunk);
  }
  return 0;
}

static int wlc_read_chunked(uint8_t *dst, uint16_t len, uint32_t addr)
{
  uint16_t off = 0;
  while (off < len) {
    uint16_t chunk = (uint16_t)((len - off) > WLC_CHUNK_SIZE ? WLC_CHUNK_SIZE : (len - off));
    if (wlc_read_addr(addr + off, dst + off, chunk) != 0) return -1;
    off = (uint16_t)(off + chunk);
  }
  return 0;
}

/* ---------- Layer 4: WLC algorithm operations ----------------------------- */

/* Read full vega_chip_info structure (14 bytes, FUN_08068e94).
   Layout:
     u16 chip_id; u8 chip_rev; u8 customer_id;
     u16 project_id; u16 svn; u16 cfg_id; u16 pe; u16 patch_id;
   Plus a 1-byte "extra" status read from WLC_REG_CHIP_EXTRA. */
typedef struct {
  uint16_t chip_id;
  uint8_t  chip_rev;
  uint8_t  customer_id;
  uint16_t project_id;
  uint16_t svn;
  uint16_t cfg_id;
  uint16_t pe;
  uint16_t patch_id;
  uint8_t  extra;
} vega_chip_info_t;

static int wlc_read_chip_info(vega_chip_info_t *info)
{
  uint8_t buf[14];
  if (wlc_read_cmd(WLC_OP_READ_INFO, buf, 14) != 0) return -1;
  info->chip_id      = (uint16_t)(buf[0] | (buf[1] << 8));
  info->chip_rev     = buf[2];
  info->customer_id  = buf[3];
  info->project_id   = (uint16_t)(buf[4] | (buf[5] << 8));
  info->svn          = (uint16_t)(buf[6] | (buf[7] << 8));
  info->cfg_id       = (uint16_t)(buf[8] | (buf[9] << 8));
  info->pe           = (uint16_t)(buf[10] | (buf[11] << 8));
  info->patch_id     = (uint16_t)(buf[12] | (buf[13] << 8));
  if (wlc_read_addr(WLC_REG_CHIP_EXTRA, &info->extra, 1) != 0) return -1;
  return 0;
}

/* Read operation mode (1 byte). Must == 1 to be in iload bootloader. */
static int wlc_read_op_mode(uint8_t *mode)
{
  return wlc_read_cmd(0x0E, mode, 1);
}

/* Load the RAM bootloader image (0xc50 bytes from dtm.bin offset 0x1540)
   to chip RAM @ 0x50000, with up to 3 read-back-verify retries. (FUN_08068bc0) */
static int wlc_load_ram_fw(const uint8_t *dtm_buf)
{
  static uint8_t verify[WLC_RAM_FW_SIZE];
  const uint8_t *fw = dtm_buf + WLC_DTM_RAM_FW_OFF;

  /* CPU + boot-mode prep */
  uint8_t v;
  v = 0x01; if (wlc_write_mem(WLC_REG_CPU_RESET, &v, 1) != 0) return -1;
  v = 0x02; if (wlc_write_mem(WLC_REG_BOOT_MODE, &v, 1) != 0) return -1;

  for (uint8_t retry = 0; retry < WLC_RAM_VERIFY_RETRY; retry++) {
    if (wlc_write_chunked(fw, WLC_RAM_FW_SIZE, WLC_RAM_BASE) != 0) {
      wlc_log("[WLC] write_chunked RAM failed");
      return -1;
    }
    if (wlc_read_chunked(verify, WLC_RAM_FW_SIZE, WLC_RAM_BASE) != 0) {
      wlc_log("[WLC] read_chunked RAM failed");
      return -1;
    }
    if (memcmp(fw, verify, WLC_RAM_FW_SIZE) == 0) {
      wlc_log("[WLC] RAM FW Loaded Successfully");
      return 0;
    }
    wlc_logf("[WLC] RAM verify mismatch attempt %u", (unsigned)(retry + 1));
  }
  return -1;
}

/* Stage one OTP page via RAM, kick the program, poll PE3 for completion.
   `chunk` <= 0x1000 (4 KB). (FUN_08068c94 inner body.) */
static int wlc_otp_program_page(const uint8_t *src, uint16_t chunk, uint32_t otp_dest)
{
  static uint8_t verify[WLC_OTP_PAGE_SIZE];
  uint8_t lenbuf[2];

  /* 1. Tell chip how many bytes are in this page */
  lenbuf[0] = (uint8_t)( chunk       & 0xFF);
  lenbuf[1] = (uint8_t)((chunk >> 8) & 0xFF);
  if (wlc_write_reg(WLC_OTP_REG_LEN, lenbuf, 2) != 0) {
    wlc_log("[WLC] reg LEN write failed");
    return -1;
  }

  /* 2. Stage the chunk into chip RAM @ otp_dest with verify retry */
  for (uint8_t retry = 0; retry < WLC_RAM_VERIFY_RETRY; retry++) {
    if (wlc_write_chunked(src, chunk, otp_dest) != 0) {
      wlc_log("[WLC] OTP stage write failed");
      return -1;
    }
    if (wlc_read_chunked(verify, chunk, otp_dest) != 0) {
      wlc_log("[WLC] OTP stage read failed");
      return -1;
    }
    if (memcmp(src, verify, chunk) == 0) goto staged_ok;
  }
  wlc_log("[WLC] OTP stage verify failed (3 retries)");
  return -1;

staged_ok:
  /* 3. Kick programming via reg 0x145 = 0x01 */
  uint8_t kick = 0x01;
  if (wlc_write_reg(WLC_OTP_REG_CTRL_C, &kick, 1) != 0) {
    wlc_log("[WLC] OTP kick failed");
    return -1;
  }

  /* 4. Poll PE3 for done. Bit clear → still busy. */
  for (uint16_t poll = 0; poll < WLC_OTP_POLL_MAX; poll++) {
    if (HAL_GPIO_ReadPin(WLC_OTP_DONE_PORT, WLC_OTP_DONE_PIN) == GPIO_PIN_RESET) {
      goto programmed;
    }
    HAL_Delay(50);
  }
  wlc_log("[WLC] OTP page timeout");
  return -1;

programmed:
  /* 5. Reset status bit via reg 0x12 = 0x10 */
  HAL_Delay(WLC_PER_PAGE_DELAY_MS);
  uint8_t status_clear = 0x10;
  if (wlc_write_reg(WLC_OTP_REG_STATUS, &status_clear, 1) != 0) {
    wlc_log("[WLC] OTP status clear failed");
    /* non-fatal — continue */
  }
  return 0;
}

/* Walk the staged OTP image one 4 KB page at a time. (FUN_08068c94 outer.) */
static int wlc_otp_program(const uint8_t *src, uint32_t total, uint32_t otp_base)
{
  uint32_t off = 0;
  while (total > 0) {
    HAL_Delay(WLC_PER_PAGE_DELAY_MS);
    uint16_t chunk = (total > WLC_OTP_PAGE_SIZE)
      ? WLC_OTP_PAGE_SIZE : (uint16_t)total;
    if (wlc_otp_program_page(src + off, chunk, otp_base + off) != 0) {
      return -1;
    }
    off   += chunk;
    total -= chunk;
  }
  return 0;
}

/* Cleanup: deassert boot, sync. (FUN_08068e54) */
static void wlc_cleanup(void)
{
  uint8_t zero = 0x00;
  (void)wlc_write_mem(WLC_REG_BOOT, &zero, 1);
  (void)wlc_sync();
}

/* ---------- Public entry point: full algorithm (FUN_08068f50) ------------- */

wlc_status_t WLC_CheckAndProgram(const uint8_t *dtm_buf, size_t dtm_size)
{
  if (!dtm_buf || dtm_size < WLC_DTM_MIN_SIZE) {
    wlc_log("[WLC] dtm.bin too small / null");
    return WLC_STATUS_BAD_DTM;
  }

  /* SPI1 + GPIOs already up via hci_tl_spi_init from caller (or BleSpi_Init
     idempotent reuse). Make sure the chip is freshly reset so we're in
     iload mode. */
  if (BleSpi_Init() != 0) {
    wlc_log("[WLC] SPI init failed");
    return WLC_STATUS_PROBE_FAILED;
  }
  hci_tl_spi_reset();

  /* 1. Probe: read operation mode (must be 1 = iload bootloader) */
  uint8_t mode = 0;
  if (wlc_read_op_mode(&mode) != 0) {
    wlc_log("[WLC] op-mode read failed (chip dead?)");
    return WLC_STATUS_PROBE_FAILED;
  }
  wlc_logf("[WLC] op_mode=0x%02X", (unsigned)mode);
  if (mode != 1) {
    /* Chip is in normal mode → already programmed and running BLE FW.
       This is the WLC's "OTP not required" success branch. */
    wlc_logf("[WLC] mode!=1 (chip already in BLE mode), skipping");
    return WLC_STATUS_NOT_REQUIRED;
  }

  if (wlc_sync() != 0) return WLC_STATUS_PROBE_FAILED;

  /* 2. Read chip info, decide what to flash */
  vega_chip_info_t info = {0};
  if (wlc_read_chip_info(&info) != 0) {
    wlc_log("[WLC] chip-info read failed");
    return WLC_STATUS_PROBE_FAILED;
  }
  wlc_logf("[WLC] cut=%02X cfg=%04X patch=%04X",
           (unsigned)info.chip_rev, (unsigned)info.cfg_id, (unsigned)info.patch_id);

  if (info.chip_rev != WLC_TARGET_CUT_ID) {
    wlc_logf("[WLC] cut mismatch (have %02X expect %02X)",
             (unsigned)info.chip_rev, (unsigned)WLC_TARGET_CUT_ID);
    return WLC_STATUS_PROBE_FAILED;
  }

  uint8_t cfg_mismatch   = (info.cfg_id   != WLC_TARGET_CFG_ID)   ? 1U : 0U;
  uint8_t patch_mismatch = (info.patch_id != WLC_TARGET_PATCH_ID) ? 1U : 0U;
  if (!cfg_mismatch && !patch_mismatch) {
    wlc_log("[WLC] flashing OK (already programmed)");
    return WLC_STATUS_NOT_REQUIRED;
  }

  /* 3. Stage the right blob into a working buffer (FUN_08068f50 lines 74-87).
        Three cases:
          both    → cfg(0x200) + patch(0xda0) + trailer(8) = 0xfa8 total
          patch   → patch(0xda0)                            = 0xda0 total
          cfg     → cfg(0x200)                              = 0x200 total
        Trailer in the both-case lands at offset cfg_size+patch_size = 0xfa0,
        right after the patch segment. (The decompiled "iVar3 + 4000" is C
        decimal 4000 = 0xfa0.) */
  static uint8_t staging[WLC_STAGING_SIZE];
  memset(staging, 0, sizeof(staging));
  uint32_t total = 0;
  if (cfg_mismatch && patch_mismatch) {
    memcpy(staging,                                  dtm_buf + WLC_DTM_CFG_OFF,   WLC_CFG_SIZE);
    memcpy(staging + WLC_CFG_SIZE,                   dtm_buf + WLC_DTM_PATCH_OFF, WLC_PATCH_SIZE);
    memcpy(staging + WLC_CFG_SIZE + WLC_PATCH_SIZE,  WLC_TRAILER,                 WLC_TRAILER_SIZE);
    total = WLC_BOTH_PAYLOAD_SIZE;  /* 0xFA8 */
  } else if (patch_mismatch) {
    memcpy(staging, dtm_buf + WLC_DTM_PATCH_OFF, WLC_PATCH_SIZE);
    total = WLC_PATCH_SIZE;
  } else {
    memcpy(staging, dtm_buf + WLC_DTM_CFG_OFF,   WLC_CFG_SIZE);
    total = WLC_CFG_SIZE;
  }
  wlc_logf("[WLC] staging size=%lu", (unsigned long)total);
  if (total > WLC_OTP_AVAILABLE) {
    wlc_log("[WLC] staging too big — fail");
    return WLC_STATUS_PROG_FAILED;
  }

  /* 4. Upload RAM bootloader and boot the chip into RAM */
  if (wlc_load_ram_fw(dtm_buf) != 0) {
    wlc_cleanup();
    return WLC_STATUS_PROG_FAILED;
  }
  uint8_t one = 0x01;
  if (wlc_write_mem(WLC_REG_BOOT, &one, 1) != 0) {
    wlc_cleanup();
    return WLC_STATUS_PROG_FAILED;
  }
  (void)wlc_sync();

  uint8_t version[2] = {0};
  if (wlc_read_cmd(0x06, version, 2) != 0) {
    wlc_cleanup();
    return WLC_STATUS_PROG_FAILED;
  }
  wlc_logf("[WLC] RAM Fw Version 0x%04X",
           (unsigned)(version[0] | (version[1] << 8)));

  /* 5. Disable iload, prime OTP write */
  if (wlc_write_mem(WLC_REG_ILOAD, &one, 1) != 0) {
    wlc_cleanup();
    return WLC_STATUS_PROG_FAILED;
  }
  uint8_t ctrl_ab[2] = {0xC0, 0x00};
  uint8_t ctrl_c     = 0x40;
  if (wlc_write_reg(WLC_OTP_REG_CTRL_AB, ctrl_ab, 2) != 0 ||
      wlc_write_reg(WLC_OTP_REG_CTRL_C,  &ctrl_c, 1) != 0) {
    wlc_cleanup();
    return WLC_STATUS_PROG_FAILED;
  }
  HAL_Delay(WLC_PRE_OTP_DELAY_MS);

  /* 6. Program OTP page-by-page */
  if (wlc_otp_program(staging, total, WLC_OTP_BASE) != 0) {
    wlc_cleanup();
    return WLC_STATUS_PROG_FAILED;
  }

  /* 7. Verify by re-reading chip info */
  uint8_t zero = 0;
  (void)wlc_write_mem(WLC_REG_BOOT, &zero, 1);
  (void)wlc_sync();
  HAL_Delay(WLC_RST_BOOT_MS);

  if (wlc_read_chip_info(&info) != 0) {
    wlc_log("[WLC] post-burn chip-info read failed");
    return WLC_STATUS_VERIFY_FAILED;
  }
  if ((cfg_mismatch   && info.cfg_id   != WLC_TARGET_CFG_ID) ||
      (patch_mismatch && info.patch_id != WLC_TARGET_PATCH_ID)) {
    wlc_logf("[WLC] verify mismatch cfg=%04X patch=%04X",
             (unsigned)info.cfg_id, (unsigned)info.patch_id);
    return WLC_STATUS_VERIFY_FAILED;
  }

  wlc_log("[WLC] flashing OK (programmed)");
  return WLC_STATUS_PROGRAMMED;
}

const char *WLC_StatusToString(wlc_status_t s)
{
  switch (s) {
    case WLC_STATUS_DISABLED:      return "disabled";
    case WLC_STATUS_NOT_REQUIRED:  return "not_required";
    case WLC_STATUS_PROGRAMMED:    return "programmed";
    case WLC_STATUS_PROBE_FAILED:  return "probe_failed";
    case WLC_STATUS_BAD_DTM:       return "bad_dtm";
    case WLC_STATUS_PROG_FAILED:   return "prog_failed";
    case WLC_STATUS_VERIFY_FAILED: return "verify_failed";
    default:                       return "unknown";
  }
}

#else  /* STBOX1_ENABLE_WLC */

wlc_status_t WLC_CheckAndProgram(const uint8_t *dtm_buf, size_t dtm_size)
{
  (void)dtm_buf; (void)dtm_size;
  return WLC_STATUS_DISABLED;
}

const char *WLC_StatusToString(wlc_status_t s)
{
  return (s == WLC_STATUS_DISABLED) ? "disabled" : "unknown";
}

#endif /* STBOX1_ENABLE_WLC */

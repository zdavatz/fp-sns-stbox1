/* USB CDC ACM debug console for SDDataLogFileX (STM32U585).
 *
 * Brings up the OTG_FS peripheral via TinyUSB, exposes a CDC ACM virtual
 * COM port, and overrides __io_putchar so STBOX1_PRINTF / printf land on USB.
 *
 * Architecture:
 *   - UsbCdc_Init() runs from main() AFTER SystemClock_Config (HSI48 must
 *     be on) and BEFORE tx_kernel_enter.
 *   - UsbCdc_ThreadX_Init() is called from App_ThreadX_Init() and spawns
 *     a service thread (priority 13) that loops tud_task() + tx_thread_sleep(1).
 *   - The OTG_FS_IRQHandler in stm32u5xx_it.c forwards to tud_int_handler(0).
 *   - __io_putchar() at the bottom of this file pushes one char to the CDC
 *     TX FIFO non-blockingly. The newlib stdio plumbing in syscalls.c calls
 *     __io_putchar for each byte _write emits, so existing STBOX1_PRINTF
 *     just works.
 */
#include "stbox1_config.h"

#if STBOX1_ENABLE_USB_CDC

#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "tx_api.h"
#include "tusb.h"
#include "stm32u5xx_hal.h"
#include "stm32u5xx_hal_rcc_ex.h"
#include "main.h"
#include "usb_cdc.h"
#include "SensorTileBoxPro.h"  /* BSP_LED_On / Off / LED_RED */

/* ---------------------------------------------------------------------------
 * USB peripheral hardware bring-up
 * --------------------------------------------------------------------------*/
static void usb_cdc_hw_init(void) {
  /* Enable VDDUSB power island. STM32U5 puts the USB transceiver behind a
   * separate supply rail — without this, OTG_FS register writes succeed but
   * the line stays floating and no host enumerates. CRUCIAL: HAL_PWREx_*
   * functions write to PWR->SVMCR, which requires the PWR peripheral clock
   * to be enabled. Without __HAL_RCC_PWR_CLK_ENABLE() the write silently
   * has no effect and VDDUSB stays gated — which is exactly the symptom
   * we hit (host sees zero bus events even though tud_rhport_init returns
   * OK). NFC_FTM's HAL_PCD_MspInit guards against this; we must too. */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWREx_EnableVddUSB();

  /* Route HSI48 to the USB / SDMMC / RNG intermediate clock (ICLK).
   * STM32U5 routes all three through one selector — SDMMC and RNG aren't
   * touched here because SDMMC is already running with its own dedicated
   * clock setup in SystemClock_Config / BSP_SD_Init. HSI48 is already ON
   * (RCC_OSCILLATORTYPE_HSI48 in SystemClock_Config). */
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ICLK;
  PeriphClkInit.IclkClockSelection   = RCC_ICLK_CLKSOURCE_HSI48;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

  /* Enable Clock Recovery System (CRS) auto-trim of HSI48 from USB SOF.
   * HSI48 alone is ±1-2 % out of reset which is too loose for USB FS spec
   * (±0.25 % during enumeration). CRS locks HSI48 to the host's 1 kHz USB
   * SOF token so the line rate stays inside spec the moment a host plugs
   * in. Without CRS the host may not see SE0/J/K transitions cleanly and
   * enumeration silently fails. */
  __HAL_RCC_CRS_CLK_ENABLE();
  RCC_CRSInitTypeDef CRSInit = {0};
  CRSInit.Prescaler             = RCC_CRS_SYNC_DIV1;
  CRSInit.Source                = RCC_CRS_SYNC_SOURCE_USB;
  CRSInit.Polarity              = RCC_CRS_SYNC_POLARITY_RISING;
  CRSInit.ReloadValue           = RCC_CRS_RELOADVALUE_DEFAULT;   /* 0xBB7F = 48000-1 */
  CRSInit.ErrorLimitValue       = RCC_CRS_ERRORLIMIT_DEFAULT;
  CRSInit.HSI48CalibrationValue = RCC_CRS_HSI48CALIBRATION_DEFAULT;
  HAL_RCCEx_CRSConfig(&CRSInit);

  /* Enable peripheral clock for OTG_FS. STM32U5: SYSCFG_OTGHSPHYCR is for
   * the HS PHY which we don't use; the FS path lives behind RCC_AHB2ENR1. */
  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

  /* GPIO config — PA11 (DM) and PA12 (DP) on AF10 (USB_OTG_FS). */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_USB;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Arm the OTG_FS interrupt. Priority 13 — between SDMMC1 (14) so a USB
   * event can't preempt an in-flight SD write, and UART4-GPS (6) so a USB
   * burst doesn't drop NMEA bytes. tud_int_handler bound in stm32u5xx_it.c. */
  HAL_NVIC_SetPriority(OTG_FS_IRQn, 13, 0);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);

  /* CRUCIAL: select the FS embedded PHY via GUSBCFG.PHYSEL (bit 6).
   * STM32U585's OTG_FS needs this bit set BEFORE the core reset so the
   * controller knows which PHY interface to bring up; without it the data
   * lines stay floating and no host enumerates — even though our other
   * init steps (clock, VddUSB, GPIO) all succeed. ST's HAL_PCD_Init does
   * this in USB_CoreInit (stm32u5xx_ll_usb.c), but TinyUSB's
   * dwc2_phy_init doesn't, so we do it manually. Set this AFTER the
   * OTG_FS RCC clock is enabled (we did that above) so the register is
   * actually writable. */
  USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_PHYSEL;
}

/* ---------------------------------------------------------------------------
 * TinyUSB device init — exported entry called from main()
 * --------------------------------------------------------------------------*/
bool UsbCdc_Init(void) {
  usb_cdc_hw_init();
  /* Non-deprecated form of tud_init(0). Returns false if the dwc2 core
   * fails its synopsys-id check (= clock/power not enabled). */
  const tusb_rhport_init_t rh_init = {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_FULL,
  };
  return tud_rhport_init(0, &rh_init);
}

/* ---------------------------------------------------------------------------
 * Service thread — drives tud_task() at ~1 kHz
 * --------------------------------------------------------------------------*/
#define USB_THREAD_STACK_SIZE  2048
static TX_THREAD usb_thread;
static UCHAR    *usb_thread_stack = NULL;

/* Capture key OTG_FS register snapshots into a global buffer that
 * fx_thread can later mirror to the SD error log. We deliberately do NOT
 * call ErrorLog_Write from this thread because (a) FileX's media access
 * isn't safe to share between threads without explicit serialisation, and
 * (b) doing so on the BLE+USB build hits the issue #12 second-flush hang
 * that we're trying to debug in the first place.
 *
 * Layout: a 4-line ring of NUL-terminated strings; written by USB thread,
 * drained by fx_thread once per second from app_filex.c. To keep this PR
 * minimal we expose just the variables; the fx_thread mirror loop will
 * land in a follow-up if useful. For now the buffer is mainly inspectable
 * via SWD memory read or a JTAG dump. */
volatile char g_usb_diag_lines[4][200];
volatile uint8_t g_usb_diag_seq = 0;

static void usb_capture_registers(void) {
  volatile uint32_t *otg = (volatile uint32_t *) USB_OTG_FS;
  uint32_t gotgctl = otg[0];
  uint32_t gccfg   = otg[(0x038)/4];
  uint32_t dctl    = otg[(0x804)/4];
  uint32_t dcfg    = otg[(0x800)/4];
  uint32_t gintsts = otg[5];
  uint32_t dsts    = otg[(0x808)/4];

  uint8_t slot = g_usb_diag_seq & 0x3U;
  snprintf((char *) g_usb_diag_lines[slot], sizeof(g_usb_diag_lines[0]),
           "usb#%u gotgctl=%08lX gccfg=%08lX dctl=%08lX dcfg=%08lX gintsts=%08lX dsts=%08lX mounted=%u",
           (unsigned) g_usb_diag_seq,
           (unsigned long) gotgctl, (unsigned long) gccfg,
           (unsigned long) dctl, (unsigned long) dcfg,
           (unsigned long) gintsts, (unsigned long) dsts,
           (unsigned) tud_mounted());
  g_usb_diag_seq++;
}

/* BLE thread exposes this; provide a fallback so usb_cdc.c also builds
 * when BLE is gated off (STBOX1_ENABLE_BLE_SYNC=0 → ble_sync.c's
 * definition is excluded). The fallback is a static const that the
 * heartbeat below can read just like the real one. */
#if STBOX1_ENABLE_BLE_SYNC
extern volatile uint8_t g_ble_probe_status;
#else
static const uint8_t g_ble_probe_status = 0xFFu;  /* BLE disabled */
#endif

/* Request DFU bootloader on next reset. Triggered by writing "DFU\n" to
 * the CDC port. Eliminates the BOOT0-button + slider-toggle dance for the
 * iterate-flash-debug loop:
 *   echo DFU > /dev/ttyACM0
 *   sleep 1 && dfu-util -d 0483:df11 -a 0 \
 *     -s 0x08000000:mass-erase:force:leave -D build/firmware.bin
 *
 * Mechanism: we don't direct-jump from running firmware (dirty OTG_FS /
 * threadx state makes the STM32U5 system bootloader silently fail to
 * enumerate USB DFU). Instead we set a magic value in TAMP->BKP0R, which
 * survives system reset, then NVIC_SystemReset(). main.c checks the magic
 * very early — before HAL_Init — and chains into the bootloader from a
 * clean post-reset state. Bootloader entry at 0x0BF90000 per AN2606. */
static void jump_to_bootloader(void) {
  /* Enable PWR clock + backup-domain write access + RTCAPB clock so we
   * can write TAMP->BKP0R. (PWR clock is already on, but enabling twice
   * is harmless.) */
  RCC->AHB3ENR |= RCC_AHB3ENR_PWREN;
  (void) RCC->AHB3ENR;
  PWR->DBPR  |= PWR_DBPR_DBP;
  RCC->APB3ENR |= RCC_APB3ENR_RTCAPBEN;
  (void) RCC->APB3ENR;

  TAMP->BKP0R = 0xDEADBEEFU;
  __DSB();

  NVIC_SystemReset();
  for (;;) { }  /* unreachable */
}

/* Line-buffered command parser on CDC RX. Drains tud_cdc_read into a
 * small buffer; on \n or \r dispatches one line. Lines longer than
 * USB_CMD_MAX-1 chars are dropped silently. */
#define USB_CMD_MAX  16
static char    usb_cmd_buf[USB_CMD_MAX];
static uint8_t usb_cmd_len = 0;

static void usb_handle_command(const char *line) {
  if (strcmp(line, "DFU") == 0) {
    /* ACK and flush before tearing USB down — host script can confirm
     * the box accepted the command before /dev/ttyACM0 disappears. */
    UsbCdc_WriteString("DFU: entering bootloader\r\n");
    tud_cdc_write_flush();
    tx_thread_sleep(10);  /* let the FIFO drain (~100 ms) */
    jump_to_bootloader();
  }
}

static void usb_poll_rx(void) {
  while (tud_cdc_available()) {
    char c;
    uint32_t got = tud_cdc_read(&c, 1);
    if (got == 0) break;
    if (c == '\r' || c == '\n') {
      if (usb_cmd_len > 0) {
        usb_cmd_buf[usb_cmd_len] = '\0';
        usb_handle_command(usb_cmd_buf);
        usb_cmd_len = 0;
      }
    } else if (usb_cmd_len < USB_CMD_MAX - 1) {
      usb_cmd_buf[usb_cmd_len++] = c;
    } else {
      usb_cmd_len = 0;  /* overflow — drop line */
    }
  }
}

static void usb_thread_entry(ULONG arg) {
  (void) arg;
  /* Red LED tracks tud_mounted() state for instant visual feedback:
   *   red ON  = host has enumerated and CDC is mounted
   *   red OFF = host hasn't seen us yet */
  bool last_mounted = false;
  uint32_t tick = 0;
  bool captured_500ms = false, captured_5s = false, captured_30s = false;
  uint32_t heartbeat_tick = 0;
  for (;;) {
    tud_task();
    usb_poll_rx();
    tud_cdc_write_flush();
    bool mounted_now = tud_mounted();
    if (mounted_now != last_mounted) {
      if (mounted_now) BSP_LED_On(LED_RED);
      else             BSP_LED_Off(LED_RED);
      last_mounted = mounted_now;
    }
    tick++;
    if (!captured_500ms && tick >= 50)   { captured_500ms = true; usb_capture_registers(); }
    if (!captured_5s    && tick >= 500)  { captured_5s    = true; usb_capture_registers(); }
    if (!captured_30s   && tick >= 3000) { captured_30s   = true; usb_capture_registers(); }

    /* Live IRQ-counter indicator on the red LED — replaces the one-shot
     * 8 s diagnostic which proved hard to time visually:
     *   red OFF   = zero OTG_FS IRQs fired (peripheral isn't on the host bus)
     *   red ON    = at least one IRQ has fired AND host has enumerated
     *   red blink = IRQs firing but enumeration not completing
     * The red-on-mounted transition above already handles the "enumerated"
     * case. Here we add a slow blink (250 ms on / 750 ms off) when IRQs
     * are arriving but tud_mounted is still false — that means the chip
     * IS seeing the host but enumeration handshake is failing. */
    extern volatile uint32_t g_otg_fs_irq_count;
    static uint32_t blink_phase = 0;
    if (g_otg_fs_irq_count > 0 && !mounted_now) {
      blink_phase++;
      if (blink_phase ==  1) BSP_LED_On(LED_RED);
      if (blink_phase == 25) BSP_LED_Off(LED_RED);
      if (blink_phase >= 100) blink_phase = 0;
    }

    /* Heartbeat printf every 1 second once the host has mounted us. Gives
     * the field tester (and the host-side usb_console.py reader) something
     * concrete to see, plus snapshots the live diagnostic state:
     *   - tx_time_get()        ThreadX tick (1 tick = 10 ms)
     *   - g_otg_fs_irq_count   number of OTG_FS interrupts since boot
     *   - g_ble_probe_status   BLE bringup phase (see ble_sync.c codes)
     *   - tud_connected/mounted current USB state
     */
    heartbeat_tick++;
    if (mounted_now && heartbeat_tick >= 100) {
      heartbeat_tick = 0;
      /* Hand-formatted, NEWLIB-FREE heartbeat. Earlier printf-based version
       * deadlocked when the BLE thread wedged inside hci_init while holding
       * newlib's reentrant lock — USB thread's printf then blocked on the
       * same lock and the host saw zero bytes despite both LEDs solid. We
       * format into a stack buffer using only inline u32->hex/dec helpers
       * and ship via UsbCdc_WriteString which bypasses _write entirely. */
      char hb[96];
      char *p = hb;
      const char *prefix = "hb t=";
      while (*prefix) *p++ = *prefix++;
      /* Decimal u32 — write digits in reverse then swap. */
      unsigned long v = (unsigned long) tx_time_get();
      char *d0 = p;
      if (v == 0) { *p++ = '0'; }
      else { while (v) { *p++ = (char)('0' + v % 10); v /= 10; } }
      for (char *a = d0, *b = p - 1; a < b; ++a, --b) { char t = *a; *a = *b; *b = t; }
      const char *mid = " otg_irq=";
      while (*mid) *p++ = *mid++;
      v = (unsigned long) g_otg_fs_irq_count;
      d0 = p;
      if (v == 0) { *p++ = '0'; }
      else { while (v) { *p++ = (char)('0' + v % 10); v /= 10; } }
      for (char *a = d0, *b = p - 1; a < b; ++a, --b) { char t = *a; *a = *b; *b = t; }
      const char *mid2 = " ble=0x";
      while (*mid2) *p++ = *mid2++;
      unsigned bs = (unsigned) g_ble_probe_status;
      *p++ = "0123456789ABCDEF"[(bs >> 4) & 0xF];
      *p++ = "0123456789ABCDEF"[bs & 0xF];
      const char *mid3 = " conn=";
      while (*mid3) *p++ = *mid3++;
      *p++ = (char)('0' + (tud_connected() ? 1 : 0));
      const char *mid4 = " mounted=";
      while (*mid4) *p++ = *mid4++;
      *p++ = (char)('0' + (tud_mounted() ? 1 : 0));
      *p++ = '\r'; *p++ = '\n'; *p = '\0';
      UsbCdc_WriteString(hb);
    }
    tx_thread_sleep(1);
  }
}

unsigned int UsbCdc_ThreadX_Init(void *memory_ptr) {
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL *) memory_ptr;

  if (tx_byte_allocate(byte_pool, (VOID **) &usb_thread_stack,
                       USB_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS) {
    return TX_POOL_ERROR;
  }

  return tx_thread_create(&usb_thread, "usb_cdc",
                          usb_thread_entry, 0,
                          usb_thread_stack, USB_THREAD_STACK_SIZE,
                          13, 13,            /* priority + preempt threshold */
                          TX_NO_TIME_SLICE,
                          TX_AUTO_START);
}

/* ---------------------------------------------------------------------------
 * Write API + __io_putchar override
 * --------------------------------------------------------------------------*/
size_t UsbCdc_Write(const void *buf, size_t len) {
  /* Gate on tud_mounted(), not tud_cdc_connected(). The latter requires the
   * host to assert DTR via SET_CONTROL_LINE_STATE — Linux cdc_acm does this
   * on open(), macOS does not (cat/Python both leave DTR low; only screen
   * /picocom set it). Gating on enumeration alone makes the heartbeat work
   * on Mac without DTR ceremony. Drop is still silent so producers in any
   * thread/IRQ never block. */
  if (!tud_mounted()) return 0u;

  /* tud_cdc_write copies into TinyUSB's internal FIFO; if FIFO is full the
   * extra bytes are dropped. Same non-blocking guarantee as above. */
  return tud_cdc_write(buf, (uint32_t) len);
}

size_t UsbCdc_WriteString(const char *s) {
  return UsbCdc_Write(s, strlen(s));
}

/* Strong override of the weak _write in syscalls.c. The newlib stdio plumbing
 * (printf, puts, fwrite, etc.) lands here with one big buffer. We push the
 * whole batch into the CDC TX FIFO in one go — the service thread's
 * tud_cdc_write_flush() drains it to the host.
 *
 * NOT routing through BSP's __io_putchar (defined in
 * Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro.c when USE_COM_LOG=1) because
 * that one targets UART4 which is now owned by the GPS module — printf bytes
 * would corrupt NMEA. By taking _write directly, BSP's __io_putchar becomes
 * unreachable and the linker DCEs it. */
int _write(int file, char *ptr, int len) {
  (void) file;
  return (int) UsbCdc_Write(ptr, (size_t) len);
}

#endif /* STBOX1_ENABLE_USB_CDC */

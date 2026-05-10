/**
  ******************************************************************************
  * @file    ble_sync.c
  * @brief   ThreadX wrapper around the X-CUBE-BLEMGR (STM32_BLE_Manager)
  *          stack. Owns one thread that brings the BlueNRG-LP up via
  *          bluetooth_init(), then services HCI events forever.
  *
  *          With STBOX1_ENABLE_BLE_SYNC = 0 this is a no-op so the legacy
  *          logger image is byte-identical (linker DCEs the entire stack).
  ******************************************************************************
  */

#include "ble_sync.h"
#include "stbox1_config.h"

#if STBOX1_ENABLE_BLE_SYNC

#include "ble_implementation.h"
#include "ble_filesync.h"
#include "hci_tl_interface.h"
#include "app_filex.h"

#include <stdio.h>

extern void hci_user_evt_proc(void);

#define BLE_SYNC_STACK_SIZE   (4 * 1024)
/* BELOW fx_thread (priority 12) so the SD-card logger ALWAYS comes up
   first, regardless of what BLE-bring-up does. Past attempts at higher
   priorities (v41/v48/v49 used priority 11) all failed in the field on
   Peter's reservebox: the BLE thread monopolised the CPU during pre-
   sleep busy-waits inside ble_chip_alive_probe (HAL_Delay(150) +
   500ms IRQ-poll = ~655 ms), then if bluetooth_init() got stuck, the
   logger thread never ran, and the SD card stayed empty — same symptom
   regardless of whether the post-Reset wait was busy-loop (HAL_Delay,
   v48) or yielding (tx_thread_sleep, v49). v50 inverts the priority:
   fx_thread runs first, opens the SD card, starts logging the boot
   marker + status flags, and only then does the kernel let the BLE
   thread (priority 14) run its bring-up. If BLE init hangs, logging
   continues uninterrupted and we get a usable error log. Trade-off:
   BLE init now races against fx_thread for SDMMC bus access, but the
   3.3V-modded box's earlier SDMMC-corruption-from-BLE-noise hypothesis
   is now considered unlikely (the chip is OTP-OK and SDMMC NSPEED is
   already lowered to ~12.5 MHz for headroom). Issue #12. */
#define BLE_SYNC_THREAD_PRIO  14

static TX_THREAD ble_sync_thread;

/* Set by ble_sync_thread_entry once the chip-alive probe + bluetooth_init
   have run. Read by ErrorLog_Open later (when fx_thread auto-START_LOG
   creates the error log) so the post-session log records what happened.
   0xFF = probe hasn't run yet; 0 = chip dead (probe FAIL); 1 = chip alive
   + init_ble_manager OK; 2 = chip alive but init_ble_manager returned
   non-zero. */
volatile uint8_t g_ble_probe_status = 0xFFU;

/* Two-stage liveness probe for the BlueNRG-LP. Returns 1 only when the
   chip both:
     (a) accepted an HCI Reset via SPI (proves the SPI / IRQ-handshake
         layer is alive — chip raised IRQ in <15 ms when we asked to
         send), AND
     (b) sent back a response packet within 500 ms (proves the HCI/HAL
         firmware inside the chip is alive enough to actually parse a
         command and respond — the part that init_ble_manager later
         depends on).

   Returns 0 if either stage fails.

   Why two stages: Peter's 4.5.2026 reserve box (3.3V hardware mod) shows
   the chip ACKs SPI bytes (passes stage a) but `init_ble_manager` then
   hangs the kernel hard enough that fx_thread freezes inside
   fx_file_create on Sens000.csv (last marker captured: "fx:
   COMMAND_START_LOG enter, before fx_file_create Sens", nothing
   afterwards). Adding stage (b) catches the half-dead chip before
   bluetooth_init runs. The user sees "no STBoxSync in scan" instead
   of "logger writes nothing".

   Bounded worst case: 5 ms RST low + 150 ms boot wait + 15 ms TX
   timeout + 500 ms RX timeout = ~670 ms. */
static uint8_t ble_chip_alive_probe(void)
{
  uint8_t hci_reset_cmd[] = {0x01, 0x03, 0x0C, 0x00};

  printf("probe: pre-hci_tl_spi_init\r\n");
  if (hci_tl_spi_init(NULL) != 0)
  {
    printf("probe: hci_tl_spi_init FAILED\r\n");
    return 0U;
  }
  printf("probe: post-hci_tl_spi_init OK\r\n");

  hci_tl_spi_reset();
  printf("probe: post-spi_reset\r\n");

  /* Stage (a): TX. Returns 0 only if chip raised IRQ in <15 ms. */
  int hci_send_rc = hci_tl_spi_send(hci_reset_cmd, sizeof(hci_reset_cmd));
  printf("probe: post-hci_tl_spi_send rc=%d\r\n", hci_send_rc);
  if (hci_send_rc != 0)
  {
    return 0U;
  }

  /* Stage (b): RX. After accepting HCI Reset, a healthy chip raises
     IRQ within ~50 ms with a Command Complete event. Poll the IRQ
     pin (HCI_TL_SPI_IRQ_PIN = PB11) for up to 500 ms, YIELDING via
     tx_thread_sleep(1) between checks — without yields the busy-wait
     starves USB-CDC (priority 13) for the entire 500 ms which breaks
     host enumeration mid-flight. Bisected to the same root cause as
     the 1 s loop in hci_tl_spi_send (issue #12, BISECT-8). */
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < 500U)
  {
    if (HAL_GPIO_ReadPin(HCI_TL_SPI_IRQ_PORT, HCI_TL_SPI_IRQ_PIN) == GPIO_PIN_SET)
    {
      return 1U;
    }
    tx_thread_sleep(1);
  }
  return 0U;
}

/* Run the entire BLE bring-up (chip-alive probe + bluetooth_init +
   EXTI11 callback registration) synchronously in main()'s context,
   BEFORE MX_ThreadX_Init starts the kernel. Reason: on Peter's
   3.3V-modded reservebox the BlueNRG-LP's HCI_Reset SPI activity
   leaves the SDMMC peripheral / SD card in a permanently broken
   state. By doing all of BLE init pre-kernel, fx_thread doesn't
   exist yet — there's no SDMMC operation to corrupt. After this
   call returns, the kernel can start; ble_sync_thread will then
   skip directly to the HCI event-loop, and fx_thread can use SDMMC
   freely (until a phone connects and triggers BLE FileSync SPI
   activity, at which point the same problem may resurface). Issue #12. */
void BleSync_PreKernelInit(void)
{
  uint8_t alive = ble_chip_alive_probe();
  if (!alive)
  {
    g_ble_probe_status = 0U;
    return;  /* ble_sync_thread will see status=0 and park */
  }
  uint8_t init_rc = bluetooth_init();
  if (init_rc != 0U)
  {
    g_ble_probe_status = 2U;
    return;  /* ble_sync_thread will see status=2 and park */
  }
  g_ble_probe_status = 1U;
  init_ble_int_for_blue_nrglp();
  /* From here BLE is up and advertising; EXTI11 is armed. The kernel
     hasn't started yet but hci_tl_lowlevel_isr (the EXTI callback)
     just sets a static flag, so any IRQ that fires between now and
     ble_sync_thread starting is harmlessly queued. */
}

static void ble_sync_thread_entry(ULONG arg)
{
  (void)arg;

  /* IMMEDIATE flush diagnostic — write + flush before EVERY risky step so
     a reset that happens mid-init still leaves a marker on disk. Without
     this, the batched-flush ErrorLog_Write loses everything between the
     last fx_media_flush and the reboot. Issue #12 debugging.
     ALSO mirror to USB CDC printf so the live debug viewer sees the same
     trace — when BLE init kills USB, the *last* line printed pinpoints
     which step did it. */
  ErrorLog_Write("ble: thread entered (will flush)");
  ErrorLog_Flush();
  printf("ble: thread entered\r\n");

  g_ble_probe_status = 0xF0U;  /* thread entered */

  /* Hold off the entire BLE bring-up until host USB enumeration has
     definitely finished. Bisect (issue #12, 2026-05-09) showed that
     even with tx_thread_sleep yields inside hci_tl_spi_send and the
     post-send IRQ poll, the BLE thread starting too early starves
     USB-CDC during the host's first enum descriptor reads (~100 ms
     window starting when D+ pull-up activates at end of UsbCdc_Init).
     A 5 s sleep also lets fx_thread complete fx_media_open +
     WriteFwInfoFile + first sensor flush before any BLE/SPI traffic
     hits the bus (Peter's same rationale on his fork). */
  ErrorLog_Write("ble: pre-init holdoff (5 s) for USB-CDC enum + fx_thread");
  ErrorLog_Flush();
  printf("ble: pre-init holdoff (5 s) for USB-CDC enum\r\n");
  tx_thread_sleep(500);  /* 5 s at 10 ms tick */

  /* Probe BEFORE entering bluetooth_init. */
  g_ble_probe_status = 0xF1U;  /* about to call ble_chip_alive_probe */
  ErrorLog_Write("ble: about to ble_chip_alive_probe");
  ErrorLog_Flush();
  printf("ble: about to ble_chip_alive_probe\r\n");
  uint8_t alive = ble_chip_alive_probe();
  ErrorLog_Write(alive ? "ble: probe returned alive=1"
                        : "ble: probe returned alive=0");
  ErrorLog_Flush();
  printf("ble: probe returned alive=%u\r\n", (unsigned) alive);
  if (!alive)
  {
    g_ble_probe_status = 0U;
    for (;;) {
      ErrorLog_Write("ble: probe FAILED - parked");
      tx_thread_sleep(3000);
    }
  }
  g_ble_probe_status = 0xF2U;  /* probe returned alive=1, about to call bluetooth_init */

  /* Post-HCI-Reset settle delay (yielding) so fx_thread keeps logging
     even if bluetooth_init then hangs. Issue #12 / Peter's reservebox. */
  ErrorLog_Write("ble: probe OK - 2s settle (yielding), then bluetooth_init");
  ErrorLog_Flush();
  printf("ble: probe OK - 2s settle\r\n");
  tx_thread_sleep(200);

  ErrorLog_Write("ble: about to bluetooth_init()");
  ErrorLog_Flush();
  printf("ble: about to bluetooth_init\r\n");

  /* Brings up the SPI-1/HCI link, registers GAP, sets the random address,
     and starts advertising as STBoxSync. */
  uint32_t t0 = tx_time_get();
  uint8_t init_rc = bluetooth_init();
  uint32_t dt = tx_time_get() - t0;
  g_ble_probe_status = 0xF3U;  /* bluetooth_init returned (any rc) */
  {
    char m[80];
    sprintf(m, "ble: bluetooth_init returned rc=%u in %lu ms",
            (unsigned)init_rc, (unsigned long)(dt * 10U));
    ErrorLog_Write(m);
    ErrorLog_Flush();
  }

  if (init_rc != 0U)
  {
    g_ble_probe_status = 2U;
    ErrorLog_Write("ble: init FAIL - thread parking, fx_thread keeps logging");
    for (;;) { tx_thread_sleep(1000); }
  }
  g_ble_probe_status = 0xF4U;  /* about to arm EXTI11 */
  ErrorLog_Write("ble: bluetooth_init OK - arming EXTI11 (factory pattern)");
  ErrorLog_Flush();

  /* Hook EXTI11 → hci_tl_lowlevel_isr. Matches factory firmware (FUN_08031468
     from Ghidra analysis) — EXTI11 stays armed BEFORE hci_init/hci_reset
     and through normal operation. Our middleware's init_ble_manager_ble_stack()
     armed it during init, but our hci_tl_spi_send/receive disabled it for
     the transaction duration. Re-arm now so post-init async events from the
     chip (advertising state changes, connection events, etc.) get processed
     by the ISR — without this, the chip may not radiate even though the
     local advertising commands return SUCCESS. */
  init_ble_int_for_blue_nrglp();
  g_ble_probe_status = 1U;  /* SUCCESS: advertising + IRQ armed */
  ErrorLog_Write("ble: EXTI11 armed - advertising");
  ErrorLog_Flush();

  extern uint8_t set_connectable;
  extern void set_connectable_ble(void);

  for (;;)
  {
    if (hci_event)
    {
      hci_event = 0;
      hci_user_evt_proc();
    }
    if (set_connectable)
    {
      set_connectable_ble();
      set_connectable = 0U;
      ErrorLog_Write("ble: advertising started");
    }
    BleFileSync_Tick();
    tx_thread_sleep(1);
  }
}

UINT BleSync_ThreadX_Init(TX_BYTE_POOL *byte_pool)
{
  CHAR *stack_ptr = NULL;
  UINT  rc;

  rc = tx_byte_allocate(byte_pool, (VOID **)&stack_ptr, BLE_SYNC_STACK_SIZE, TX_NO_WAIT);
  if (rc != TX_SUCCESS)
  {
    return rc;
  }

  return tx_thread_create(&ble_sync_thread,
                          "BLE Sync Thread",
                          ble_sync_thread_entry,
                          0,
                          stack_ptr,
                          BLE_SYNC_STACK_SIZE,
                          BLE_SYNC_THREAD_PRIO,
                          BLE_SYNC_THREAD_PRIO,
                          TX_NO_TIME_SLICE,
                          TX_AUTO_START);
}

#else  /* STBOX1_ENABLE_BLE_SYNC */

UINT BleSync_ThreadX_Init(TX_BYTE_POOL *byte_pool)
{
  (void)byte_pool;
  return TX_SUCCESS;
}

void BleSync_PreKernelInit(void)
{
  /* No-op when BLE is compiled out. */
}

#endif /* STBOX1_ENABLE_BLE_SYNC */

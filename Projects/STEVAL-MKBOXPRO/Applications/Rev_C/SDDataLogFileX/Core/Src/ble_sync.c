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
/* Below the FileX writer (12) and GPS thread (~11) so SD bandwidth and
   GPS line assembly always win. */
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

  if (hci_tl_spi_init(NULL) != 0)
  {
    return 0U;
  }

  hci_tl_spi_reset();

  /* Stage (a): TX. Returns 0 only if chip raised IRQ in <15 ms. */
  if (hci_tl_spi_send(hci_reset_cmd, sizeof(hci_reset_cmd)) != 0)
  {
    return 0U;
  }

  /* Stage (b): RX. After accepting HCI Reset, a healthy chip raises
     IRQ within ~50 ms with a Command Complete event. We poll the IRQ
     pin (HCI_TL_SPI_IRQ_PIN = PB11) for up to 500 ms — busy wait, no
     yield, runs at thread context so SysTick stays live. */
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < 500U)
  {
    if (HAL_GPIO_ReadPin(HCI_TL_SPI_IRQ_PORT, HCI_TL_SPI_IRQ_PIN) == GPIO_PIN_SET)
    {
      return 1U;
    }
  }
  return 0U;
}

static void ble_sync_thread_entry(ULONG arg)
{
  (void)arg;

  /* Probe BEFORE entering bluetooth_init / init_ble_manager. On a dead
     chip the probe fails fast; init_ble_manager would otherwise hang
     the kernel during fx_thread's first tx_thread_sleep yield. */
  uint8_t alive = ble_chip_alive_probe();
  if (!alive)
  {
    g_ble_probe_status = 0U;
    /* Park forever. fx_thread keeps running. SD logger + GPS + battery
       all unaffected; only BLE FileSync is disabled this session. */
    for (;;) {
      ErrorLog_Write("ble: probe FAILED - parked, no init_ble_manager");
      tx_thread_sleep(3000);  /* re-write every 30 s so the log keeps
                                 confirming we're still parked */
    }
  }

  /* Brings up the SPI-1/HCI link, registers GAP, sets the random address,
     and starts advertising as STBoxSync. set_board_name() is called from
     within init_ble_manager(). Non-zero return = the BlueNRG-LP ACK'd
     SPI bytes (probe passed) but the GATT/GAP handshake failed somewhere
     deeper. We must NOT arm the EXTI in that case: a stuck-high IRQ line
     would otherwise storm the NVIC indefinitely and starve fx_thread
     mid-write (the symptom Peter sees as "10 s logging then frozen
     green LED, BLE never advertises"). */
  ErrorLog_Write("ble: probe OK - calling bluetooth_init");
  uint8_t init_rc = bluetooth_init();
  if (init_rc != 0U)
  {
    g_ble_probe_status = 2U;
    char m[64];
    sprintf(m, "ble: bluetooth_init returned rc=%u - parking", (unsigned)init_rc);
    ErrorLog_Write(m);
    /* Park the thread so it never rearms the IRQ. fx_thread continues
       writing the log untouched. */
    for (;;) {
      ErrorLog_Write("ble: still parked after init FAIL");
      tx_thread_sleep(3000);
    }
  }
  g_ble_probe_status = 1U;
  ErrorLog_Write("ble: bluetooth_init OK - arming EXTI11");

  /* Hook EXTI11 → hci_tl_lowlevel_isr — this is what flips `hci_event`
     from the NVIC when the BlueNRG-LP raises its IRQ line. */
  init_ble_int_for_blue_nrglp();

  for (;;)
  {
    if (hci_event)
    {
      hci_event = 0;
      hci_user_evt_proc();
    }
    /* Drives the LIST state machine — kept out of the HCI callback so
       FileX directory walks don't block the event pump. */
    BleFileSync_Tick();
    /* 10 ms is roughly one ThreadX tick — short enough that connection-
       interval timing isn't disturbed, long enough that an idle BLE link
       doesn't burn CPU against the FileX writer. */
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

#endif /* STBOX1_ENABLE_BLE_SYNC */

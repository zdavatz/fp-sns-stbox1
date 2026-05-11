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

static void ble_sync_thread_entry(ULONG arg)
{
  (void)arg;

  /* Diagnostic: delay BLE init by 5 s so fx_thread can complete
     fx_media_open + WriteFwInfoFile + first sensor flush before any
     BLE/SPI traffic hits the bus. */
  tx_thread_sleep(500);   /* 500 ticks = 5 s @ 10 ms/tick */
  ErrorLog_Write("ble: thread woke after 5 s sleep, calling bluetooth_init");

  /* Brings up the SPI-1/HCI link, registers GAP, sets the random address,
     and starts advertising as STBoxSync. set_board_name() is called from
     within init_ble_manager(). */
  uint32_t t0 = tx_time_get();
  uint8_t init_rc = bluetooth_init();
  uint32_t dt = tx_time_get() - t0;
  {
    char m[64];
    sprintf(m, "ble: bluetooth_init returned rc=%u in %lu ms",
            (unsigned)init_rc, (unsigned long)(dt * 10U));
    ErrorLog_Write(m);
  }
  if (init_rc != 0U)
  {
    ErrorLog_Write("ble: init FAIL - thread parking, fx_thread keeps logging");
    for (;;) { tx_thread_sleep(1000); }
  }
  ErrorLog_Write("ble: init ok - skipping EXTI re-arm (v40 pure-polling)");

  /* v40: do NOT call init_ble_int_for_blue_nrglp() — it would enable
     EXTI11 NVIC, immediately triggering the pending ISR (latched edge
     from chip activity during init). The ISR then races on SPI with
     thread context and hangs.
     Instead: pure polling. The main loop drains chip events in thread
     context every iteration. No ISR involved. Same idea as the v36
     in-band drain pattern, just extended to steady-state. */

  extern uint8_t set_connectable;
  extern void set_connectable_ble(void);
  extern int32_t hci_notify_asynch_evt(void *pdata);
  extern int32_t is_data_available(void);

  /* Force initial advertising — the middleware's set_connectable
     transitions only happen on disconnect events. */
  set_connectable = 1U;
  ErrorLog_Write("ble: set_connectable=1, entering main loop");

  uint32_t loop_count = 0;
  uint32_t total_drained = 0;
  uint32_t total_evt_proc = 0;
  uint32_t last_report_tick = tx_time_get();
  for (;;)
  {
    loop_count++;
    if (loop_count == 1U) ErrorLog_Write("ble: main loop iter 1");

    /* Polling drain — replaces ISR-driven event delivery. */
    int drain = 0;
    while (is_data_available() && drain < 16) {
      hci_notify_asynch_evt(NULL);
      drain++;
    }
    total_drained += drain;

    /* v43: in ISR mode, the EXTI handler sets hci_event=1 when events
       arrive, so user code knows "there's something to dispatch".
       In our polling mode the ISR doesn't fire — but events DO get
       queued by hci_notify_asynch_evt above. So we must dispatch
       unconditionally (or set the flag ourselves when we drained
       anything). Without this, all 100K+ drained events were silently
       ignored — the user-facing callbacks (write_request_filecmd,
       attr_mod_request_filedata) never fired. */
    if (drain > 0) {
      hci_event = 1;
    }

    if (hci_event)
    {
      hci_event = 0;
      hci_user_evt_proc();
      total_evt_proc++;
    }

    /* v42: every 2 seconds report how many events the polling drain
       saw and how many got dispatched. Tells us if Mac connect/write
       traffic is actually reaching us. */
    if ((tx_time_get() - last_report_tick) >= 200U) {  /* 200 ticks = 2 s */
      char m[80];
      sprintf(m, "ble: poll stats iter=%lu drained=%lu evt_proc=%lu",
              (unsigned long)loop_count, (unsigned long)total_drained,
              (unsigned long)total_evt_proc);
      ErrorLog_Write(m);
      last_report_tick = tx_time_get();
    }
    if (set_connectable)
    {
      ErrorLog_Write("ble: calling set_connectable_ble");
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

#endif /* STBOX1_ENABLE_BLE_SYNC */

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
  ErrorLog_Write("ble: init ok - re-arming EXTI11 (factory pattern)");

  /* Match factory firmware (FUN_08031468 from Ghidra analysis): EXTI11
     IRQ stays armed BEFORE hci_init/hci_reset and through normal
     operation. Our middleware's init_ble_manager_ble_stack() armed it
     during init, but our hci_tl_spi_send/receive disabled it for the
     transaction duration. Re-arm now so post-init async events from the
     chip (advertising state changes, connection events, etc.) get
     processed by the ISR — without this, the chip may not radiate even
     though the local advertising commands return SUCCESS. */
  init_ble_int_for_blue_nrglp();

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

#endif /* STBOX1_ENABLE_BLE_SYNC */

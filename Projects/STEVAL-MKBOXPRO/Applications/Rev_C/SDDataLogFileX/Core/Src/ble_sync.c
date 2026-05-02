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

extern void hci_user_evt_proc(void);

#define BLE_SYNC_STACK_SIZE   (4 * 1024)
/* Below the FileX writer (12) and GPS thread (~11) so SD bandwidth and
   GPS line assembly always win. */
#define BLE_SYNC_THREAD_PRIO  14

static TX_THREAD ble_sync_thread;

static void ble_sync_thread_entry(ULONG arg)
{
  (void)arg;

  /* Brings up the SPI-1/HCI link, registers GAP, sets the random address,
     and starts advertising as STBoxSync. set_board_name() is called from
     within init_ble_manager(). */
  bluetooth_init();

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

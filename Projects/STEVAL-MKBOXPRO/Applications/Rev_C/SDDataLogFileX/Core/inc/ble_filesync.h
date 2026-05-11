/**
  ******************************************************************************
  * @file    ble_filesync.h
  * @brief   FileSync GATT service — lets the host walk the SD root and
  *          download CSV/log files over BLE.
  *
  * Two characteristics, both under the BlueST features service that
  * STM32_BLE_Manager registers automatically (the manager forces all
  * custom chars into the same parent service):
  *
  *   FileCmd  00000080-0010-11e1-ac36-0002a5d5c51b   write w/o response
  *   FileData 00000040-0010-11e1-ac36-0002a5d5c51b   notify
  *
  * Opcode layout on FileCmd (one byte, optional payload — payload is the
  * filename WITHOUT trailing NUL; length comes from the ATT write itself):
  *   0x01 LIST                     — enumerate root, no payload
  *   0x02 READ   <name>            — stream file body
  *   0x03 DELETE <name>            — drop file, ack with 1-byte status
  *   0x04 STOP_LOG                 — gracefully close the active session;
  *                                   no FileData reply, host re-checks via LIST
  *
  * Replies on FileData (notify):
  *   LIST   : one ASCII line per file `name,size\n`, terminator = single `\n`
  *   READ   : raw file bytes streamed in chunks of up to FILESYNC_NOTIFY_MAX;
  *            host knows total length from LIST and stops when bytes == size.
  *            Errors before the first byte are reported with a 1-byte status
  *            (see status table below).
  *   DELETE : single status byte (ST_OK / ST_NOTFND / ST_IOERR / ST_BUSY).
  *
  * Status bytes:
  *   0x00 OK
  *   0xB0 BUSY      — logging in progress, retry after STOP_LOG
  *   0xE1 NOT_FOUND
  *   0xE2 IO_ERROR
  *   0xE3 BAD_REQUEST
  ******************************************************************************
  */

#ifndef _BLE_FILESYNC_H_
#define _BLE_FILESYNC_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Wired up from ble_implementation.c::ble_init_custom_service(). */
void BleFileSync_Register(void);

/* Called from the BLE thread after each hci_user_evt_proc() tick — drives
   the LIST state machine without blocking the HCI event pump. */
void BleFileSync_Tick(void);

/* Reset the FileSync state machine + notification-enabled flag. Called
   from ble_function.c::disconnection_completed_function so a host that
   drops mid-LIST (or mid-READ) doesn't leave the state stuck in
   ST_LIST_EMIT — which would silently swallow the next OP_LIST from
   the same or a new host after reconnect (write_request_filecmd
   ignores opcodes when state != ST_IDLE). */
void BleFileSync_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* _BLE_FILESYNC_H_ */

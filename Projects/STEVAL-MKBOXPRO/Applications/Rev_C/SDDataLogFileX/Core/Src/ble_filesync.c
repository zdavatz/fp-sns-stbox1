/**
  ******************************************************************************
  * @file    ble_filesync.c
  * @brief   FileSync GATT service — slice 3 implements LIST. READ / DELETE /
  *          STOP_LOG land in slice 4.
  *
  *          The two characteristics live under the BlueST features service
  *          (00000000-0001-11e1-9ab4-0002a5d5c51b) that ble_manager owns —
  *          we only register the chars and let the manager group them in.
  ******************************************************************************
  */

#include <stdio.h>
#include <string.h>
#include "ble_manager.h"
#include "ble_manager_common.h"
#include "ble_filesync.h"
#include "fx_api.h"
#include "app_filex.h"

/* sdio_disk is created/opened by app_filex.c; we share the same handle so a
   LIST during an active logging session doesn't need a second media_open. */
extern FX_MEDIA sdio_disk;

/* ---------- UUIDs --------------------------------------------------------- */
/* COPY_UUID_128 takes bytes high-to-low (uuid_15 → uuid_0). */
#define COPY_FILECMD_CHAR_UUID(uuid_struct)  COPY_UUID_128(uuid_struct, \
  0x00,0x00,0x00,0x80, 0x00,0x10, 0x11,0xe1, 0xac,0x36, 0x00,0x02,0xa5,0xd5,0xc5,0x1b)
#define COPY_FILEDATA_CHAR_UUID(uuid_struct) COPY_UUID_128(uuid_struct, \
  0x00,0x00,0x00,0x40, 0x00,0x10, 0x11,0xe1, 0xac,0x36, 0x00,0x02,0xa5,0xd5,0xc5,0x1b)

/* Notify payload size — set just under DEFAULT_MAX_CHAR_LEN (255) so a single
   row line always fits in one ATT_HANDLE_VALUE_NTF after MTU negotiation. */
#define FILESYNC_NOTIFY_MAX  240

/* ---------- Opcodes ------------------------------------------------------- */
#define OP_LIST       0x01
#define OP_READ       0x02
#define OP_DELETE     0x03
#define OP_STOP_LOG   0x04
#define OP_START_LOG  0x05  /* + 4-byte LE duration in seconds (issue #14) */

/* ---------- Status bytes returned over FileData --------------------------- */
/* For DELETE / STOP_LOG the box answers with a single status byte so the
   GUI can confirm the action took effect. READ uses raw byte streaming —
   the GUI knows the exact length from the LIST step and stops listening
   when it has them all. */
#define ST_OK     0x00
#define ST_BUSY   0xB0  /* logging in progress, retry after STOP_LOG */
#define ST_NOTFND 0xE1
#define ST_IOERR  0xE2
#define ST_BADREQ 0xE3

/* ---------- State machine ------------------------------------------------- */
typedef enum
{
  ST_IDLE = 0,
  ST_LIST_BEGIN,    /* fetch first directory entry */
  ST_LIST_EMIT,     /* a row is sitting in `line` waiting to be notified */
  ST_LIST_NEXT,     /* fetch next directory entry */
  ST_LIST_DONE,     /* send terminator, then back to IDLE */
  ST_READ_OPEN,     /* open the requested file, snapshot size */
  ST_READ_STREAM,   /* read + notify chunks until EOF */
  ST_READ_CLOSE,    /* fx_file_close, back to IDLE */
  ST_DELETE,        /* perform delete, send status byte */
  ST_RESPOND_BUSY,  /* host requested READ/DELETE while logging */
  ST_RESPOND_NOTFND,
  ST_RESPOND_IOERR,
  ST_RESPOND_BADREQ
} filesync_state_t;

static ble_char_object_t  ble_char_file_cmd;
static ble_char_object_t  ble_char_file_data;

static volatile filesync_state_t state = ST_IDLE;
static volatile uint8_t          notifications_enabled = 0;

/* Pending command payload — populated by write_request_filecmd, drained
   by Tick. Volatile because Tick (BLE thread loop) and write_request_cb
   (also BLE thread but inside hci_user_evt_proc) run sequentially but
   the compiler might otherwise re-order across them. */
static volatile char     pending_name[64];

/* READ-stream working state — only valid between ST_READ_OPEN and
   ST_READ_CLOSE. */
static FX_FILE  read_file;
static ULONG    read_remaining;

/* ---------- Forward decls ------------------------------------------------- */
static void attr_mod_request_filedata(void *ble_char_pointer, uint16_t attr_handle, uint16_t offset,
                                      uint8_t data_length, uint8_t *att_data);
static void write_request_filecmd(void *ble_char_pointer, uint16_t attr_handle, uint16_t offset,
                                  uint8_t data_length, uint8_t *att_data);

/* ---------- Public API ---------------------------------------------------- */

void BleFileSync_Register(void)
{
  /* FileCmd — host writes opcodes here (write w/o response). */
  memset(&ble_char_file_cmd, 0, sizeof(ble_char_file_cmd));
  COPY_FILECMD_CHAR_UUID(ble_char_file_cmd.uuid);
  ble_char_file_cmd.char_uuid_type       = UUID_TYPE_128;
  ble_char_file_cmd.char_value_length    = FILESYNC_NOTIFY_MAX;
  ble_char_file_cmd.char_properties      = (uint8_t)(CHAR_PROP_WRITE) | (uint8_t)(CHAR_PROP_WRITE_WITHOUT_RESP);
  ble_char_file_cmd.security_permissions = ATTR_PERMISSION_NONE;
  ble_char_file_cmd.gatt_evt_mask        = GATT_NOTIFY_ATTRIBUTE_WRITE;
  ble_char_file_cmd.enc_key_size         = 16;
  ble_char_file_cmd.is_variable          = 1;
  ble_char_file_cmd.write_request_cb     = write_request_filecmd;
  ble_manager_add_char(&ble_char_file_cmd);

  /* FileData — box notifies file rows / payloads here. */
  memset(&ble_char_file_data, 0, sizeof(ble_char_file_data));
  COPY_FILEDATA_CHAR_UUID(ble_char_file_data.uuid);
  ble_char_file_data.char_uuid_type       = UUID_TYPE_128;
  ble_char_file_data.char_value_length    = FILESYNC_NOTIFY_MAX;
  ble_char_file_data.char_properties      = (uint8_t)(CHAR_PROP_NOTIFY);
  ble_char_file_data.security_permissions = ATTR_PERMISSION_NONE;
  ble_char_file_data.gatt_evt_mask        = GATT_NOTIFY_ATTRIBUTE_WRITE;
  ble_char_file_data.enc_key_size         = 16;
  ble_char_file_data.is_variable          = 1;
  ble_char_file_data.attr_mod_request_cb  = attr_mod_request_filedata;
  ble_manager_add_char(&ble_char_file_data);
}

/* ---------- Callbacks ----------------------------------------------------- */

static void attr_mod_request_filedata(void *ble_char_pointer, uint16_t attr_handle, uint16_t offset,
                                      uint8_t data_length, uint8_t *att_data)
{
  (void)ble_char_pointer; (void)attr_handle; (void)offset;

  if (data_length >= 1)
  {
    /* CCCD low byte: 0x01 = notify enable. Anything with bit-0 set turns
       it on; 0 turns it off. */
    notifications_enabled = (att_data[0] & 0x01U) ? 1U : 0U;
    if (!notifications_enabled)
    {
      /* Host detached its subscription mid-walk — abort cleanly. */
      state = ST_IDLE;
    }
  }
}

/* Copies (and NUL-terminates) the filename argument from a FileCmd write.
   Returns 0 if the input is malformed (empty, too long, embedded NUL). */
static int copy_pending_name(const uint8_t *src, uint8_t src_len)
{
  if (src_len == 0 || src_len >= sizeof(pending_name)) return 0;
  for (uint8_t i = 0; i < src_len; i++)
  {
    char c = (char)src[i];
    if (c == '\0') return 0;
    pending_name[i] = c;
  }
  pending_name[src_len] = '\0';
  return 1;
}

static void write_request_filecmd(void *ble_char_pointer, uint16_t attr_handle, uint16_t offset,
                                  uint8_t data_length, uint8_t *att_data)
{
  (void)ble_char_pointer; (void)attr_handle; (void)offset;

  if (data_length < 1) return;

  /* Reject any new opcode while one is still in flight — keeps the state
     machine simple and prevents a misbehaving host from corrupting an
     in-progress LIST or READ stream. */
  if (state != ST_IDLE && att_data[0] != OP_STOP_LOG) return;

  switch (att_data[0])
  {
    case OP_LIST:
      state = ST_LIST_BEGIN;
      break;

    case OP_READ:
      if (!copy_pending_name(att_data + 1, data_length - 1)) { state = ST_RESPOND_BADREQ; break; }
      if (Ble_IsLoggingActive())                              { state = ST_RESPOND_BUSY;   break; }
      state = ST_READ_OPEN;
      break;

    case OP_DELETE:
      if (!copy_pending_name(att_data + 1, data_length - 1)) { state = ST_RESPOND_BADREQ; break; }
      if (Ble_IsLoggingActive())                              { state = ST_RESPOND_BUSY;   break; }
      state = ST_DELETE;
      break;

    case OP_STOP_LOG:
      /* Side-channel — doesn't go through the FileData reply path. The
         host can poll Ble_IsLoggingActive indirectly by retrying a LIST
         (sizes will stop growing) or by waiting and re-sending READ. */
      Ble_RequestStopLog();
      break;

    case OP_START_LOG:
    {
      /* Issue #14 mode-switch: iPhone GUI sends START_LOG + 4-byte
       * little-endian duration in seconds. Box writes BKP1R = LOG
       * magic + BKP2R = duration, then NVIC_SystemReset → boots into
       * LOG mode → fx_thread auto-starts logging → fx_thread monitors
       * elapsed time → reboots back to BLE mode at expiry.
       *
       * We do all the BKP work synchronously here in the FileCmd
       * write callback. By the time NVIC_SystemReset fires the host
       * will have torn down the BLE connection — fine, since we're
       * about to reboot anyway. No FileData response needed. */
      if (data_length < 5) { state = ST_RESPOND_BADREQ; break; }
      uint32_t duration = ((uint32_t)att_data[1])
                        | ((uint32_t)att_data[2] << 8)
                        | ((uint32_t)att_data[3] << 16)
                        | ((uint32_t)att_data[4] << 24);
      if (duration == 0U || duration > 86400U) {  /* sanity: 1s..1day */
        state = ST_RESPOND_BADREQ;
        break;
      }
      /* Backup-domain write access already enabled in main.c at boot. */
      TAMP->BKP1R = 0x4C4F4720U;  /* 'LOG ' magic */
      TAMP->BKP2R = duration;
      __DSB();
      /* No reply, no graceful BLE teardown — just reset. The reboot
       * itself disconnects the host cleanly. */
      NVIC_SystemReset();
      for (;;) { }  /* unreachable */
    }

    default:
      state = ST_RESPOND_BADREQ;
      break;
  }
}

/* ---------- LIST state machine ------------------------------------------- */

static int notify_line(const char *line, uint8_t len)
{
  if (!notifications_enabled) return -1;

  ble_status_t rc = safe_aci_gatt_update_char_value(&ble_char_file_data, 0, len, (uint8_t *)line);
  /* Caller decides whether to retry on INSUFFICIENT_RESOURCES — we just
     report the rc verbatim. */
  return (rc == BLE_STATUS_SUCCESS) ? 0 : -1;
}

static int notify_terminator(void)
{
  /* Single newline marks end-of-listing. The 0-length notify path was
     considered, but some BLE stacks treat 0-length as a no-op. */
  const char nl = '\n';
  return notify_line(&nl, 1);
}

void BleFileSync_Tick(void)
{
  static char    name[64];
  static char    line[FILESYNC_NOTIFY_MAX];
  static uint8_t line_len;
  UINT           attrs;
  ULONG          size;
  UINT           year, month, day, hour, minute, second;
  UINT           rc;
  int            n;

  if (state == ST_IDLE)        return;
  if (!notifications_enabled)  { state = ST_IDLE; return; }

  switch (state)
  {
    case ST_LIST_BEGIN:
      rc = fx_directory_first_full_entry_find(&sdio_disk, name, &attrs, &size,
                                              &year, &month, &day, &hour, &minute, &second);
      if (rc != FX_SUCCESS)
      {
        state = ST_LIST_DONE;
        break;
      }
      if (attrs & FX_DIRECTORY) { state = ST_LIST_NEXT; break; }

      n = snprintf(line, sizeof(line), "%s,%lu\n", name, (unsigned long)size);
      if (n <= 0 || n > FILESYNC_NOTIFY_MAX) { state = ST_LIST_NEXT; break; }
      line_len = (uint8_t)n;
      state = ST_LIST_EMIT;
      /* fall through — try the send immediately */

    /* fall-through */
    case ST_LIST_EMIT:
      /* On success, advance the directory cursor; on congestion, stay here
         and the next tick retries the same row. */
      if (notify_line(line, line_len) == 0) state = ST_LIST_NEXT;
      break;

    case ST_LIST_NEXT:
      rc = fx_directory_next_full_entry_find(&sdio_disk, name, &attrs, &size,
                                             &year, &month, &day, &hour, &minute, &second);
      if (rc != FX_SUCCESS) { state = ST_LIST_DONE; break; }
      if (attrs & FX_DIRECTORY) break;  /* skip subdir, retry next tick */

      n = snprintf(line, sizeof(line), "%s,%lu\n", name, (unsigned long)size);
      if (n <= 0 || n > FILESYNC_NOTIFY_MAX) break;
      line_len = (uint8_t)n;
      state = ST_LIST_EMIT;
      break;

    case ST_LIST_DONE:
      if (notify_terminator() == 0) state = ST_IDLE;
      break;

    /* ----------------- READ ---------------------------------------------- */
    case ST_READ_OPEN:
      rc = fx_file_open(&sdio_disk, &read_file, (CHAR *)pending_name, FX_OPEN_FOR_READ);
      if (rc != FX_SUCCESS) { state = ST_RESPOND_NOTFND; break; }
      read_remaining = read_file.fx_file_current_file_size;
      state = ST_READ_STREAM;
      /* Empty file: skip straight to close so we don't try a 0-byte
         read that some FileX paths flag as an error. */
      if (read_remaining == 0) state = ST_READ_CLOSE;
      break;

    case ST_READ_STREAM:
    {
      ULONG to_read = (read_remaining < FILESYNC_NOTIFY_MAX) ? read_remaining : FILESYNC_NOTIFY_MAX;
      ULONG actual  = 0;
      rc = fx_file_read(&read_file, line, to_read, &actual);
      if (rc != FX_SUCCESS || actual == 0)
      {
        /* Truncated read mid-stream — close and bail. The host already
           knows the expected total from LIST, so a short stream tells it
           something went wrong. */
        state = ST_READ_CLOSE;
        break;
      }
      if (notify_line(line, (uint8_t)actual) != 0)
      {
        /* Notify congested — push the file cursor back and retry on the
           next tick instead of dropping bytes. */
        ULONG dummy_pos;
        (void)fx_file_relative_seek(&read_file, actual, FX_SEEK_BACK);
        (void)dummy_pos;
        break;
      }
      read_remaining -= actual;
      if (read_remaining == 0) state = ST_READ_CLOSE;
      break;
    }

    case ST_READ_CLOSE:
      (void)fx_file_close(&read_file);
      state = ST_IDLE;
      break;

    /* ----------------- DELETE -------------------------------------------- */
    case ST_DELETE:
    {
      rc = fx_file_delete(&sdio_disk, (CHAR *)pending_name);
      uint8_t status_byte = (rc == FX_SUCCESS)        ? ST_OK
                          : (rc == FX_NOT_FOUND)      ? ST_NOTFND
                          : ST_IOERR;
      if (notify_line((const char *)&status_byte, 1) == 0)
      {
        /* Flush sector caches so a subsequent LIST really sees the file
           gone — without the flush, FAT updates can lag a power-cycle. */
        if (rc == FX_SUCCESS) (void)fx_media_flush(&sdio_disk);
        state = ST_IDLE;
      }
      break;
    }

    /* ----------------- One-byte status responders ------------------------ */
    case ST_RESPOND_BUSY:
    {
      uint8_t b = ST_BUSY;
      if (notify_line((const char *)&b, 1) == 0) state = ST_IDLE;
      break;
    }
    case ST_RESPOND_NOTFND:
    {
      uint8_t b = ST_NOTFND;
      if (notify_line((const char *)&b, 1) == 0) state = ST_IDLE;
      break;
    }
    case ST_RESPOND_IOERR:
    {
      uint8_t b = ST_IOERR;
      if (notify_line((const char *)&b, 1) == 0) state = ST_IDLE;
      break;
    }
    case ST_RESPOND_BADREQ:
    {
      uint8_t b = ST_BADREQ;
      if (notify_line((const char *)&b, 1) == 0) state = ST_IDLE;
      break;
    }

    default:
      state = ST_IDLE;
      break;
  }
}

/**
  ******************************************************************************
  * @file    gps_nmea.c
  * @brief   u-blox MAX-M10S NMEA parser on UART4 @ 38400 baud
  ******************************************************************************
  */

#include "gps_nmea.h"
#include "stbox1_config.h"
#include "SensorTileBoxPro.h"
#include "buzzer.h"
#include "tx_api.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* UART4 handle is provided by the BSP as hcom_uart[COM1] */
extern UART_HandleTypeDef hcom_uart[];
#define GPS_UART (&hcom_uart[COM1])

#define NMEA_LINE_MAX 96

/* UART4 RX byte goes into a ring buffer from the IRQ; the GPS thread drains
   the ring and runs NMEA line assembly + parsing at thread priority. Before
   this split, parse_rmc/parse_gga ran inside HAL_UART_RxCpltCallback at
   NVIC priority 6, which preempted the SDMMC1 IRQ (priority 14). At 10 Hz
   GPS (4000 bytes/s = 4000 UART IRQs/s with variable-length parse work per
   completed sentence) the SD transfers stalled, MessageQueue filled, and
   the sensor thread fell from 100 Hz to ~7.7 Hz — see the 23.4.2026 Ayano
   data for the signature (9371 rows at Δtick=1, then 54473 rows at Δtick=13). */
/* Sized for > 1 poll cycle of 10 Hz UBX output: GGA (~80 B) + RMC (~80 B)
   per cycle = ~160 B / 100 ms. 1 kB gives ~6× headroom so a jittered poll
   cycle (or a brief UART error burst) still fits. */
#define GPS_RX_RING_SIZE 1024
static volatile uint8_t  RxRing[GPS_RX_RING_SIZE];
static volatile uint16_t RxRingHead;   /* IRQ writes */
static volatile uint16_t RxRingTail;   /* GPS thread reads */
static volatile uint32_t RxRingDropped; /* diagnostic counter for ring overflow */

static uint8_t  RxByte;
static char     LineBuf[NMEA_LINE_MAX];
static uint16_t LineLen;

static GPS_Fix_t LatestFix;
static volatile uint8_t FixUpdated;

/* UTC wall-clock from $GNRMC (time + date fields). Populated only on
   valid RMC (status 'A'). Used to seed FileX's FAT clock so Sens/Gps/Bat
   files are stamped with today's actual date instead of the compile date.
   Year is four-digit (2026, not 26). Sub-seconds discarded. */
static volatile uint8_t  WallClockValid;
static uint32_t WallClockYear, WallClockMonth, WallClockDay;
static uint32_t WallClockHour, WallClockMin,   WallClockSec;

/* ------------------------------------------------------------------ */
/* NMEA parser                                                        */
/* ------------------------------------------------------------------ */

/* NMEA checksum: XOR of everything between $ and * */
static uint8_t nmea_checksum_ok(const char *line)
{
  if (line[0] != '$') return 0;
  uint8_t sum = 0;
  const char *p = line + 1;
  while (*p && *p != '*') { sum ^= (uint8_t)*p; p++; }
  if (*p != '*') return 0;
  uint8_t expected = (uint8_t)strtoul(p + 1, NULL, 16);
  return (sum == expected);
}

/* Convert "ddmm.mmmm" + hemisphere ('N'/'S'/'E'/'W') to decimal degrees.
   Returns 0.0 on empty/invalid. */
static double nmea_latlon(const char *val, char hem, uint8_t is_lat)
{
  if (!val || !*val) return 0.0;
  double raw = strtod(val, NULL);
  double deg = is_lat ? (int)(raw / 100.0) : (int)(raw / 100.0);
  double min = raw - deg * 100.0;
  double dec = deg + min / 60.0;
  if (hem == 'S' || hem == 'W') dec = -dec;
  return dec;
}

/* Split NMEA line in-place into field pointers. Returns number of fields.
   After call, the '*' before checksum is NUL-terminated. */
static int nmea_split(char *line, char *fields[], int max_fields)
{
  int n = 0;
  char *p = line;
  if (*p != '$') return 0;
  p++;                     /* skip '$' */
  fields[n++] = p;
  while (*p && *p != '*' && n < max_fields)
  {
    if (*p == ',') { *p = '\0'; fields[n++] = p + 1; }
    p++;
  }
  if (*p == '*') *p = '\0';
  return n;
}

/* Parse $GNRMC: time, status, lat, N/S, lon, E/W, speed(kt), course, date, ... */
static void parse_rmc(char *fields[], int n)
{
  if (n < 10) return;
  const char *utc    = fields[1];
  const char *status = fields[2];
  const char *lat    = fields[3];
  const char *ns     = fields[4];
  const char *lon    = fields[5];
  const char *ew     = fields[6];
  const char *spd    = fields[7];
  const char *cog    = fields[8];
  const char *date   = fields[9];  /* "ddmmyy" */

  if (status[0] != 'A') return;   /* 'V' = void */

  /* Audible "first valid GPS fix" indicator: triple beep, fired once
     per boot. Runs in gps_thread context (priority 11) so HAL_Delay
     inside Buzzer_Beep is safe — the UART IRQ keeps filling RxRing
     while we beep, no NMEA bytes are lost. */
  static uint8_t FixBeepFired = 0;
  if (!FixBeepFired) {
    FixBeepFired = 1;
    Buzzer_FixAcquired();
  }

  /* No tx_interrupt_control needed: both parse_rmc (via GPS_Process) and
     the sole consumers (GPS_GetLatestFix / GPS_GetWallClock) run in the
     same GPS thread context since the IRQ-to-thread split. */
  strncpy(LatestFix.Utc, utc, sizeof(LatestFix.Utc) - 1);
  LatestFix.Utc[sizeof(LatestFix.Utc) - 1] = '\0';
  LatestFix.Lat = nmea_latlon(lat, ns[0], 1);
  LatestFix.Lon = nmea_latlon(lon, ew[0], 0);
  LatestFix.SpeedKmh = (float)(strtod(spd, NULL) * 1.852);
  LatestFix.Course   = (float)strtod(cog, NULL);
  LatestFix.TickMs   = tx_time_get();
  LatestFix.Valid    = 1;
  FixUpdated         = 1;

  /* Parse date + time into wall-clock fields. UTC is "hhmmss.ss",
     date is "ddmmyy". The 21st century assumption below is fine for this
     application (the u-blox MAX-M10S launched in 2022). */
  if (utc && strlen(utc) >= 6 && date && strlen(date) >= 6)
  {
    WallClockHour  = (uint32_t)((utc[0]-'0')*10 + (utc[1]-'0'));
    WallClockMin   = (uint32_t)((utc[2]-'0')*10 + (utc[3]-'0'));
    WallClockSec   = (uint32_t)((utc[4]-'0')*10 + (utc[5]-'0'));
    WallClockDay   = (uint32_t)((date[0]-'0')*10 + (date[1]-'0'));
    WallClockMonth = (uint32_t)((date[2]-'0')*10 + (date[3]-'0'));
    WallClockYear  = (uint32_t)(2000 + (date[4]-'0')*10 + (date[5]-'0'));
    WallClockValid = 1;
  }
}

/* Parse $GNGGA: time, lat, N/S, lon, E/W, quality, num-sat, HDOP, alt, M, ... */
static void parse_gga(char *fields[], int n)
{
  if (n < 10) return;
  const char *quality = fields[6];
  const char *numsat  = fields[7];
  const char *hdop    = fields[8];
  const char *alt     = fields[9];

  LatestFix.Fix    = (uint8_t)atoi(quality);
  LatestFix.NumSat = (uint8_t)atoi(numsat);
  LatestFix.Hdop   = (float)strtod(hdop, NULL);
  LatestFix.Alt    = (float)strtod(alt, NULL);
  FixUpdated       = 1;
}

static void nmea_handle_line(char *line)
{
  if (!nmea_checksum_ok(line)) return;

  char *fields[24];
  int n = nmea_split(line, fields, 24);
  if (n < 1) return;

  const char *tag = fields[0];
  /* u-blox uses $GN... for multi-constellation, $GP... for GPS-only */
  if (strlen(tag) < 5) return;
  const char *type = tag + 2;   /* skip GN/GP/GL/GA etc. */

  if      (strncmp(type, "RMC", 3) == 0) parse_rmc(fields, n);
  else if (strncmp(type, "GGA", 3) == 0) parse_gga(fields, n);
}

/* ------------------------------------------------------------------ */
/* UART4 reception                                                    */
/* ------------------------------------------------------------------ */

/* IRQ-only: drop the byte into the ring buffer. No line assembly, no
   parsing — those run in GPS_Process() at thread priority so UART4 IRQ
   can complete in <10 µs and not preempt the SDMMC1 IRQ. */
void GPS_UART_RxByte(uint8_t byte)
{
  uint16_t next = (uint16_t)((RxRingHead + 1) % GPS_RX_RING_SIZE);
  if (next != RxRingTail)
  {
    RxRing[RxRingHead] = byte;
    RxRingHead = next;
  }
  else
  {
    /* Ring full — thread fell behind. At 10 Hz GPS with only GGA+RMC
       enabled we see ~4 kB/s peak, so a 512-byte ring buffers > 100 ms,
       well above the typical GPS poll cadence. Count drops for diagnostics. */
    RxRingDropped++;
  }
}

/* Drain the ring and feed bytes through the NMEA line-assembly + parser.
   Call from the GPS thread (thread priority), once per poll cycle. */
void GPS_Process(void)
{
  for (;;)
  {
    if (RxRingHead == RxRingTail) return;  /* empty */
    uint8_t b = RxRing[RxRingTail];
    RxRingTail = (uint16_t)((RxRingTail + 1) % GPS_RX_RING_SIZE);

    if (b == '\n' || b == '\r')
    {
      if (LineLen > 0 && LineLen < NMEA_LINE_MAX)
      {
        LineBuf[LineLen] = '\0';
        nmea_handle_line(LineBuf);
      }
      LineLen = 0;
      continue;
    }
    if (b == '$')
    {
      /* new sentence start -> reset buffer */
      LineLen = 0;
    }
    if (LineLen < NMEA_LINE_MAX - 1)
    {
      LineBuf[LineLen++] = (char)b;
    }
    else
    {
      /* overrun: drop the line, wait for next '$' */
      LineLen = 0;
    }
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == UART4)
  {
    GPS_UART_RxByte(RxByte);
    HAL_UART_Receive_IT(GPS_UART, &RxByte, 1);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == UART4)
  {
    /* Clear any pending error flags and re-arm RX. LineLen belongs to
       the thread now — don't touch it from the IRQ; the next '$' in the
       stream will reset it on its own. */
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);
    HAL_UART_Receive_IT(GPS_UART, &RxByte, 1);
  }
}

void GPS_UART_IRQHandler(void)
{
  HAL_UART_IRQHandler(GPS_UART);
}

/* ------------------------------------------------------------------ */
/* UBX config (auto-switch GPS UART1 to GPS_UART_BAUDRATE)            */
/* ------------------------------------------------------------------ */

/* Send a UBX message: sync chars B5 62, class, id, length (LE), payload,
   Fletcher-8 checksum over [class..end-of-payload]. Polling TX. */
static void ubx_send(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len)
{
  uint8_t hdr[6] = { 0xB5, 0x62, cls, id,
                     (uint8_t)(len & 0xFF), (uint8_t)(len >> 8) };
  uint8_t ck[2] = { 0, 0 };

  for (int i = 2; i < 6; i++) { ck[0] += hdr[i]; ck[1] += ck[0]; }
  for (int i = 0; i < len; i++) { ck[0] += payload[i]; ck[1] += ck[0]; }

  HAL_UART_Transmit(GPS_UART, hdr, 6, 100);
  if (len) HAL_UART_Transmit(GPS_UART, (uint8_t *)payload, len, 100);
  HAL_UART_Transmit(GPS_UART, ck, 2, 100);
}

/* Wait for UBX-ACK-ACK (cls=0x05,id=0x01) or ACK-NAK (cls=0x05,id=0x00)
   whose 2-byte payload matches the cls/id we sent. Returns:
     1 = ACK-ACK (command accepted)
     0 = ACK-NAK (command rejected — bad format or value)
    -1 = timeout

   Polling RX: the interrupt-driven RX path is NOT armed yet during
   GPS_Init(), so HAL_UART_Receive can own the UART. NMEA sentences from
   the module are discarded as non-UBX noise (no 0xB5 0x62 prefix). */
static int ubx_wait_ack(uint8_t cls, uint8_t id, uint32_t timeout_ms)
{
  uint32_t deadline = HAL_GetTick() + timeout_ms;
  int     state   = 0;
  uint8_t rx_cls  = 0, rx_id  = 0;
  uint16_t rx_len = 0, rx_idx = 0;
  uint8_t rx_p0   = 0, rx_p1  = 0;

  while ((int32_t)(deadline - HAL_GetTick()) > 0)
  {
    uint8_t b;
    if (HAL_UART_Receive(GPS_UART, &b, 1, 5) != HAL_OK) continue;

    switch (state)
    {
      case 0: if (b == 0xB5) state = 1;                               break;
      case 1: state = (b == 0x62) ? 2 : ((b == 0xB5) ? 1 : 0);        break;
      case 2: rx_cls = b; state = 3;                                  break;
      case 3: rx_id  = b; state = 4;                                  break;
      case 4: rx_len = b; state = 5;                                  break;
      case 5: rx_len |= ((uint16_t)b << 8); rx_idx = 0; state = 6;    break;
      case 6:
        if      (rx_idx == 0) rx_p0 = b;
        else if (rx_idx == 1) rx_p1 = b;
        rx_idx++;
        if (rx_idx >= rx_len) state = 7;
        break;
      case 7: state = 8;                                              break;  /* CK_A */
      case 8:
        /* Full UBX frame received. If it's an ACK for our cls/id, return. */
        if (rx_cls == 0x05 && rx_p0 == cls && rx_p1 == id)
          return (rx_id == 0x01) ? 1 : 0;
        state = 0;                                                    break;
    }
  }
  return -1;
}

/* Send a UBX command and block for ACK. Retries up to `retries` times on
   NAK or timeout. Returns same codes as ubx_wait_ack for the last attempt. */
static int ubx_send_retry(uint8_t cls, uint8_t id, const uint8_t *payload,
                          uint16_t len, uint32_t timeout_ms, int retries)
{
  int res = -1;
  for (int attempt = 0; attempt <= retries; attempt++)
  {
    ubx_send(cls, id, payload, len);
    res = ubx_wait_ack(cls, id, timeout_ms);
    if (res == 1) return 1;
    HAL_Delay(50);
  }
  return res;
}

/* ------------------------------------------------------------------ */
/* GPS_Init status log — filled in during GPS_Init(), read later by    */
/* COMMAND_START_LOG and written to the error log so post-boot we can  */
/* verify the module accepted the requested fix rate.                  */
/* ------------------------------------------------------------------ */

#define GPS_INIT_LOG_MAX 160
static char GpsInitLog[GPS_INIT_LOG_MAX];

static void gps_log_append(const char *s)
{
  size_t cur = strlen(GpsInitLog);
  size_t cap = GPS_INIT_LOG_MAX - cur - 1;
  if (cap == 0) return;
  strncat(GpsInitLog, s, cap);
}

static const char *ack_str(int r)
{
  return (r == 1) ? "OK" : (r == 0 ? "NAK" : "TO");
}

const char *GPS_GetInitLog(void)
{
  return GpsInitLog;
}

/* UBX-CFG-PRT for UART1: 8N1, NMEA only out, UBX+NMEA in, given baudrate. */
static void gps_cfg_port_uart1(uint32_t baudrate)
{
  uint8_t p[20] = {0};
  p[0]  = 0x01;                           /* portID = UART1 */
  /* p[1] reserved, p[2..3] txReady = 0 */
  p[4]  = 0xC0; p[5] = 0x08;              /* mode = 0x000008C0 (8N1) */
  p[8]  = (uint8_t)(baudrate      );
  p[9]  = (uint8_t)(baudrate >>  8);
  p[10] = (uint8_t)(baudrate >> 16);
  p[11] = (uint8_t)(baudrate >> 24);
  p[12] = 0x03;                           /* inProtoMask  = UBX + NMEA */
  p[14] = 0x02;                           /* outProtoMask = NMEA */
  ubx_send(0x06, 0x00, p, sizeof(p));     /* CFG-PRT */
}

/* UBX-CFG-RATE, UBX-CFG-MSG, UBX-CFG-CFG payloads are built inline in
   GPS_Init() and dispatched via ubx_send_retry() so the ACK status of
   each command is captured in GpsInitLog. Keeping them inline avoids
   drift between the payload layout and the ACK-retry path. */

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

void GPS_Init(void)
{
  memset(&LatestFix, 0, sizeof(LatestFix));
  FixUpdated = 0;
  LineLen = 0;
  GpsInitLog[0] = '\0';

  char buf[64];

  /* BSP_COM_Init configures GPIO AF + enables UART4 clock and does a
     basic UART init at 115200. We drive the baudrate ourselves below. */
  BSP_COM_Init(COM1);

  /* Step 1: at 9600 baud (u-blox factory default) ask GPS to switch its
     UART1 to GPS_UART_BAUDRATE. Best-effort, no ACK wait — if the module
     was already at GPS_UART_BAUDRATE (second boot onwards), any reply
     would come back at the new baudrate and we couldn't read it here.
     Requires PA0 (UART4 TX) wired to GPS RX. */
  GPS_UART->Init.BaudRate = 9600;
  HAL_UART_Init(GPS_UART);
  gps_cfg_port_uart1(GPS_UART_BAUDRATE);
  HAL_Delay(200);  /* drain TX + let the GPS apply the new baudrate */

  /* Step 2: reconfigure UART4 to match the GPS. */
  GPS_UART->Init.BaudRate = GPS_UART_BAUDRATE;
  HAL_UART_Init(GPS_UART);
  HAL_Delay(50);   /* let the line settle before reading ACKs */

  /* Step 3: send UBX-CFG-RATE FIRST — it's the command that actually
     controls fix frequency. Block on ACK-ACK with 3 retries so we don't
     silently continue if the module dropped the packet.  Earlier builds
     sent this without any ACK check and observed 1 Hz in the logs despite
     a 10 Hz config. */
  uint8_t rate_p[6];
  rate_p[0] = (uint8_t)(GPS_MEAS_PERIOD_MS & 0xFF);
  rate_p[1] = (uint8_t)(GPS_MEAS_PERIOD_MS >> 8);
  rate_p[2] = 0x01;   rate_p[3] = 0x00;       /* navRate = 1 */
  rate_p[4] = 0x01;   rate_p[5] = 0x00;       /* timeRef = GPS */
  int r_rate = ubx_send_retry(0x06, 0x08, rate_p, sizeof(rate_p), 300, 3);
  sprintf(buf, "rate=%s(%uHz) ", ack_str(r_rate), (unsigned)GPS_RATE_HZ);
  gps_log_append(buf);
  HAL_Delay(100);

  /* Step 4: disable NMEA sentences we don't parse (GLL, GSA, GSV, VTG),
     one at a time with ACK+retry and 50 ms spacing. With only GGA + RMC
     enabled, 10 Hz fits inside the 38400-baud UART budget. */
  const uint8_t unused[4][2] = { {0xF0,0x01}, {0xF0,0x02}, {0xF0,0x03}, {0xF0,0x05} };
  int r_msg = 1;  /* 1 means all four succeeded */
  for (int i = 0; i < 4; i++)
  {
    uint8_t mp[3] = { unused[i][0], unused[i][1], 0 };
    int r = ubx_send_retry(0x06, 0x01, mp, sizeof(mp), 300, 3);
    if (r != 1) r_msg = r;   /* remember the worst result */
    HAL_Delay(50);
  }
  sprintf(buf, "msg=%s ", ack_str(r_msg));
  gps_log_append(buf);

  /* Step 4b: switch the navigation engine's dynamic-platform model to Sea
     (CFG-NAVSPG-DYNMODEL = 5). The factory default is Portable (0), which
     assumes automotive-class accelerations and tunes the Kalman filter
     accordingly — at low SUPfoil/wing speeds (1–10 km/h) this collapses
     the reported ground speed (NMEA RMC Speed field) toward zero even
     while position is clearly advancing. Sea model expects low-g, low-
     acceleration motion over water and trusts the Doppler velocity
     instead of suppressing it.

     Layers = 0x01 (RAM only). The first attempt used 0x07 (RAM+BBR+Flash)
     and got NAK'd on Peter's MAX-M10S (5.5.2026 boot log:
     `dyn=NAK(Sea)`) — some MAX-M10S firmware variants don't accept a
     single VALSET targeting all three layers (Flash write timing or
     atomicity constraints). RAM-only takes effect immediately and the
     existing UBX-CFG-CFG save step at the end of GPS_Init persists the
     RAM state to BBR + Flash, so persistence across power cycles is
     preserved through the legacy save path. */
  uint8_t valset_p[9] = {
    0x00,                               /* version */
    0x01,                               /* layers: RAM only */
    0x00, 0x00,                         /* transaction (none) + reserved */
    0x21, 0x00, 0x11, 0x20,             /* key CFG-NAVSPG-DYNMODEL (LE) */
    0x05                                /* value: 5 = Sea */
  };
  int r_dyn = ubx_send_retry(0x06, 0x8A, valset_p, sizeof(valset_p), 300, 3);
  sprintf(buf, "dyn=%s(Sea) ", ack_str(r_dyn));
  gps_log_append(buf);
  HAL_Delay(50);

  /* Step 5: persist everything (rate + port + msg config) to BBR + Flash
     + EEPROM so the module boots in the right config next time. */
  uint8_t save_p[13] = {0};
  save_p[4] = 0xFF; save_p[5] = 0xFF;         /* saveMask = all sections */
  save_p[12] = 0x17;                          /* deviceMask = BBR|Flash|EEPROM */
  int r_save = ubx_send_retry(0x06, 0x09, save_p, sizeof(save_p), 600, 2);
  sprintf(buf, "save=%s", ack_str(r_save));
  gps_log_append(buf);
  HAL_Delay(150);

  /* Enable UART4 interrupt in the NVIC */
  HAL_NVIC_SetPriority(UART4_IRQn, 6, 0);
  HAL_NVIC_EnableIRQ(UART4_IRQn);

  /* Start interrupt-driven single-byte RX; callback chains itself */
  HAL_UART_Receive_IT(GPS_UART, &RxByte, 1);
}

uint8_t GPS_GetLatestFix(GPS_Fix_t *out)
{
  if (!out) return 0;
  UINT primask = tx_interrupt_control(TX_INT_DISABLE);
  uint8_t updated = FixUpdated;
  *out = LatestFix;
  FixUpdated = 0;
  tx_interrupt_control(primask);
  return updated;
}

uint8_t GPS_GetWallClock(uint32_t *year, uint32_t *month, uint32_t *day,
                         uint32_t *hour, uint32_t *minute, uint32_t *second)
{
  UINT primask = tx_interrupt_control(TX_INT_DISABLE);
  uint8_t valid = WallClockValid;
  if (valid)
  {
    if (year)   *year   = WallClockYear;
    if (month)  *month  = WallClockMonth;
    if (day)    *day    = WallClockDay;
    if (hour)   *hour   = WallClockHour;
    if (minute) *minute = WallClockMin;
    if (second) *second = WallClockSec;
  }
  tx_interrupt_control(primask);
  return valid;
}

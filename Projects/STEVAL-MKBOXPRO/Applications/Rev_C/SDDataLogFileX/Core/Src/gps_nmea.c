/**
  ******************************************************************************
  * @file    gps_nmea.c
  * @brief   u-blox MAX-M10S NMEA parser on UART4 @ 38400 baud
  ******************************************************************************
  */

#include "gps_nmea.h"
#include "stbox1_config.h"
#include "SensorTileBoxPro.h"
#include "tx_api.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* UART4 handle is provided by the BSP as hcom_uart[COM1] */
extern UART_HandleTypeDef hcom_uart[];
#define GPS_UART (&hcom_uart[COM1])

#define NMEA_LINE_MAX 96

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

  UINT primask = tx_interrupt_control(TX_INT_DISABLE);

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

  tx_interrupt_control(primask);
}

/* Parse $GNGGA: time, lat, N/S, lon, E/W, quality, num-sat, HDOP, alt, M, ... */
static void parse_gga(char *fields[], int n)
{
  if (n < 10) return;
  const char *quality = fields[6];
  const char *numsat  = fields[7];
  const char *hdop    = fields[8];
  const char *alt     = fields[9];

  UINT primask = tx_interrupt_control(TX_INT_DISABLE);

  LatestFix.Fix    = (uint8_t)atoi(quality);
  LatestFix.NumSat = (uint8_t)atoi(numsat);
  LatestFix.Hdop   = (float)strtod(hdop, NULL);
  LatestFix.Alt    = (float)strtod(alt, NULL);
  FixUpdated       = 1;

  tx_interrupt_control(primask);
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

void GPS_UART_RxByte(uint8_t byte)
{
  if (byte == '\n' || byte == '\r')
  {
    if (LineLen > 0 && LineLen < NMEA_LINE_MAX)
    {
      LineBuf[LineLen] = '\0';
      nmea_handle_line(LineBuf);
    }
    LineLen = 0;
    return;
  }
  if (byte == '$')
  {
    /* new sentence start -> reset buffer */
    LineLen = 0;
  }
  if (LineLen < NMEA_LINE_MAX - 1)
  {
    LineBuf[LineLen++] = (char)byte;
  }
  else
  {
    /* overrun: drop the line, wait for next '$' */
    LineLen = 0;
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
    /* Clear any pending error flags and re-arm RX */
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);
    LineLen = 0;
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

/* UBX-CFG-CFG: save all config sections to BBR + Flash + EEPROM. */
static void gps_cfg_save_all(void)
{
  uint8_t p[13] = {0};
  /* clearMask = 0 */
  p[4] = 0xFF; p[5] = 0xFF;               /* saveMask = 0xFFFF (all sections) */
  /* loadMask = 0 */
  p[12] = 0x17;                           /* deviceMask = BBR | Flash | EEPROM */
  ubx_send(0x06, 0x09, p, sizeof(p));     /* CFG-CFG */
}

/* UBX-CFG-RATE: set measurement + navigation rate.
   meas_ms = interval between measurements in ms (100 = 10 Hz).
   nav_cycles = navigation solution per N measurements (1 = every meas). */
static void gps_cfg_rate(uint16_t meas_ms, uint16_t nav_cycles)
{
  uint8_t p[6];
  p[0] = (uint8_t)(meas_ms    & 0xFF);
  p[1] = (uint8_t)(meas_ms    >> 8);
  p[2] = (uint8_t)(nav_cycles & 0xFF);
  p[3] = (uint8_t)(nav_cycles >> 8);
  p[4] = 0x01;                            /* timeRef = GPS */
  p[5] = 0x00;
  ubx_send(0x06, 0x08, p, sizeof(p));     /* CFG-RATE */
}

/* UBX-CFG-MSG: set the UART1 output rate of a single NMEA sentence.
   rate = 0 disables the sentence, 1 = every nav solution, etc. */
static void gps_cfg_msg_rate(uint8_t msg_class, uint8_t msg_id, uint8_t rate)
{
  uint8_t p[3] = { msg_class, msg_id, rate };
  ubx_send(0x06, 0x01, p, sizeof(p));     /* CFG-MSG (short form = UART1 only) */
}

/* Disable the NMEA sentences we don't parse, so 10 Hz fits inside the
   38400-baud UART budget. Keep only GGA + RMC. */
static void gps_cfg_disable_unused_nmea(void)
{
  gps_cfg_msg_rate(0xF0, 0x01, 0);        /* GLL off */
  gps_cfg_msg_rate(0xF0, 0x02, 0);        /* GSA off */
  gps_cfg_msg_rate(0xF0, 0x03, 0);        /* GSV off */
  gps_cfg_msg_rate(0xF0, 0x05, 0);        /* VTG off */
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

void GPS_Init(void)
{
  memset(&LatestFix, 0, sizeof(LatestFix));
  FixUpdated = 0;
  LineLen = 0;

  /* BSP_COM_Init configures GPIO AF + enables UART4 clock and does a
     basic UART init at 115200. We drive the baudrate ourselves below. */
  BSP_COM_Init(COM1);

  /* Step 1: open UART4 at 9600 (u-blox factory default) and tell the GPS
     to switch its UART1 to GPS_UART_BAUDRATE. If the GPS was already at
     GPS_UART_BAUDRATE, it sees the 9600-rate bytes as garbage and ignores
     them — harmless. Requires PA0 (UART4 TX) to be wired to GPS RX. */
  GPS_UART->Init.BaudRate = 9600;
  HAL_UART_Init(GPS_UART);
  gps_cfg_port_uart1(GPS_UART_BAUDRATE);
  HAL_Delay(150);  /* drain TX + let the GPS apply the new baudrate */

  /* Step 2: reconfigure UART4 to match the GPS. */
  GPS_UART->Init.BaudRate = GPS_UART_BAUDRATE;
  HAL_UART_Init(GPS_UART);

  /* Step 3: disable the NMEA sentences we don't parse and set the fix rate
     from GPS_MEAS_PERIOD_MS (derived from GPS_RATE_HZ, default 10 Hz — can
     be overridden at build time with `make GPS_RATE_HZ=<n>`). At 38400 baud
     with only GGA + RMC, rates up to ~25 Hz fit in the UART budget. */
  gps_cfg_disable_unused_nmea();
  HAL_Delay(50);
  gps_cfg_rate(GPS_MEAS_PERIOD_MS, 1);
  HAL_Delay(50);

  /* Step 4: tell the GPS to persist the new baudrate + rate + msg config
     to BBR + Flash so it survives power cycles. */
  gps_cfg_save_all();
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

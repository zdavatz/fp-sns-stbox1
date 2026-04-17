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
/* Public API                                                         */
/* ------------------------------------------------------------------ */

void GPS_Init(void)
{
  memset(&LatestFix, 0, sizeof(LatestFix));
  FixUpdated = 0;
  LineLen = 0;

  /* BSP_COM_Init configures GPIO AF + enables UART4 clock and does a
     basic UART init at 115200. We then override baudrate to the GPS value. */
  BSP_COM_Init(COM1);

  GPS_UART->Init.BaudRate = GPS_UART_BAUDRATE;
  HAL_UART_Init(GPS_UART);

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

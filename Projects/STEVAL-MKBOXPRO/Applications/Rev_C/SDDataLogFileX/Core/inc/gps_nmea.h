/**
  ******************************************************************************
  * @file    gps_nmea.h
  * @brief   u-blox MAX-M10S NMEA parser on UART4 @ 38400 baud
  ******************************************************************************
  *
  * Parses $GNRMC (time, lat/lon, speed/course) and $GNGGA (altitude, fix,
  * num-satellites, HDOP). Ignores other NMEA sentences.
  *
  * Wiring (SensorTile.box PRO Rev_C):
  *   GPS TX  -> PA1 (UART4 RX)
  *   GPS RX  -> PA0 (UART4 TX)   (optional, for UBX config only)
  *   3V3     -> 3V3
  *   GND     -> GND
  *
  * The GPS module must be pre-configured once via u-center to 38400 baud
  * and that setting saved to GPS flash (CFG-CFG Save). No runtime UBX config.
  *
  ******************************************************************************
  */

#ifndef __GPS_NMEA_H
#define __GPS_NMEA_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32u5xx_hal.h"
#include <stdint.h>

typedef struct
{
  uint32_t TickMs;        /* ThreadX tick count when fix was parsed */
  uint8_t  Valid;         /* 1 if a valid fix has been parsed since power-on */
  uint8_t  Fix;           /* GGA quality: 0=no fix, 1=GPS, 2=DGPS */
  uint8_t  NumSat;        /* number of satellites used (GGA) */
  char     Utc[12];       /* "hhmmss.ss" from RMC */
  double   Lat;           /* decimal degrees, signed (N=+, S=-) */
  double   Lon;           /* decimal degrees, signed (E=+, W=-) */
  float    Alt;           /* metres above mean sea level (GGA) */
  float    SpeedKmh;      /* ground speed in km/h (RMC knots * 1.852) */
  float    Course;        /* course over ground, degrees true (RMC) */
  float    Hdop;          /* horizontal dilution of precision (GGA) */
} GPS_Fix_t;

/* Initialise UART4 for GPS at GPS_UART_BAUDRATE and start interrupt-driven RX.
   Must be called after the system clock + GPIO are ready. */
void GPS_Init(void);

/* Copy the most recent parsed fix atomically.
   Returns 1 if the fix was updated since the last call, 0 otherwise. */
uint8_t GPS_GetLatestFix(GPS_Fix_t *out);

/* Copy the most recent GPS wall-clock (UTC) parsed from $GNRMC.
   Returns 1 if a valid date+time has been seen since power-on, 0 otherwise.
   Year is four digits (e.g. 2026). Hour/min/sec are integers; sub-second
   milliseconds are ignored — good enough to seed FileX's FAT clock. */
uint8_t GPS_GetWallClock(uint32_t *year, uint32_t *month, uint32_t *day,
                         uint32_t *hour, uint32_t *minute, uint32_t *second);

/* Return the status log from the last GPS_Init() run — one short line
   summarising the UBX-CFG-RATE / CFG-MSG / CFG-CFG ACK results. Written
   to the error log in COMMAND_START_LOG so we can verify post-boot that
   the GPS accepted the requested fix rate. Null-terminated, always safe
   to read. Example: "rate=OK(10Hz) msg=OK save=OK". */
const char *GPS_GetInitLog(void);

/* Called from UART4_IRQHandler via HAL_UART_IRQHandler -> RxCpltCallback.
   Pushes the incoming byte into the UART RX ring buffer. NMEA line
   assembly + sentence parsing is deferred to GPS_Process(), which must
   run at thread priority — this keeps the UART4 IRQ short enough to not
   preempt the SDMMC1 IRQ during SD card writes. */
void GPS_UART_RxByte(uint8_t byte);

/* Drain the UART RX ring buffer, assemble NMEA lines, and call the
   sentence parsers for complete $GNRMC / $GNGGA lines. Call once per
   GPS thread poll cycle, before GPS_GetLatestFix. Cheap when the ring
   is empty (~3 instructions). */
void GPS_Process(void);

/* Direct IRQ hook for UART4 */
void GPS_UART_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __GPS_NMEA_H */

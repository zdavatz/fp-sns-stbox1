/**
  ******************************************************************************
  * @file    stbox1_config.h
  * @author  System Research & Applications Team - Catania Lab.
  * @version V2.1.0
  * @date    20-May-2025
  * @brief   FP-SNS-STBOX1 configuration
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STBOX1_CONFIG_H
#define __STBOX1_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Exported define ------------------------------------------------------------*/

/* For enabling the printf.
   NOTE: disabled by default because UART4 (PA0/PA1) is now wired to the
   u-blox MAX-M10S GPS module. Re-enable only if you disconnect the GPS
   and want serial debug on UART4 again. */
/* #define STBOX1_ENABLE_PRINTF */

/* GPS module (u-blox MAX-M10S) on UART4 @ 38400 baud, 8N1, NMEA output.
   Configure the GPS once via u-center: UART baudrate -> 38400,
   then CFG-CFG Save to persist into GPS flash. */
#define STBOX1_GPS_ENABLE
#define GPS_UART_BAUDRATE  38400

/* GPS measurement rate in Hz. Override at build time:
     make GPS_RATE_HZ=5
   Valid 1..25 for single-GNSS mode on u-blox MAX-M10S. With only GGA+RMC
   enabled, 25 Hz is the UART ceiling at 38400 baud. */
#ifndef GPS_RATE_HZ
#define GPS_RATE_HZ 10
#endif
#if (GPS_RATE_HZ < 1) || (GPS_RATE_HZ > 25)
#error "GPS_RATE_HZ must be in [1, 25] Hz for MAX-M10S at 38400 baud."
#endif
#define GPS_MEAS_PERIOD_MS (1000 / GPS_RATE_HZ)

/* Audio logging (MicNNN.wav via MP23DB01HP). Disabled by default:
   on Peter's hardware-modified board (3.3V + various disconnected
   connectors) BSP_AUDIO_IN_Init() hangs indefinitely, blocking the
   fx_thread before it can write any sensor or GPS samples. Set to 1
   only on an unmodified SensorTile.box PRO. */
#define STBOX1_LOG_AUDIO 0

#define STTS22H_ODR 1.0f /* ODR = 1.0Hz */
#define ISM330DHCX_ACC_ODR 104.0f /* ODR = 104Hz */
#define ISM330DHCX_ACC_FS 4 /* FS = 4g */
#define ISM330DHCX_GYRO_ODR 104.0f /* ODR = 104Hz */
#define ISM330DHCX_GYRO_FS 2000 /* FS = 2000dps */
#define ILPS22QS_ODR 25.0f /* ODR = 25.0Hz */
#define IIS2MDC_MAG_ODR 100.0f /* ODR = 100Hz */
#define IIS2MDC_MAG_FS 50 /* FS = 50gauss */

/**************************************
  * Don't Change the following defines *
  ***************************************/

/* Package Version only numbers 0->9 */
#define STBOX1_VERSION_MAJOR '2'
#define STBOX1_VERSION_MINOR '1'
#define STBOX1_VERSION_PATCH '0'

/* Package Name */
#define STBOX1_PACKAGENAME "SDDataLogFileX"


#ifdef STBOX1_ENABLE_PRINTF
#define STBOX1_PRINTF(...) printf(__VA_ARGS__)
#else /* STBOX1_ENABLE_PRINTF */
#define STBOX1_PRINTF(...)
#endif /* STBOX1_ENABLE_PRINTF */

#ifdef __cplusplus
}
#endif

#endif /* __STBOX1_CONFIG_H */

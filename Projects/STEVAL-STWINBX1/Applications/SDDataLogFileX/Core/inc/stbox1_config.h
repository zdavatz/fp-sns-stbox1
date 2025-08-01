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

/* For enabling the printf */
#define STBOX1_ENABLE_PRINTF

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
#define STBOX1_VERSION_MINOR '0'
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

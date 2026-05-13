/**
  ******************************************************************************
  * @file    main.h
  * @author  System Research & Applications Team - Catania Lab.
  * @version V2.1.0
  * @date    20-May-2025
  * @brief   Header for main.c file.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "SensorTileBoxPro.h"
#include "SensorTileBoxPro_sd.h"
#include "fx_stm32_sd_driver.h"
#include "stbox1_config.h"
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
extern void Error_Handler(char *File,int32_t Line);

/* USER CODE BEGIN EFP */
extern void ErrorLog_Write(const char *msg);
extern unsigned int ErrorLog_Flush(void);
extern void ErrorLog_Close(void);
extern void DiagBlinkRed(uint8_t n);

/* Issue #17: independent watchdog kick. Refreshed by fx_thread's main
 * loop. Started in main() before MX_ThreadX_Init. 2 s timeout. */
extern void Iwdg_Refresh(void);

/* Mode-switch (issue #14, 2026-05-10). Each mode runs ST's reference
 * design exactly — never concurrent. Boot picks one based on TAMP
 * backup register. fx_thread + ble_sync_thread read this and adjust
 * behaviour: in LOG mode, ble_sync parks; in BLE mode, fx_thread
 * doesn't open SD media for periodic writing. State transitions are
 * via NVIC_SystemReset (BLE thread sets BKP1R=LOG magic + duration
 * → reboot; fx_thread on duration expiry clears BKP1R + reboot back
 * to BLE). */
typedef enum {
  APP_MODE_BLE = 0,  /* default — advertising, no logging */
  APP_MODE_LOG = 1,  /* logger active, BLE off */
} app_mode_t;
extern volatile app_mode_t g_app_mode;
extern volatile uint32_t   g_log_duration_seconds;  /* set by BLE thread, read by fx_thread */

/* v57: post-mortem fault info, populated by ARM fault handlers in
   stm32u5xx_it.c and read by ErrorLog_Open after reset. Lives in
   .noinit section so it survives warm reset. */
typedef struct {
  uint32_t magic;        /* 0xFA0177AB if valid */
  uint32_t fault_id;     /* 1=HardFault 2=MemManage 3=BusFault 4=UsageFault 5=NMI */
  uint32_t pc;           /* PC at the faulting instruction */
  uint32_t lr;           /* LR at fault entry */
  uint32_t psr;          /* xPSR at fault entry */
  uint32_t cfsr;         /* SCB->CFSR */
  uint32_t hfsr;         /* SCB->HFSR */
  uint32_t mmfar;        /* SCB->MMFAR */
  uint32_t bfar;         /* SCB->BFAR */
} fault_info_t;

/* v59: disk-less diagnostic ring buffer. Each DIAG_CHECKPOINT(label)
   call updates RAM only (no SD I/O, cannot hang). After a power-cycle
   ErrorLog_Open reads the ring and emits every entry to disk in one
   batched write — surviving even if fx_media_flush is broken on the
   second-and-subsequent calls. Ring size 32 entries × 8 bytes = 256
   bytes. Survives warm reset because it lives in .noinit. */
#define DIAG_RING_SIZE   32U
#define DIAG_RING_MAGIC  0xD1A6BE17U   /* "DIAGBE17" */
typedef struct {
  uint32_t tick;     /* tx_time_get() at the moment, or 0 pre-RTOS */
  uint32_t code;     /* checkpoint code, see DC_* below */
} diag_entry_t;
typedef struct {
  uint32_t       magic;     /* DIAG_RING_MAGIC if valid */
  uint32_t       head;      /* next slot to write */
  uint32_t       wrap;      /* nonzero once head wrapped → ring is full */
  diag_entry_t   entries[DIAG_RING_SIZE];
} diag_ring_t;
extern diag_ring_t g_diag_ring;

/* checkpoint codes — high byte = subsystem, low 24 bits = state */
#define DC_INIT              0x010001U  /* main() entered */
#define DC_SD_INIT_OK        0x010002U
#define DC_SD_INIT_FAIL      0x0100FFU
#define DC_FX_THREAD_ENTRY   0x020001U  /* fx_thread_entry first instruction */
#define DC_FX_QUEUE_GET      0x020002U  /* about to receive next message */
#define DC_FX_START_LOG      0x020010U  /* COMMAND_START_LOG case entered */
#define DC_FX_LED_ON         0x020011U
#define DC_FX_FILE_NAME_SET  0x020012U
#define DC_FX_BEFORE_OPEN    0x020013U  /* about to call ErrorLog_Open */
#define DC_FX_AFTER_OPEN     0x020014U  /* ErrorLog_Open returned */
#define DC_FX_DIAG_A_WRITE   0x020020U
#define DC_FX_DIAG_A_FLUSH   0x020021U  /* about to call ErrorLog_Flush #1 */
#define DC_FX_DIAG_A_DONE    0x020022U  /* ErrorLog_Flush #1 returned */
#define DC_FX_DIAG_B_WRITE   0x020023U
#define DC_FX_DIAG_B_FLUSH   0x020024U
#define DC_FX_DIAG_B_DONE    0x020025U
#define DC_FX_DIAG_C_WRITE   0x020026U
#define DC_FX_DIAG_C_DONE    0x020027U
#define DC_FX_HEARTBEAT      0x020030U  /* in heartbeat loop, low byte = iter % 256 */
/* v75: checkpoints in the normal Sens/Gps/Bat path (DIAG_ONLY=0) so we
   can see how far fx_thread gets when the ErrorLog cache writes don't
   reach disk. .noinit survives slider OFF/ON on this board so the next
   boot's ErrorLog_Open dumps these for inspection. */
#define DC_FX_AFTER_SLEEP_2S 0x020040U  /* 2 sec after ErrorLog_Open */
#define DC_FX_BEFORE_CREATE  0x020041U  /* about to fx_file_create Sens */
#define DC_FX_AFTER_CREATE   0x020042U  /* fx_file_create returned */
#define DC_FX_BEFORE_OPENF   0x020043U  /* about to fx_file_open Sens */
#define DC_FX_AFTER_OPENF    0x020044U  /* fx_file_open returned */
#define DC_FX_BEFORE_SEEK    0x020045U  /* about to fx_file_seek */
#define DC_FX_AFTER_SEEK     0x020046U  /* fx_file_seek returned */
#define DC_FX_BEFORE_HDRWR   0x020047U  /* about to fx_file_write header */
#define DC_FX_AFTER_HDRWR    0x020048U  /* fx_file_write header returned */
#define DC_FX_BEFORE_HFLUSH  0x020049U  /* about to fx_media_flush */
#define DC_FX_AFTER_HFLUSH   0x02004AU  /* fx_media_flush returned */

/* Append a checkpoint to the ring. Cannot hang. Cannot fault. */
extern void Diag_Checkpoint(uint32_t code);
#define DIAG_CHECKPOINT(c) Diag_Checkpoint(c)
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* USER CODE BEGIN Private defines */   

/* USER CODE END Private defines */

/* Exported Variables --------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

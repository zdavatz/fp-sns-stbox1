/**
  ******************************************************************************
  * @file    app_filex.c
  * @author  System Research & Applications Team - Catania Lab.
  * @version V2.1.0
  * @date    20-May-2025
  * @brief   FileX applicative file
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

/* Includes ------------------------------------------------------------------*/
#include "app_filex.h"
#include "usb_cdc.h"   /* UsbCdc_Write — ErrorLog mirror to USB CDC */
#include <string.h>  /* strlen used in ErrorLog_Open's BLE-status line */

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "stbox1_config.h"
#include "SensorTileBoxPro.h"
#include "SensorTileBoxPro_env_sensors.h"
#include "SensorTileBoxPro_motion_sensors.h"
#include "SensorTileBoxPro_audio.h"
#include "SensorTileBoxPro_gg.h"
#include "SensorTileBoxPro_bus.h"
#include "main.h"
#include "gps_nmea.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  UINT CommandType;
  ULONG MsgTime;
  union {
    struct {
      float pressure;
      float temperature;
      BSP_MOTION_SENSOR_Axes_t acc;
      BSP_MOTION_SENSOR_Axes_t gyro;
      BSP_MOTION_SENSOR_Axes_t mag;
    } sensors;
    GPS_Fix_t gps;
  } payload;
} MessageData_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define DEFAULT_STACK_SIZE               (2 * 1024)

/* fx_app_thread priority */
#define FX_APP_THREAD_PRIO              12

/* read_app_thread priority */
#define READ_APP_THREAD_PRIO              10

/* fx_app_thread preemption priority */
#define FX_APP_PREEMPTION_THRESHOLD     FX_APP_THREAD_PRIO

/* read_app_thread preemption priority */
#define READ_APP_PREEMPTION_THRESHOLD     READ_APP_THREAD_PRIO

#define   MESSAGE_QUEUE_SIZE                 100

#define COMMAND_STOP_LOG  0
#define COMMAND_START_LOG  1
#define COMMAND_SAVE_AUDIO 2
#define COMMAND_SAVE_SENSORS 3
#define COMMAND_SAVE_GPS 4

#define STBOX1_AUDIO_DATA_NOT_READY 0xFFFFFF

#define PCM_AUDIO_IN_SAMPLES     (AUDIO_IN_SAMPLING_FREQUENCY / 1000)
#define AUDIO_BUFF_SIZE (PCM_AUDIO_IN_SAMPLES *1024)

#define SKIP_FIRST_200_MS 200

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* Buffer for FileX FX_MEDIA sector cache. this should be 32-Bytes
aligned to avoid cache maintenance issues */
ALIGN_32BYTES(uint32_t media_memory[FX_STM32_SD_DEFAULT_SECTOR_SIZE / sizeof(uint32_t)]);

/* Define FileX global data structures.  */
FX_MEDIA        sdio_disk;
FX_FILE         SensorsFxFile;
FX_FILE         AudioFxFile;
FX_FILE         GpsFxFile;
/* Define ThreadX global data structures.  */
TX_THREAD       fx_app_thread;
TX_THREAD       read_app_thread;
TX_THREAD       gps_app_thread;
TX_QUEUE        MessageQueue;

/* Timer for reading the sensor's Data*/
TX_TIMER ReadTimer;

/* Semaphore for controlling the Reading Thread */
TX_SEMAPHORE SemaphorePtr;

/* Array for Sensors Values */
static UCHAR SensorBuffer[MESSAGE_QUEUE_SIZE * sizeof(MessageData_t)];
static UCHAR *SensorBufferWritingPointer = SensorBuffer;
static UCHAR *SensorBufferEndPointer = SensorBuffer + MESSAGE_QUEUE_SIZE * sizeof(MessageData_t);

/* Output PCM buffer from Digital Microphone */
uint16_t OnBoard_PCM_Buffer[2 * PCM_AUDIO_IN_SAMPLES];

static uint16_t Audio_OUT_Buff[AUDIO_BUFF_SIZE];
static volatile uint32_t WriteIndexBufferAudio = 0;
static volatile uint32_t  ReadIndexBufferAudio = STBOX1_AUDIO_DATA_NOT_READY;
static volatile int32_t SkipFirst200mS;

/* Header for wav audio file */
static uint8_t pAudioHeader[44];

static volatile CHAR SensorsFileOpen = 0;
static volatile CHAR AudioFileOpen = 0;
static volatile CHAR GpsFileOpen = 0;

/* Error log file */
FX_FILE         ErrorLogFxFile;
static volatile CHAR ErrorLogFileOpen = 0;

#if STBOX1_LOG_BATTERY
/* STC3115 fuel-gauge state. Init attempt happens once on first START_LOG
   (guarded by BatteryInitAttempted). On I2C failure we disable battery
   logging for the remainder of the boot but keep the rest of the logger
   running — same non-fatal pattern used for the MIC. */
FX_FILE              BatteryFxFile;
static volatile CHAR BatteryFileOpen        = 0;
static volatile CHAR BatteryInitAttempted   = 0;
static volatile CHAR BatteryInitOk          = 0;
static void         *HandleGGComponent      = NULL;
#endif /* STBOX1_LOG_BATTERY */

/* Wall-clock base for UpdateFileXClock. Starts as __DATE__/__TIME__ at
   boot; once $GNRMC delivers a valid date+time, gps_thread overwrites the
   base so FAT timestamps on Sens/Gps/Bat files track UTC instead of the
   stale compile date. */
static UINT  ClockBaseYear, ClockBaseMonth, ClockBaseDay;
static UINT  ClockBaseHour, ClockBaseMin,   ClockBaseSec;
static ULONG ClockBaseTick;  /* tx_time_get() at the moment base was set */
static volatile CHAR ClockSeededFromGPS = 0;

/* For Understanding Max Message Queues size */
INT MessagePushed = 0;
INT MessageRemoved = 0;
INT MessageMaxSize = 0;


/* USER CODE END PV */

static volatile uint32_t UserButtonPressed = 0;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static void fx_thread_entry(ULONG thread_input);
static void read_thread_entry(ULONG thread_input);
static void gps_thread_entry(ULONG thread_input);
static void ReadingTimerCallbackFunction(ULONG timer);
static void AudioProcess_SD_Recording(uint16_t *pInBuff, uint32_t len);
static uint32_t WavProcess_HeaderInit(void);
static uint32_t WavProcess_HeaderUpdate(uint32_t len);
static void ErrorLog_BuildFilename(CHAR *buf);
static void ErrorLog_Open(void);
static void WriteFwInfoFile(void);
static void WriteCheckpointFile(int chk_num, int need_open);
static INT  CheckAndApplyFirmwareUpdate(void);
static void InitClockBaseFromCompileDate(void);
static void SetClockBaseFromGPS(UINT year, UINT month, UINT day,
                                UINT hour, UINT minute, UINT second);
static void UpdateFileXClock(void);

/* USER CODE END PFP */

/**
  * @brief  Application FileX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT MX_FileX_Init(VOID *memory_ptr)
{
  UINT ret = FX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL *)memory_ptr;

  /* USER CODE BEGIN App_FileX_MEM_POOL */
  (void)byte_pool;
  /* USER CODE END App_FileX_MEM_POOL */

  /* USER CODE BEGIN MX_FileX_Init */

  VOID *pointer;

  /* Allocate memory for the writing thread's stack */
  ret = tx_byte_allocate(byte_pool, &pointer, DEFAULT_STACK_SIZE * 2, TX_NO_WAIT);

  if (ret != FX_SUCCESS)
  {
    /* Failed at allocating memory */
    Error_Handler(__FILE__, __LINE__);
  }

  /* Create the writing thread.  */
  tx_thread_create(&fx_app_thread, "FileX Writing App Thread", fx_thread_entry, 0, pointer, DEFAULT_STACK_SIZE * 2,
                   FX_APP_THREAD_PRIO, FX_APP_PREEMPTION_THRESHOLD, TX_NO_TIME_SLICE, TX_AUTO_START);

  /* Initialize FileX.  */
  fx_system_initialize();

  /* Seed the FileX system clock so files don't get FAT's default
     31.12.16 timestamp. Compile-date base is used until the first
     $GNRMC fix, after which gps_thread overrides it with GPS UTC.
     Re-applied before each file create and periodic flush. */
  InitClockBaseFromCompileDate();
  UpdateFileXClock();

  STBOX1_PRINTF("FileX Thread Created\r\n");

  /* Allocate memory for the reading thread's stack */
  ret = tx_byte_allocate(byte_pool, &pointer, DEFAULT_STACK_SIZE * 2, TX_NO_WAIT);

  if (ret != FX_SUCCESS)
  {
    /* Failed at allocating memory */
    Error_Handler(__FILE__, __LINE__);
  }

  /* Create the reading thread.  */
  tx_thread_create(&read_app_thread, "Reading Sensor App Thread", read_thread_entry, 0, pointer, DEFAULT_STACK_SIZE * 2,
                   READ_APP_THREAD_PRIO, READ_APP_PREEMPTION_THRESHOLD, TX_NO_TIME_SLICE, TX_AUTO_START);


  STBOX1_PRINTF("Read Sensor Thread Created\r\n");

  /* Allocate memory for the GPS poll thread (smaller stack, rarely runs) */
  ret = tx_byte_allocate(byte_pool, &pointer, DEFAULT_STACK_SIZE, TX_NO_WAIT);

  if (ret != FX_SUCCESS)
  {
    Error_Handler(__FILE__, __LINE__);
  }

  /* Create the GPS poll thread — polls GPS_GetLatestFix at 1 Hz
     and pushes COMMAND_SAVE_GPS into the queue when a new fix arrives. */
  tx_thread_create(&gps_app_thread, "GPS App Thread", gps_thread_entry, 0, pointer, DEFAULT_STACK_SIZE,
                   READ_APP_THREAD_PRIO + 1, READ_APP_THREAD_PRIO + 1, TX_NO_TIME_SLICE, TX_AUTO_START);

  STBOX1_PRINTF("GPS Thread Created\r\n");

  /* Allocate memory for MessageQueue */
  ret = tx_byte_allocate(byte_pool, &pointer, MESSAGE_QUEUE_SIZE * sizeof(MessageData_t *), TX_NO_WAIT);

  if (ret != FX_SUCCESS)
  {
    /* Failed at allocating memory */
    Error_Handler(__FILE__, __LINE__);
  }

  /* Create the MessageQueue shared by Reading and Writing Thread */
  if (tx_queue_create(&MessageQueue, "Message Queue", sizeof(MessageData_t *) / 4,
                      pointer, MESSAGE_QUEUE_SIZE * sizeof(MessageData_t *)) != TX_SUCCESS)
  {
    ret = TX_QUEUE_ERROR;
  }

  if (ret != FX_SUCCESS)
  {
    /* Failed at allocating Queue */
    Error_Handler(__FILE__, __LINE__);
  }

  STBOX1_PRINTF("MessageQueue Created\r\n");

  /* Create the tx_timer */
  if (tx_timer_create(
        &ReadTimer,
        "ReadingTimer",
        ReadingTimerCallbackFunction,
        (ULONG)TX_NULL,
        1 /* 10ms */,
        1 /* 10ms */,
        TX_NO_ACTIVATE) != TX_SUCCESS)
  {
    Error_Handler(__FILE__, __LINE__);
  }

  STBOX1_PRINTF("ReadingTimer Created:\r\n\t Read Sensors@100Hz\r\n\t Audio 48KHz\r\n");

  if (tx_semaphore_create(&SemaphorePtr, "Semaphore for Sensor Reading", 0 /* Initial value */) != TX_SUCCESS)
  {
    Error_Handler(__FILE__, __LINE__);
  }

  STBOX1_PRINTF("SemaphorePtr Created\r\n");


  STBOX1_PRINTF("System Ready...\r\n\tPress User Button\r\n\tfor Starting/Stopping SD Log\r\n");

  /* USER CODE END MX_FileX_Init */

  return ret;
}

/* USER CODE BEGIN 1 */

static void ReadingTimerCallbackFunction(ULONG timer)
{
  /* Release the Semaphore */
  tx_semaphore_put(&SemaphorePtr);
}

static void fx_thread_entry(ULONG thread_input)
{
  /* Diagnostic: turn green LED on the moment fx_thread is scheduled.
     If green is still dark after the boot beep + tx_app_define red blink,
     fx_thread never got scheduled (priority inversion, scheduler hang,
     etc.). If green appears immediately but the SD card stays empty,
     fx_thread is running but blocks in CheckAndApplyFirmwareUpdate
     (most likely fx_media_open). The COMMAND_START_LOG handler below
     calls BSP_LED_On(LED_GREEN) again — idempotent, so this early
     marker doesn't break the normal "green = logging" indication. */
  BSP_LED_On(LED_GREEN);

  UINT status;
  MessageData_t *RMsg;
  /* Time column is `tx_time_get()` — ThreadX ticks (1 tick = 10 ms).
     Header is "Time [10ms]" so downstream tools don't misread it as
     milliseconds. Scripts divide by 100 to get seconds. */
  CHAR header[] = "Time [10ms], AccX [mg], AccY [mg], AccZ [mg], GyroX [mdps], GyroY [mdps], GyroZ [mdps],MagX [mgauss],MagY [mgauss],MagZ [mgauss],P [mB],T ['C]\r\n";

  SHORT SDCardCounter = 0;
  CHAR file_name[30];

  /* Check SD card for firmware update before starting normal logging */
  CheckAndApplyFirmwareUpdate();
  /* v71: WriteCheckpointFile(4, 1) REMOVED. v68/v69/v70 hung at this
     point — fx_media_open after CAFU's fx_media_close does NOT
     complete on Zeno's box, even with a 2 second sleep in between.
     This is not a "two flushes too close" timing issue but something
     structural about the open/close cycle on this hardware. To unblock:
     skip the post-CAFU CHK markers entirely and verify the box reaches
     BSP_LED_On(LED_GREEN). If green comes on, we know fx_thread reaches
     COMMAND_START_LOG handler — the bug is purely in the open/close
     pattern, not in fx_thread scheduling or read_thread posting. */

  while (1)
  {
    /* Determine whether a message MessageQueue  is available */
    status = tx_queue_receive(&MessageQueue, &RMsg, TX_WAIT_FOREVER);
    /* v71: WriteCheckpointFile(6, 1) REMOVED — same reason as CHK04. */
    MessageRemoved++;
    if (status == TX_SUCCESS)
    {
      switch (RMsg->CommandType)
      {
        case COMMAND_START_LOG:
        {
          DIAG_CHECKPOINT(DC_FX_START_LOG);
          /* v71: WriteCheckpointFile(5, 1) REMOVED — same reason as CHK04.
             We want to see if BSP_LED_On(LED_GREEN) actually fires. */
          BSP_LED_Off(LED_RED);
          BSP_LED_On(LED_GREEN);  /* Green ON = logging active */
          DIAG_CHECKPOINT(DC_FX_LED_ON);
          /* v71: WriteCheckpointFile(7, 1) REMOVED — green LED is the
             observable signal whether fx_thread reached this far. */

          /* Init the Queue Statistics */
          MessagePushed = 0;
          MessageRemoved = 0;
          MessageMaxSize = 0;

          /* Reset the Sensors and Mic out Buffers */
          SensorBufferWritingPointer = SensorBuffer;
          WriteIndexBufferAudio = 0;
          ReadIndexBufferAudio = STBOX1_AUDIO_DATA_NOT_READY;
          SkipFirst200mS = SKIP_FIRST_200_MS;

          if (SensorsFileOpen == 0)
          {
            /* Refresh clock so this session's files get a current stamp */
            UpdateFileXClock();

            sprintf(file_name, "Sens%03d.csv", SDCardCounter);
            SDCardCounter++;

            /* v73: SKIP the fx_media_open here. CAFU already opened the
               media and (since v72) does NOT close it because
               fx_media_close hangs in its internal cleanup phase on this
               hardware. The media stays open from boot through the
               session. v72 confirmed: when CAFU skips close, fx_thread
               reaches this case, BSP_LED_On runs (green briefly on?),
               then fx_media_open fails (media already open) and
               Error_Handler fires (red toggle). v73 skips the open so
               fx_thread can proceed to fx_file_create the Sens file
               using the still-open media. Issue #12. */
            status = FX_SUCCESS;
            /* (Original fx_media_open call removed.) */

            STBOX1_PRINTF("SD FX Media Open\r\n");

            /* Open error log file (append) */
            DIAG_CHECKPOINT(DC_FX_BEFORE_OPEN);
            ErrorLog_Open();
            DIAG_CHECKPOINT(DC_FX_AFTER_OPEN);
            /* v75: 2-second sleep after ErrorLog_Open's fx_media_flush
               to give the SD card time to finish its post-flush
               housekeeping. Without this delay, the next operation
               (fx_file_create) hangs internally on its own SDMMC
               transactions, preventing any cached data (manifest, etc.)
               from being committed. */
            tx_thread_sleep(200);
            DIAG_CHECKPOINT(DC_FX_AFTER_SLEEP_2S);

            /* v54: DIAG-ONLY mode for Peter's reservebox. v50/v51/v53 each
               showed the same 4-line truncation; the actual hang location
               is unknown. Before doing anything risky, emit a numbered
               sequence of marker+flush pairs so post-session we can see
               EXACTLY which step on the way to fx_file_create succeeded.
               Then enter a heartbeat-only loop that never tries to create
               Sens/Gps/Bat. Each marker is independently flushed so the
               on-disk tail tells us the last successful operation. */
/* v74: turn OFF the v54 diag-only path. v73 confirmed fx_thread reaches
   COMMAND_START_LOG and BSP_LED_On succeeds, but the diag-only block
   between ErrorLog_Open and the normal Sens-create path was eating
   60+ seconds in heartbeat sleeps + has its own flush hangs (the first
   ErrorLog_Flush after ErrorLog_Open is the same back-to-back flush
   bug). Skipping the block lets fx_thread fall through directly to the
   normal Sens/Gps/Bat creation, which has the v13+ batched-flush
   pattern + tx_thread_sleep(20) before fx_file_create. */
#define STBOX1_DIAG_ONLY 0
#if STBOX1_DIAG_ONLY
            {
              /* v59: NO ErrorLog_Flush calls. v58 hung Peter's box right
                 here — the second fx_media_flush back-to-back after
                 ErrorLog_Open's own flush takes minutes (not infinite,
                 but longer than Peter's patience) due to bounded-but-
                 cumulative SDMMC retries. Instead, every step appends
                 a checkpoint to g_diag_ring in .noinit RAM. After
                 power-cycle, ErrorLog_Open dumps the ring to disk —
                 we see exactly how far this attempt got. */
              DIAG_CHECKPOINT(DC_FX_DIAG_A_WRITE);
              ErrorLog_Write("diag: A");
              DIAG_CHECKPOINT(DC_FX_DIAG_A_FLUSH);
              UINT fst1 = ErrorLog_Flush();
              DIAG_CHECKPOINT(DC_FX_DIAG_A_DONE | (fst1 & 0xFFU));

              DIAG_CHECKPOINT(DC_FX_DIAG_B_WRITE);
              CHAR m[64];
              sprintf(m, "diag: A flush returned 0x%X", (unsigned)fst1);
              ErrorLog_Write(m);
              DIAG_CHECKPOINT(DC_FX_DIAG_B_FLUSH);
              ErrorLog_Flush();
              DIAG_CHECKPOINT(DC_FX_DIAG_B_DONE);

              tx_thread_sleep(100);  /* 1 sec yielding gap */

              DIAG_CHECKPOINT(DC_FX_DIAG_C_WRITE);
              ErrorLog_Write("diag: C — slept 1 s");
              DIAG_CHECKPOINT(DC_FX_DIAG_C_DONE);

              /* Heartbeat loop without per-iteration flush. Each
                 iteration appends checkpoint to .noinit ring. */
              for (UINT iter = 0; iter < 60U; iter++)
              {
                DIAG_CHECKPOINT(DC_FX_HEARTBEAT | (iter & 0xFFU));
                tx_thread_sleep(100);  /* 1 s yielding */
              }
              ErrorLog_Flush();  /* one final flush after the entire run */

              ErrorLog_Write("diag: heartbeat loop done — would now create Sens");
              ErrorLog_Flush();
              /* Fall through to normal Sens/Gps/Bat creation if we got
                 this far. Unlikely on Peter's box — the heartbeat loop
                 will show us the failure point first. */
            }
#endif

            /* v13 strategy: pre-write a forward-looking manifest of all the
               upcoming risky operations as a single batch, then flush ONCE.
               If any subsequent flush hangs (the v11 failure mode), we still
               have all the "about to attempt" markers on disk — telling us
               exactly which step the firmware reached before stalling. The
               many-writes-one-flush pattern matches the working ErrorLog_Open
               path on Peter's 3.3V-modded box. */
            {
              CHAR m[80];
              sprintf(m, "fx: COMMAND_START_LOG enter, first create target='%s'", file_name);
              ErrorLog_Write(m);
            }
            ErrorLog_Write("fx: about to call first fx_file_create Sens");
            ErrorLog_Write("fx: about to call fx_file_open Sens");
            ErrorLog_Write("fx: about to call fx_file_seek Sens");
            ErrorLog_Write("fx: about to call fx_file_write Sens header");

            /* v51: NO explicit flush here. v50 field-test on Peter's
               reservebox showed the manifest ErrorLog_Flush() right after
               ErrorLog_Open's own flush hangs — the SD card is still
               committing the previous flush internally and a back-to-back
               fx_media_flush gets stuck waiting for card-busy to clear.
               Symptom in v50: only the 4 lines from ErrorLog_Open made it
               to disk; the 7 forward-looking manifest lines never landed.
               Fix: leave the manifest writes in the FileX cache. They'll
               get committed implicitly by the next fx_file_create's FAT
               update, which goes through a different (working) path than
               a bare back-to-back fx_media_flush. Cost: if everything
               hangs INSIDE fx_file_create, the manifest is lost — but in
               that case we'd also lose more from a back-to-back flush
               anyway. We add a 200 ms tx_thread_sleep instead so the SD
               card has time to settle from ErrorLog_Open's flush before
               we pile more I/O on it. */
            tx_thread_sleep(20);  /* 200 ms — let SD card finish previous commit */

            DIAG_CHECKPOINT(DC_FX_BEFORE_CREATE);
            /* Create a file in the root directory.  */
            status =  fx_file_create(&sdio_disk, file_name);
            DIAG_CHECKPOINT(DC_FX_AFTER_CREATE | (status & 0xFFU));

            {
              CHAR m[64];
              sprintf(m, "fx: first fx_file_create Sens returned status=0x%X",
                      (unsigned)status);
              ErrorLog_Write(m);
            }
            /* Commit the create's result + the pre-create flush result before
               attempting the next risky step. */
            ErrorLog_Flush();

            /* Check the create status.  */
            if (status != FX_SUCCESS)
            {
              /* Check for an already created status */
              if (status != FX_ALREADY_CREATED)
              {
                /* Create error, call error handler.  */
                STBOX1_PRINTF("Error creating file %s\r\n", file_name);
                Error_Handler(__FILE__, __LINE__);
              }
              else
              {
                /* Searching one available File. Cap iterations at 256 so a
                   stuck directory scan becomes visible (would otherwise be
                   indistinguishable from "hangs forever in fx_file_create"). */
                ULONG iter = 0;
                while (status == FX_ALREADY_CREATED && iter < 256U)
                {
                  sprintf(file_name, "Sens%03d.csv", SDCardCounter);
                  SDCardCounter++;
                  iter++;
                  {
                    CHAR m[80];
                    sprintf(m, "fx: ALREADY_CREATED loop iter=%lu trying='%s'",
                            (unsigned long)iter, file_name);
                    ErrorLog_Write(m);
                  }
                  status =  fx_file_create(&sdio_disk, file_name);
                  {
                    CHAR m[80];
                    sprintf(m, "fx: ALREADY_CREATED loop iter=%lu returned status=0x%X",
                            (unsigned long)iter, (unsigned)status);
                    ErrorLog_Write(m);
                  }
                  /* Check the create status.  */
                  if (status != FX_SUCCESS)
                  {
                    /* Check for an already created status */
                    if (status != FX_ALREADY_CREATED)
                    {
                      /* Create error, call error handler.  */
                      STBOX1_PRINTF("Error creating file %s\r\n", file_name);
                      Error_Handler(__FILE__, __LINE__);
                    }
                  }
                }
                if (iter >= 256U && status == FX_ALREADY_CREATED)
                {
                  ErrorLog_Write("fx: FATAL ALREADY_CREATED loop exceeded 256 iter");
                  Error_Handler(__FILE__, __LINE__);
                }
              }
            }

            ErrorLog_Write("fx: before fx_file_open Sens");

            DIAG_CHECKPOINT(DC_FX_BEFORE_OPENF);
            /* Open the test file.  */
            status =  fx_file_open(&sdio_disk, &SensorsFxFile, file_name, FX_OPEN_FOR_WRITE);
            DIAG_CHECKPOINT(DC_FX_AFTER_OPENF | (status & 0xFFU));

            {
              CHAR m[64];
              sprintf(m, "fx: fx_file_open Sens returned status=0x%X", (unsigned)status);
              ErrorLog_Write(m);
            }
            /* Commit the open's result. */
            ErrorLog_Flush();

            /* Check the file open status.  */
            if (status != FX_SUCCESS)
            {
              /* Error opening file — log instead of hard-stopping so
                 we can see the actual status code in the error log
                 (Error_Handler closes the log and red-LED-loops). */
              ErrorLog_Write("fx: FATAL fx_file_open Sens FAILED");
              STBOX1_PRINTF("Error opening SensXXX.csv\r\n");
              Error_Handler(__FILE__, __LINE__);
            }

            ErrorLog_Write("fx: before fx_file_seek Sens");

            DIAG_CHECKPOINT(DC_FX_BEFORE_SEEK);
            /* Seek to the beginning of the test file.  */
            status =  fx_file_seek(&SensorsFxFile, 0);
            DIAG_CHECKPOINT(DC_FX_AFTER_SEEK | (status & 0xFFU));

            {
              CHAR m[64];
              sprintf(m, "fx: fx_file_seek Sens returned status=0x%X", (unsigned)status);
              ErrorLog_Write(m);
            }

            /* Check the file seek status.  */
            if (status != FX_SUCCESS)
            {
              ErrorLog_Write("fx: FATAL fx_file_seek Sens FAILED");
              Error_Handler(__FILE__, __LINE__);
            }
            SensorsFileOpen = 1;
            STBOX1_PRINTF("File %s open\r\n", file_name);

            ErrorLog_Write("fx: before fx_file_write Sens header");

            DIAG_CHECKPOINT(DC_FX_BEFORE_HDRWR);
            /* Write a string to the test file.  */
            status =  fx_file_write(&SensorsFxFile, header, sizeof(header) - 1);
            DIAG_CHECKPOINT(DC_FX_AFTER_HDRWR | (status & 0xFFU));

            {
              CHAR m[64];
              sprintf(m, "fx: fx_file_write Sens header returned status=0x%X", (unsigned)status);
              ErrorLog_Write(m);
            }

            /* Check the file write status.  */
            if (status != FX_SUCCESS)
            {
              ErrorLog_Write("fx: FATAL fx_file_write Sens FAILED");
              STBOX1_PRINTF("Error writing SensXXX.csv\r\n");
              Error_Handler(__FILE__, __LINE__);
            }

            ErrorLog_Write("fx: before fx_media_flush after Sens header");

            DIAG_CHECKPOINT(DC_FX_BEFORE_HFLUSH);
            /* v77: bring back fx_media_flush. With BLE disabled (see
               STBOX1_ENABLE_BLE_SYNC=0) we test if the previous hang
               was BLE thread / SPI / EXTI11 IRQ interference. */
            fx_media_flush(&sdio_disk);
            DIAG_CHECKPOINT(DC_FX_AFTER_HFLUSH);
            ErrorLog_Write("start: sens header written");

            if (tx_timer_activate(&ReadTimer) != TX_SUCCESS)
            {
              STBOX1_PRINTF("Error activating the TX Timer\r\n");
              Error_Handler(__FILE__, __LINE__);
            }

          }
          else
          {
            STBOX1_PRINTF("Error SensXXX.csv already opened\r\n");
          }

          /* --- open GpsNNN.csv BEFORE the mic block: if mic init hangs
             or fails, we still want GPS to be logging. --- */
          if (GpsFileOpen == 0)
          {
            /* Time column is ThreadX ticks (see sensor header) */
            CHAR gps_header[] = "Time [10ms], UTC, Lat, Lon, Alt [m], Speed [km/h], Course [deg], Fix, NumSat, HDOP\r\n";
            sprintf(file_name, "Gps%03d.csv", SDCardCounter - 1);

            status = fx_file_create(&sdio_disk, file_name);
            if (status != FX_SUCCESS && status != FX_ALREADY_CREATED)
            {
              STBOX1_PRINTF("Error creating %s\r\n", file_name);
              Error_Handler(__FILE__, __LINE__);
            }

            status = fx_file_open(&sdio_disk, &GpsFxFile, file_name, FX_OPEN_FOR_WRITE);
            if (status != FX_SUCCESS)
            {
              STBOX1_PRINTF("Error opening %s\r\n", file_name);
              Error_Handler(__FILE__, __LINE__);
            }

            fx_file_seek(&GpsFxFile, 0);
            GpsFileOpen = 1;
            STBOX1_PRINTF("File %s open\r\n", file_name);

            fx_file_write(&GpsFxFile, gps_header, sizeof(gps_header) - 1);

            fx_media_flush(&sdio_disk);
            ErrorLog_Write("start: gps header written");

            /* Surface the outcome of GPS_Init()'s UBX-CFG-RATE / CFG-MSG /
               CFG-CFG commands. If rate!=OK the module will stay at its
               persisted rate (often 1 Hz) and the on-card Gps CSV will
               show 100-tick (=1 s) row spacing. Written once per START_LOG
               so the first Error_Log entry reflects the current boot. */
            {
              const char *gpslog = GPS_GetInitLog();
              if (gpslog && gpslog[0])
              {
                CHAR m[192];
                sprintf(m, "gps: %s", gpslog);
                ErrorLog_Write(m);
              }
            }
          }

#if STBOX1_LOG_BATTERY
          /* --- Battery: non-fatal. If STC3115 I2C init fails, log it and
             continue without battery log. Init runs once per boot; the
             BatteryInitAttempted flag keeps a stop/start cycle from
             re-probing a known-bad gauge. --- */
          if (!BatteryInitAttempted)
          {
            BatteryInitAttempted = 1;

            /* Diagnostic probe before the opaque ST driver call: the
               BSP_GG_Init path only returns COMPONENT_OK/ERROR without
               telling us where on the I2C path it failed. Capture:
               (1) bus init rc, (2) whether the STC3115 ACKs at 0xE0 at
               all, and (3) the HAL I2C error code so we can tell NAK
               (HAL_I2C_ERROR_AF=0x04, chip not answering) from BUS
               ERROR / TIMEOUT (wiring / pull-ups / stuck bus). */
            int32_t i2c4_rc = BSP_I2C4_Init();
            HAL_StatusTypeDef ping = HAL_I2C_IsDeviceReady(&hi2c4, 0xE0, 3, 200);
            uint32_t hal_err = HAL_I2C_GetError(&hi2c4);
            CHAR probe[96];
            sprintf(probe, "gauge: i2c4_init=%ld ping_0xE0=%s halerr=0x%08lX",
                    (long)i2c4_rc,
                    (ping == HAL_OK) ? "ACK" : "NAK",
                    (unsigned long)hal_err);
            ErrorLog_Write(probe);

            DrvStatusTypeDef gg_st = BSP_GG_Init(&HandleGGComponent);
            if (gg_st == COMPONENT_OK || gg_st == COMPONENT_BATT_FAIL)
            {
              BatteryInitOk = 1;
              ErrorLog_Write("start: gas gauge init ok");
            }
            else
            {
              BatteryInitOk = 0;
              ErrorLog_Write("start: gas gauge init FAIL - continuing without battery log");
            }
          }

          if (BatteryInitOk && BatteryFileOpen == 0)
          {
            /* Time column is ThreadX ticks (see sensor header) */
            CHAR bat_header[] = "Time [10ms], Voltage [mV], SOC [0.1%], Current [100uA]\r\n";
            sprintf(file_name, "Bat%03d.csv", SDCardCounter - 1);

            status = fx_file_create(&sdio_disk, file_name);
            if (status != FX_SUCCESS && status != FX_ALREADY_CREATED)
            {
              ErrorLog_Write("start: bat file create FAIL - skipping battery log");
            }
            else
            {
              status = fx_file_open(&sdio_disk, &BatteryFxFile, file_name, FX_OPEN_FOR_WRITE);
              if (status != FX_SUCCESS)
              {
                ErrorLog_Write("start: bat file open FAIL - skipping battery log");
              }
              else
              {
                fx_file_seek(&BatteryFxFile, 0);
                BatteryFileOpen = 1;
                fx_file_write(&BatteryFxFile, bat_header, sizeof(bat_header) - 1);
                fx_media_flush(&sdio_disk);

                /* Log starting voltage + SOC to the error log for quick
                   post-session visibility without opening the CSV. */
                uint8_t v_mode;
                uint32_t start_v = 0, start_soc = 0;
                BSP_GG_Task(HandleGGComponent, &v_mode);
                BSP_GG_GetVoltage(HandleGGComponent, &start_v);
                BSP_GG_GetSOC(HandleGGComponent, &start_soc);
                CHAR m[96];
                sprintf(m, "start: battery %lu mV %lu.%lu%%",
                        start_v, start_soc / 10, start_soc % 10);
                ErrorLog_Write(m);
              }
            }
          }
#else  /* STBOX1_LOG_BATTERY */
          ErrorLog_Write("start: battery disabled at compile time");
#endif /* STBOX1_LOG_BATTERY */

#if STBOX1_LOG_AUDIO
          /* --- Mic: non-fatal — if init/record fails, log it and
             continue without audio. Mic hardware has been observed to
             hang silently on some hardware-modified boards; we must
             not take the sensor/GPS logging down with it. --- */
          if (AudioFileOpen == 0)
          {
            ErrorLog_Write("start: mic init begin");

            BSP_AUDIO_Init_t MicParams;
            MicParams.BitsPerSample = 16;
            MicParams.ChannelsNbr = 1;
            MicParams.Device = ACTIVE_MICROPHONES_MASK;
            MicParams.SampleRate = AUDIO_IN_SAMPLING_FREQUENCY;
            MicParams.Volume = AUDIO_VOLUME_INPUT;

            if (BSP_AUDIO_IN_Init(0 /*BSP_AUDIO_IN_INSTANCE*/, &MicParams) != BSP_ERROR_NONE)
            {
              STBOX1_PRINTF("ERROR Initializing MIC\r\n");
              ErrorLog_Write("start: mic init FAIL - continuing without audio");
            }
            else
            {
              ErrorLog_Write("start: mic init ok");

              /* Only create the .wav file once the mic is confirmed alive.
                 Avoids leaving an orphan 44-byte WAV on the card. */
              sprintf(file_name, "Mic%03d.wav", SDCardCounter - 1);
              status =  fx_file_create(&sdio_disk, file_name);
              if (status != FX_SUCCESS && status != FX_ALREADY_CREATED)
              {
                STBOX1_PRINTF("Error creating %s \r\n", file_name);
                Error_Handler(__FILE__, __LINE__);
              }

              status =  fx_file_open(&sdio_disk, &AudioFxFile, file_name, FX_OPEN_FOR_WRITE);
              if (status != FX_SUCCESS)
              {
                STBOX1_PRINTF("Error opening MicXXX.wav\r\n");
                Error_Handler(__FILE__, __LINE__);
              }

              fx_file_seek(&AudioFxFile, 0);

              WavProcess_HeaderInit();
              status = fx_file_write(&AudioFxFile, pAudioHeader, sizeof(pAudioHeader));
              if (status != FX_SUCCESS)
              {
                STBOX1_PRINTF("Error writing MicXXX.wav\r\n");
                Error_Handler(__FILE__, __LINE__);
              }

              AudioFileOpen = 1;

              if (BSP_AUDIO_IN_Record(0 /*BSP_AUDIO_IN_INSTANCE*/, (uint8_t *) OnBoard_PCM_Buffer,
                                      PCM_AUDIO_IN_SAMPLES) != BSP_ERROR_NONE)
              {
                STBOX1_PRINTF("ERROR Starting MIC\r\n");
                ErrorLog_Write("start: mic record FAIL - closing .wav, continuing");
                BSP_AUDIO_IN_DeInit(0);
                fx_file_close(&AudioFxFile);
                AudioFileOpen = 0;
              }
              else
              {
                ErrorLog_Write("start: mic running");
                STBOX1_PRINTF("MIC Start\r\n");
              }
            }
            fx_media_flush(&sdio_disk);
          }
          else
          {
            STBOX1_PRINTF("Error MicXXX.wav already opened\r\n");
          }
#else  /* STBOX1_LOG_AUDIO */
          ErrorLog_Write("start: mic disabled at compile time");
#endif /* STBOX1_LOG_AUDIO */
        }
        break;
        case COMMAND_STOP_LOG:
        {
          if (SensorsFileOpen)
          {
            if (tx_timer_deactivate(&ReadTimer) != TX_SUCCESS)
            {
              Error_Handler(__FILE__, __LINE__);
            }

            /* Drain remaining sensor/GPS messages from queue before closing */
            {
              MessageData_t *DrainMsg;
              while (tx_queue_receive(&MessageQueue, &DrainMsg, TX_NO_WAIT) == TX_SUCCESS)
              {
                if (DrainMsg->CommandType == COMMAND_SAVE_SENSORS)
                {
                  CHAR data_s[256];
                  INT size;
                  size = sprintf(data_s, "%ld, %d, %d, %d, %d, %d, %d, %d, %d, %d, %5.2f, %5.2f\r\n",
                                 DrainMsg->MsgTime,
                                 DrainMsg->payload.sensors.acc.x, DrainMsg->payload.sensors.acc.y, DrainMsg->payload.sensors.acc.z,
                                 DrainMsg->payload.sensors.gyro.x, DrainMsg->payload.sensors.gyro.y, DrainMsg->payload.sensors.gyro.z,
                                 DrainMsg->payload.sensors.mag.x, DrainMsg->payload.sensors.mag.y, DrainMsg->payload.sensors.mag.z,
                                 DrainMsg->payload.sensors.pressure, DrainMsg->payload.sensors.temperature);
                  fx_file_write(&SensorsFxFile, data_s, size);
                }
                else if (DrainMsg->CommandType == COMMAND_SAVE_GPS && GpsFileOpen)
                {
                  CHAR data_s[256];
                  INT size;
                  GPS_Fix_t *g = &DrainMsg->payload.gps;
                  size = sprintf(data_s, "%ld, %s, %.7f, %.7f, %.2f, %.2f, %.2f, %u, %u, %.2f\r\n",
                                 DrainMsg->MsgTime, g->Utc, g->Lat, g->Lon,
                                 g->Alt, g->SpeedKmh, g->Course,
                                 g->Fix, g->NumSat, g->Hdop);
                  fx_file_write(&GpsFxFile, data_s, size);
                }
                MessageRemoved++;
              }
            }

            /* LED off to clearly signal logging stopped */
            BSP_LED_Off(LED_GREEN);

            /* Close the test file.  */
            status =  fx_file_close(&SensorsFxFile);

            /* Check the file close status.  */
            if (status != FX_SUCCESS)
            {
              /* Error closing the file, call error handler.  */
              STBOX1_PRINTF("Error closing SensXXX.csv\r\n");
              Error_Handler(__FILE__, __LINE__);
            }

            STBOX1_PRINTF("File SensXXX.csv closed\r\n");
            SensorsFileOpen = 0;
          }
          else
          {
            STBOX1_PRINTF("Error SensXXX.csv Not opened\r\n");
          }

          if (GpsFileOpen)
          {
            status = fx_file_close(&GpsFxFile);
            if (status != FX_SUCCESS)
            {
              STBOX1_PRINTF("Error closing GpsXXX.csv\r\n");
              Error_Handler(__FILE__, __LINE__);
            }
            STBOX1_PRINTF("File GpsXXX.csv closed\r\n");
            GpsFileOpen = 0;
          }

#if STBOX1_LOG_BATTERY
          if (BatteryFileOpen)
          {
            /* One final read for the end-of-session marker before close. */
            if (BatteryInitOk)
            {
              uint8_t  v_mode;
              uint32_t end_v = 0, end_soc = 0;
              BSP_GG_Task(HandleGGComponent, &v_mode);
              BSP_GG_GetVoltage(HandleGGComponent, &end_v);
              BSP_GG_GetSOC(HandleGGComponent, &end_soc);
              CHAR m[96];
              sprintf(m, "stop: battery %lu mV %lu.%lu%%",
                      end_v, end_soc / 10, end_soc % 10);
              ErrorLog_Write(m);
            }
            status = fx_file_close(&BatteryFxFile);
            if (status != FX_SUCCESS)
            {
              STBOX1_PRINTF("Error closing BatXXX.csv\r\n");
              Error_Handler(__FILE__, __LINE__);
            }
            STBOX1_PRINTF("File BatXXX.csv closed\r\n");
            BatteryFileOpen = 0;
          }
#endif /* STBOX1_LOG_BATTERY */

          if (AudioFileOpen)
          {
#if STBOX1_LOG_AUDIO
            if (BSP_AUDIO_IN_Stop(0 /*BSP_AUDIO_IN_INSTANCE*/) != BSP_ERROR_NONE)
            {
              STBOX1_PRINTF("ERROR Stopping MIC\r\n");
              ErrorLog_Write("stop: BSP_AUDIO_IN_Stop FAIL - continuing");
            }

            if (BSP_AUDIO_IN_DeInit(0 /*BSP_AUDIO_IN_INSTANCE*/) != BSP_ERROR_NONE)
            {
              STBOX1_PRINTF("ERROR De-Initializing MIC\r\n");
              ErrorLog_Write("stop: BSP_AUDIO_IN_DeInit FAIL - continuing");
            }
#endif /* STBOX1_LOG_AUDIO */

            STBOX1_PRINTF("MIC Stop\r\n");

            /* Flush the Queue for Sensors Data */
            status = tx_queue_flush(&MessageQueue);

            /* Update the  MicXXX.wav header */
            {
              /* size of the MicXXX.wav file */
              uint32_t len =  AudioFxFile.fx_file_current_file_size;

              WavProcess_HeaderUpdate(len);

              /* Move at the file beginning */
              status = fx_file_seek(&AudioFxFile, 0);
              if (status != FX_SUCCESS)
              {
                /* Error moving at the file beginning, call error handler.  */
                Error_Handler(__FILE__, __LINE__);
              }

              /* Write the updated Wav header  */
              status =  fx_file_write(&AudioFxFile, pAudioHeader, sizeof(pAudioHeader));
              if (status != FX_SUCCESS)
              {
                /* Error writing the file, call error handler.  */
                STBOX1_PRINTF("Error writing MicXXX.csv\r\n");
                Error_Handler(__FILE__, __LINE__);
              }

            }

            /* Close the test file.  */
            status =  fx_file_close(&AudioFxFile);

            /* Check the file close status.  */
            if (status != FX_SUCCESS)
            {
              /* Error closing the file, call error handler.  */
              STBOX1_PRINTF("Error closing MicXXX.csv\r\n");
              Error_Handler(__FILE__, __LINE__);
            }

            STBOX1_PRINTF("File MicXXX.wav closed\r\n");

            AudioFileOpen = 0;
          }
          else
          {
            STBOX1_PRINTF("MicXXX.wav not opened (mic init failed or disabled)\r\n");
          }

          /* Close error log + media AFTER all file closes, unconditionally.
             Previously the media_close was nested inside `if (AudioFileOpen)`;
             if mic init failed, Sens/Gps got closed but media stayed open and
             the SD card was left in an inconsistent state. */
          ErrorLog_Close();

          status =  fx_media_close(&sdio_disk);
          if (status != FX_SUCCESS)
          {
            STBOX1_PRINTF("Error closing SD FX Media\r\n");
            Error_Handler(__FILE__, __LINE__);
          }
          STBOX1_PRINTF("SD FX Media Closed\r\n");

          STBOX1_PRINTF("|--------------------|\r\n");
          STBOX1_PRINTF("| Queues summary:    |\r\n");
          STBOX1_PRINTF("|--------------------|\r\n");
          STBOX1_PRINTF("|   Queue Max: %4d  |\r\n", MessageMaxSize);
          STBOX1_PRINTF("|--------------------|\r\n");
        }
        break;
        case COMMAND_SAVE_AUDIO:
        {
          if (AudioFileOpen == 1)
          {
            if (ReadIndexBufferAudio != STBOX1_AUDIO_DATA_NOT_READY)
            {
              status =  fx_file_write(&AudioFxFile, (UCHAR *)(Audio_OUT_Buff + ReadIndexBufferAudio), AUDIO_BUFF_SIZE);
              /* Check the file write status.  */
              if (status != FX_SUCCESS)
              {
                /* Error writing to a file, call error handler.  */
                STBOX1_PRINTF("Error writing MicXXX.csv\r\n");
                Error_Handler(__FILE__, __LINE__);
              }
            }
          }
        }
        break;

        case COMMAND_SAVE_SENSORS:
        {
          /* Mode-switch (issue #14, 2026-05-10): in LOG mode, watch
           * for duration expiry. tx_time_get() returns ticks (10 ms
           * each). g_log_duration_seconds is what the iPhone app sent
           * via START command before reboot. When elapsed >= duration,
           * close everything cleanly + reboot back to BLE mode. */
          if (g_app_mode == APP_MODE_LOG && g_log_duration_seconds > 0U)
          {
            uint32_t elapsed_seconds = tx_time_get() / 100U;
            if (elapsed_seconds >= g_log_duration_seconds)
            {
              ErrorLog_Write("mode: LOG duration expired - flushing + reboot to BLE");
              ErrorLog_Flush();
              if (SensorsFileOpen)  { (void)fx_file_close(&SensorsFxFile); SensorsFileOpen = 0; }
              if (GpsFileOpen)      { (void)fx_file_close(&GpsFxFile);     GpsFileOpen = 0; }
              if (BatteryFileOpen)  { (void)fx_file_close(&BatteryFxFile); BatteryFileOpen = 0; }
              (void)fx_media_flush(&sdio_disk);
              (void)fx_media_close(&sdio_disk);
              /* Clear next-boot mode → defaults to BLE on reboot. */
              TAMP->BKP1R = 0U;
              TAMP->BKP2R = 0U;
              __DSB();
              NVIC_SystemReset();
              for (;;) { }  /* unreachable */
            }
          }

          if (SensorsFileOpen)
          {
            CHAR data_s[256];
            INT size;
            static uint32_t flush_counter = 0;
            size = sprintf(data_s, "%ld, %d, %d, %d, %d, %d, %d, %d, %d, %d, %5.2f, %5.2f\r\n",
                           RMsg->MsgTime,
                           RMsg->payload.sensors.acc.x, RMsg->payload.sensors.acc.y, RMsg->payload.sensors.acc.z,
                           RMsg->payload.sensors.gyro.x, RMsg->payload.sensors.gyro.y, RMsg->payload.sensors.gyro.z,
                           RMsg->payload.sensors.mag.x, RMsg->payload.sensors.mag.y, RMsg->payload.sensors.mag.z,
                           RMsg->payload.sensors.pressure, RMsg->payload.sensors.temperature);

            /* Write a string to the test file.  */
            status =  fx_file_write(&SensorsFxFile, data_s, size);

            /* Check the file write status.  */
            if (status != FX_SUCCESS)
            {
              /* Error writing to a file, call error handler.  */
              STBOX1_PRINTF("Error writing SensXXX.csv\r\n");
              Error_Handler(__FILE__, __LINE__);
            }

            /* Periodic flush so FAT directory entries (file sizes) stay
             * up-to-date. Without the user button wired up there is no
             * graceful stop — power-off at any time must still leave
             * playable files. Flush every ~1 s (100 samples @ 100 Hz)
             * covers Sens/Mic/Gps since fx_media_flush is media-wide.
             * Update clock before flush so directory-entry timestamps
             * advance with wall-clock time. */
            if (++flush_counter >= 100)
            {
              flush_counter = 0;
              UpdateFileXClock();

#if STBOX1_LOG_BATTERY
              /* One battery row per ~1 s, written here so it ends up in
                 the same fx_media_flush as the sensor FAT directory
                 update — an ungraceful power-off still leaves the battery
                 CSV valid up to the last flush. */
              if (BatteryFileOpen && BatteryInitOk)
              {
                uint8_t  v_mode;
                uint32_t bv = 0, bsoc = 0;
                int32_t  bcur = 0;
                BSP_GG_Task(HandleGGComponent, &v_mode);
                BSP_GG_GetVoltage(HandleGGComponent, &bv);
                BSP_GG_GetSOC(HandleGGComponent, &bsoc);
                BSP_GG_GetCurrent(HandleGGComponent, &bcur);
                CHAR  bline[96];
                INT   blen = sprintf(bline, "%ld, %lu, %lu, %ld\r\n",
                                     RMsg->MsgTime, bv, bsoc, bcur);
                fx_file_write(&BatteryFxFile, bline, blen);
              }
#endif /* STBOX1_LOG_BATTERY */

              fx_media_flush(&sdio_disk);

#if STBOX1_ENABLE_BLE_SYNC
              /* v37 diagnostic: log BLE probe-status changes via the
                 1-Hz periodic flush. ble_sync_thread sets g_ble_probe_status
                 to 0/1/2 after probe + bluetooth_init complete; if its
                 ErrorLog_Write calls happened before ErrorLog_Open
                 (early boot race), they were dropped. This re-emits the
                 outcome on every change so we see it land. Issue #12. */
              extern volatile uint8_t g_ble_probe_status;
              static uint8_t last_logged_probe = 0xFEU;  /* != 0xFF so first 0xFF gets logged */
              if (g_ble_probe_status != last_logged_probe) {
                CHAR pmsg[100];
                const char *meaning = "?";
                switch (g_ble_probe_status) {
                  case 0x00: meaning = "probe FAILED"; break;
                  case 0x01: meaning = "init OK, advertising"; break;
                  case 0x02: meaning = "probe OK, bluetooth_init FAILED"; break;
                  case 0xF0: meaning = "thread entered"; break;
                  case 0xF1: meaning = "stuck in ble_chip_alive_probe (or HCI_Reset send)"; break;
                  case 0xF2: meaning = "stuck in bluetooth_init (init_ble_manager)"; break;
                  case 0xF3: meaning = "bluetooth_init returned (transient)"; break;
                  case 0xF4: meaning = "stuck in init_ble_int_for_blue_nrglp"; break;
                  case 0xFF: meaning = "thread never started"; break;
                  default:   meaning = "unknown"; break;
                }
                sprintf(pmsg, "ble: probe_status changed 0x%02X -> 0x%02X (%s)",
                        (unsigned)last_logged_probe,
                        (unsigned)g_ble_probe_status, meaning);
                ErrorLog_Write(pmsg);
                last_logged_probe = g_ble_probe_status;
              }
#endif
            }
          }
          BSP_LED_Toggle(LED_GREEN);
        }
        break;

        case COMMAND_SAVE_GPS:
        {
          if (GpsFileOpen)
          {
            CHAR data_s[256];
            INT size;
            GPS_Fix_t *g = &RMsg->payload.gps;
            size = sprintf(data_s, "%ld, %s, %.7f, %.7f, %.2f, %.2f, %.2f, %u, %u, %.2f\r\n",
                           RMsg->MsgTime, g->Utc, g->Lat, g->Lon,
                           g->Alt, g->SpeedKmh, g->Course,
                           g->Fix, g->NumSat, g->Hdop);

            status = fx_file_write(&GpsFxFile, data_s, size);
            if (status != FX_SUCCESS)
            {
              STBOX1_PRINTF("Error writing GpsXXX.csv\r\n");
              Error_Handler(__FILE__, __LINE__);
            }
          }
        }
        break;

        default:
          STBOX1_PRINTF("Command =%d Not recognized\r\n", RMsg->CommandType);
          Error_Handler(__FILE__, __LINE__);
      }
    }
  }
}

/**
  * @brief  BSP Push Button callback
  *
  * @param  Button Specifies the pin connected EXTI line
  * @retval None
  */
void BSP_PB_Callback(Button_TypeDef Button)
{
  if (Button == BUTTON_USER)
  {
    /* Set the User Button flag */
    UserButtonPressed = SET;
    /* Release the Semaphore */
    tx_semaphore_put(&SemaphorePtr);
  }
}

/**
  * @brief  Post a STOP_LOG message into the FileX writer queue from the
  *         BLE thread. Owned MessageData_t (separate from the read-thread
  *         ring buffer) keeps button + BLE stops from racing on the same
  *         storage. Idempotent — second call while already stopped is a
  *         no-op because COMMAND_STOP_LOG is itself idempotent.
  * @retval None
  */
void Ble_RequestStopLog(void)
{
  static MessageData_t  ble_stop_msg;
  static MessageData_t *ble_stop_msg_ptr = &ble_stop_msg;
  ble_stop_msg.CommandType = COMMAND_STOP_LOG;
  /* Don't block the BLE thread; if the queue is momentarily full, the
     host can resend STOP_LOG on the next opcode. */
  (void)tx_queue_send(&MessageQueue, &ble_stop_msg_ptr, TX_NO_WAIT);
}

int Ble_IsLoggingActive(void)
{
  return (SensorsFileOpen || GpsFileOpen) ? 1 : 0;
}

/**
  * @brief  Build error log filename from compile date
  *         Format: Error_Log_Pump_Tsueri_dd.mm.yyyy.log
  * @param  buf: output buffer (at least 45 bytes)
  * @retval None
  */
static void ErrorLog_BuildFilename(CHAR *buf)
{
  /* __DATE__ = "Mar  9 2026" */
  const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                          "Jul","Aug","Sep","Oct","Nov","Dec"};
  const char *d = __DATE__;
  int month = 1, day, year;

  for (int i = 0; i < 12; i++)
  {
    if (d[0] == months[i][0] && d[1] == months[i][1] && d[2] == months[i][2])
    {
      month = i + 1;
      break;
    }
  }
  day = (d[4] == ' ') ? (d[5] - '0') : ((d[4] - '0') * 10 + (d[5] - '0'));
  year = (d[7] - '0') * 1000 + (d[8] - '0') * 100 + (d[9] - '0') * 10 + (d[10] - '0');

  sprintf(buf, "Error_Log_Pump_Tsueri_%02d.%02d.%04d.log", day, month, year);
}

/**
  * @brief  Initialise the wall-clock base from the compile date/time.
  *         Called once during MX_FileX_Init so UpdateFileXClock has a
  *         sensible base before the first GPS fix arrives. Overridden
  *         later by SetClockBaseFromGPS when $GNRMC delivers real UTC.
  * @retval None
  */
static void InitClockBaseFromCompileDate(void)
{
  const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                          "Jul","Aug","Sep","Oct","Nov","Dec"};
  const char *d = __DATE__;
  const char *t = __TIME__;
  UINT m = 1;
  for (UINT i = 0; i < 12; i++)
  {
    if (d[0] == months[i][0] && d[1] == months[i][1] && d[2] == months[i][2])
    {
      m = i + 1;
      break;
    }
  }
  ClockBaseYear  = (UINT)((d[7]-'0')*1000 + (d[8]-'0')*100 + (d[9]-'0')*10 + (d[10]-'0'));
  ClockBaseMonth = m;
  ClockBaseDay   = (d[4] == ' ') ? (UINT)(d[5] - '0') : (UINT)((d[4] - '0') * 10 + (d[5] - '0'));
  ClockBaseHour  = (UINT)((t[0]-'0')*10 + (t[1]-'0'));
  ClockBaseMin   = (UINT)((t[3]-'0')*10 + (t[4]-'0'));
  ClockBaseSec   = (UINT)((t[6]-'0')*10 + (t[7]-'0'));
  ClockBaseTick  = 0;
}

/**
  * @brief  Overwrite the wall-clock base with GPS UTC. Called by gps_thread
  *         once $GNRMC delivers a valid date+time, so Sens/Gps/Bat files
  *         created (or flushed) afterwards get today's real FAT timestamp
  *         instead of the stale compile date.
  * @retval None
  */
static void SetClockBaseFromGPS(UINT year, UINT month, UINT day,
                                UINT hour, UINT minute, UINT second)
{
  ClockBaseYear  = year;
  ClockBaseMonth = month;
  ClockBaseDay   = day;
  ClockBaseHour  = hour;
  ClockBaseMin   = minute;
  ClockBaseSec   = second;
  ClockBaseTick  = tx_time_get();
  ClockSeededFromGPS = 1;
}

/**
  * @brief  Set FileX's system date/time from (base + ticks-since-base).
  *         Base is the compile date at boot, or the most recent GPS UTC
  *         if a fix has been seen. Files land with approximately wall-clock
  *         timestamps instead of FAT's default 31.12.16. Month/year
  *         rollover past the base date is not handled.
  * @retval None
  */
static void UpdateFileXClock(void)
{
  UINT year = ClockBaseYear;
  UINT m    = ClockBaseMonth;
  UINT day  = ClockBaseDay;
  UINT hour = ClockBaseHour;
  UINT min  = ClockBaseMin;
  UINT sec  = ClockBaseSec;

  ULONG elapsed = (tx_time_get() - ClockBaseTick) / 100;  /* ticks → seconds */
  sec  += (UINT)(elapsed % 60);
  min  += (UINT)(elapsed / 60);
  if (sec >= 60) { min += sec / 60; sec %= 60; }
  hour += min / 60;
  min  %= 60;
  day  += hour / 24;
  hour %= 24;
  /* Month/year rollover intentionally skipped. */

  fx_system_date_set(year, m, day);
  fx_system_time_set(hour, min, sec);
}

/**
  * @brief  Open error log file on SD card (append mode)
  *         Called after SD media is opened
  * @retval None
  */
static void ErrorLog_Open(void)
{
  UINT status;
  CHAR log_name[50];

  if (ErrorLogFileOpen)
    return;

  ErrorLog_BuildFilename(log_name);

  /* Create if not existing */
  status = fx_file_create(&sdio_disk, log_name);
  if (status != FX_SUCCESS && status != FX_ALREADY_CREATED)
  {
    STBOX1_PRINTF("Error creating %s\r\n", log_name);
    return;
  }

  /* Open for write */
  status = fx_file_open(&sdio_disk, &ErrorLogFxFile, log_name, FX_OPEN_FOR_WRITE);
  if (status != FX_SUCCESS)
  {
    STBOX1_PRINTF("Error opening %s\r\n", log_name);
    return;
  }

  /* Seek to end for append */
  fx_file_seek(&ErrorLogFxFile, ErrorLogFxFile.fx_file_current_file_size);

  ErrorLogFileOpen = 1;

  /* Write boot marker + firmware fingerprint so we can tell post-session
     exactly which binary produced the data. Flash footprint is computed
     from linker symbols: _sidata is the Flash load-address of .data (the
     last thing placed in Flash), so _sidata + sizeof(.data) is the total
     flash used by the application. */
  extern uint32_t _sidata, _sdata, _edata;
  ULONG flash_end_addr = (ULONG)&_sidata + ((ULONG)&_edata - (ULONG)&_sdata);
  ULONG flash_kb = (flash_end_addr - 0x08000000UL + 1023) / 1024;

  extern uint32_t BootResetCsr;  /* snapshotted in main() after HAL_Init() */

  CHAR boot_msg[256];
  INT len = sprintf(boot_msg, "--- Boot v%u %s %s ---\r\n",
                    (unsigned)FW_BUILD_NUM, __DATE__, __TIME__);
  fx_file_write(&ErrorLogFxFile, boot_msg, len);
  len = sprintf(boot_msg,
    "fw: v%u (" DIAG_VERSION ") build %s %s | GPS %uHz | AUDIO=%d BATTERY=%d | flash ~%luKB\r\n",
    (unsigned)FW_BUILD_NUM,
    __DATE__, __TIME__,
    (unsigned)GPS_RATE_HZ,
    STBOX1_LOG_AUDIO, STBOX1_LOG_BATTERY,
    (unsigned long)flash_kb);
  fx_file_write(&ErrorLogFxFile, boot_msg, len);

  /* Reset-reason stamp. BootResetCsr was snapshotted in main() right after
     HAL_Init(). Multiple flags can be set simultaneously (e.g. BOR+PIN on
     a clean power-up from dead batteries), so we list all that are present.
     POR is inferred: the STM32U5 sets BORRSTF on every power-on as well as
     on brown-out, but on a true cold boot PINRSTF is also set alongside it
     — on a brown-out mid-session, PINRSTF is absent. */
  CHAR reason[80] = "";
  INT rlen = 0;
  if (BootResetCsr & RCC_CSR_LPWRRSTF) rlen += sprintf(reason+rlen, " LPWR");
  if (BootResetCsr & RCC_CSR_WWDGRSTF) rlen += sprintf(reason+rlen, " WWDG");
  if (BootResetCsr & RCC_CSR_IWDGRSTF) rlen += sprintf(reason+rlen, " IWDG");
  if (BootResetCsr & RCC_CSR_SFTRSTF)  rlen += sprintf(reason+rlen, " SOFTWARE");
  if (BootResetCsr & RCC_CSR_BORRSTF) {
    if (BootResetCsr & RCC_CSR_PINRSTF) rlen += sprintf(reason+rlen, " POR");
    else                                rlen += sprintf(reason+rlen, " BOR");
  } else if (BootResetCsr & RCC_CSR_PINRSTF) {
    rlen += sprintf(reason+rlen, " PIN");
  }
  if (BootResetCsr & RCC_CSR_OBLRSTF) rlen += sprintf(reason+rlen, " OBL");
  if (rlen == 0) sprintf(reason, " UNKNOWN");
  len = sprintf(boot_msg, "reset:%s (CSR=0x%08lX)\r\n",
                reason, (unsigned long)BootResetCsr);
  fx_file_write(&ErrorLogFxFile, boot_msg, len);

  /* BLE chip-alive probe status. ble_sync_thread runs at lower priority
     than fx_thread so by the time we get here it has either finished
     the probe (status set to 0/1/2) or is still mid-handshake (status
     still 0xFF). The status drives a one-line marker so the field
     tester / post-session analyzer can tell whether BLE FileSync is
     available without having to scan from the GUI. */
#if STBOX1_ENABLE_BLE_SYNC
  extern volatile uint8_t g_ble_probe_status;
  const char *ble_msg;
  switch (g_ble_probe_status)
  {
    case 0U:    ble_msg = "ble: chip-alive probe FAILED - BLE disabled this session\r\n"; break;
    case 1U:    ble_msg = "ble: chip alive, init ok\r\n"; break;
    case 2U:    ble_msg = "ble: chip alive but init_ble_manager FAILED - BLE disabled\r\n"; break;
    default:    ble_msg = "ble: probe still pending at error-log open\r\n"; break;
  }
  fx_file_write(&ErrorLogFxFile, (CHAR *)ble_msg, strlen(ble_msg));
#endif

  /* v59: dump the diagnostic ring buffer from the previous boot. If
     fx_thread got stuck somewhere in COMMAND_START_LOG, the ring
     captured exactly where. We write the entries as a single batched
     sequence into the FileX cache; the trailing fx_media_flush at
     the bottom of ErrorLog_Open commits them all in one transaction.
     If magic doesn't match, .noinit was uninitialised (cold boot) —
     skip silently. */
  {
    extern diag_ring_t g_diag_ring;
    if (g_diag_ring.magic == DIAG_RING_MAGIC)
    {
      uint32_t count = g_diag_ring.wrap ? DIAG_RING_SIZE : g_diag_ring.head;
      uint32_t start = g_diag_ring.wrap ? g_diag_ring.head : 0U;
      CHAR rmsg[80];
      INT rl = sprintf(rmsg, "DIAG_RING_DUMP: %lu entries, last from previous boot:\r\n",
                       (unsigned long)count);
      fx_file_write(&ErrorLogFxFile, rmsg, rl);
      for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (start + i) % DIAG_RING_SIZE;
        rl = sprintf(rmsg, "  [%2lu] tick=%lu code=0x%06lX\r\n",
                     (unsigned long)i,
                     (unsigned long)g_diag_ring.entries[idx].tick,
                     (unsigned long)g_diag_ring.entries[idx].code);
        fx_file_write(&ErrorLogFxFile, rmsg, rl);
      }
      /* Reset the ring for THIS boot's checkpoints. */
      g_diag_ring.magic = DIAG_RING_MAGIC;
      g_diag_ring.head  = 0;
      g_diag_ring.wrap  = 0;
    }
  }

  /* v57: post-mortem fault info recovery. g_fault_info lives in .noinit
     so it survives warm reset (NVIC reset, watchdog, brown-out). If a
     fault occurred on the previous boot — magic == 0xFA0177AB — emit
     the captured PC/LR/CFSR as a permanent on-disk record, then clear
     the magic so this only fires once per fault. Strings match the
     LED-morse pattern numbers from stm32u5xx_it.c so post-session
     correlation is trivial. */
  {
    extern fault_info_t g_fault_info;
    if (g_fault_info.magic == 0xFA0177ABU)
    {
      static const char *FAULT_NAME[] = {
        "?", "HardFault", "MemManage", "BusFault", "UsageFault", "NMI"
      };
      const char *name = (g_fault_info.fault_id <= 5U)
                       ? FAULT_NAME[g_fault_info.fault_id] : "?";
      CHAR fmsg[256];
      INT flen = sprintf(fmsg,
        "FAULT_RECOVERED: %s (id=%lu morse=%lu pulses)\r\n"
        "  PC=0x%08lX LR=0x%08lX PSR=0x%08lX\r\n"
        "  CFSR=0x%08lX HFSR=0x%08lX MMFAR=0x%08lX BFAR=0x%08lX\r\n",
        name,
        (unsigned long)g_fault_info.fault_id,
        (unsigned long)(g_fault_info.fault_id + 4U),  /* morse offset */
        (unsigned long)g_fault_info.pc,
        (unsigned long)g_fault_info.lr,
        (unsigned long)g_fault_info.psr,
        (unsigned long)g_fault_info.cfsr,
        (unsigned long)g_fault_info.hfsr,
        (unsigned long)g_fault_info.mmfar,
        (unsigned long)g_fault_info.bfar);
      fx_file_write(&ErrorLogFxFile, fmsg, flen);
      g_fault_info.magic = 0;  /* clear so we don't re-log next boot */
    }
  }

  fx_media_flush(&sdio_disk);

  STBOX1_PRINTF("Error log: %s\r\n", log_name);
}

/**
  * @brief  Write a message to the error log file
  * @param  msg: null-terminated string to log
  * @retval None
  */
/* ErrorLog_Write no longer flushes after every line. The per-write flush
   pattern was observed to hang on Peter's 3.3V-modded box right after
   ErrorLog_Open's batched flush succeeded — likely an SD-card post-write
   busy state the SDMMC driver mishandles. Accumulate writes in the FileX
   cache instead; explicit ErrorLog_Flush() calls (or any other operation
   that triggers a flush, like fx_file_create) commit them to disk in
   the same many-writes-one-flush pattern that ErrorLog_Open uses
   successfully. Cost of this: if the firmware hangs between flushes,
   markers since the last flush are lost — but in practice we get more
   markers on disk, not fewer, because we hit the hanging flush less
   often. */
/* v52: track ErrorLog write failures so we don't spend 10 s × N writes
   busy-waiting for a stuck SD card. After 2 consecutive failures, set
   the broken flag and silently no-op subsequent writes. The flag stays
   set for the rest of the boot — there's no "card recovered" detection.
   Trade-off: if the card is genuinely transient-busy and clears, we
   still skip writes for the rest of the session — but that beats
   monopolising fx_thread's CPU on a card that's not coming back. */
static UINT ErrorLogConsecutiveFails = 0U;
static UINT ErrorLogBroken = 0U;

/* Issue #14 (2026-05-10): when BLE thread suspends fx_thread for the
 * BLE bring-up window, fx_thread might be holding FileX media mutex.
 * Any fx_file_* call from BLE thread would then deadlock. Set this
 * flag from BLE thread around the suspend → ErrorLog_Write/Flush
 * skip the SD path (USB CDC mirror still works). */
volatile uint8_t g_ble_init_skip_fx = 0;

void ErrorLog_Write(const char *msg)
{
  /* USB CDC mirror: every SD-bound error line also streams over the
   * virtual COM port so a developer plugged in via USB-C sees the same
   * trace live without having to pop the SD card. Goes through
   * UsbCdc_Write which is non-blocking — drops bytes silently if the
   * host hasn't opened the port. Done BEFORE the SD write so we still
   * get visibility when ErrorLogBroken or !ErrorLogFileOpen kills the
   * SD path. The "log: " prefix lets the host distinguish error-log
   * lines from heartbeat / probe printfs in the cat stream. */
  {
    CHAR usb_line[300];
    INT  usb_len = sprintf(usb_line, "log: %s\r\n", msg);
    (void) UsbCdc_Write(usb_line, (size_t) usb_len);
  }

  if (g_ble_init_skip_fx) return;  /* deadlock-avoidance during BLE init */
  if (!ErrorLogFileOpen) return;
  if (ErrorLogBroken) return;

  CHAR line[300];
  INT len = sprintf(line, "[%lu ms] %s\r\n", tx_time_get(), msg);
  UINT status = fx_file_write(&ErrorLogFxFile, line, len);

  if (status != FX_SUCCESS)
  {
    ErrorLogConsecutiveFails++;
    if (ErrorLogConsecutiveFails >= 2U)
    {
      ErrorLogBroken = 1U;  /* stop trying — card is not recovering */
    }
  }
  else
  {
    ErrorLogConsecutiveFails = 0U;
  }
}

/* Explicit flush — call at strategic checkpoints. Returns the FileX
   status so callers can decide whether to give up. */
UINT ErrorLog_Flush(void)
{
  if (g_ble_init_skip_fx) return FX_SUCCESS;  /* deadlock-avoidance */
  if (!ErrorLogFileOpen)
    return FX_SUCCESS;
  return fx_media_flush(&sdio_disk);
}

/**
  * @brief  Close error log file and flush SD media
  * @retval None
  */
void ErrorLog_Close(void)
{
  if (!ErrorLogFileOpen)
    return;

  fx_file_close(&ErrorLogFxFile);
  ErrorLogFileOpen = 0;
}

/**
  * @brief  Write FW_INFO_v<N>.TXT at the SD-card root with the current
  *         firmware fingerprint, where <N> is FW_BUILD_NUM. Field tester
  *         can see which firmware is on the box from the SD listing
  *         alone — no need to open any file. Old FW_INFO* files (the
  *         legacy fixed-name FW_INFO.TXT and any older FW_INFO_v*.TXT
  *         from previously-flashed firmware) are deleted on every boot
  *         so the listing stays clean.
  *
  *         Same content as the boot marker in the error log (build
  *         date/time, GPS rate, feature flags, flash footprint).
  *
  *         Caller must already have opened sdio_disk.
  * @retval None
  */
/* v66: SD-resident checkpoint markers. Each call writes a tiny
   CHK<n>.TXT file at SD root. The last file present after a hang
   pinpoints which checkpoint was the last one reached. Set
   need_open=1 outside the CAFU region (where sdio_disk is closed) so
   the helper opens+closes media around the write; set need_open=0
   when the caller is already holding the media open. CHK files are
   wiped at every boot via the directory cleanup in WriteFwInfoFile. */
static void WriteCheckpointFile(int chk_num, int need_open)
{
  if (need_open) {
    /* v70: sleep before re-opening media. v69 with 50 ticks (500 ms)
       wasn't enough — CHK04 still didn't land on Zeno's box. Bumped to
       200 ticks (2 sec) to test if it's just a timing problem. Cost is
       4× 2 = 8 sec extra boot time across all CHK4..7 — tolerable for
       diagnostic. If 2 sec still doesn't work, the theory is wrong and
       we need to investigate fx_media_open/close pairing differently
       (e.g., never close media, use BSP_SD_GetCardState polling, or
       skip the open/close entirely by keeping media open throughout). */
    tx_thread_sleep(200);
    UINT s = fx_media_open(&sdio_disk, "STM32_SDIO_DISK", fx_stm32_sd_driver,
                           0, (VOID *)media_memory, sizeof(media_memory));
    if (s != FX_SUCCESS) return;
  }

  FX_FILE chk;
  CHAR name[16];
  CHAR data[40];

  sprintf(name, "CHK%02d.TXT", chk_num);
  fx_file_create(&sdio_disk, name);
  if (fx_file_open(&sdio_disk, &chk, name, FX_OPEN_FOR_WRITE) == FX_SUCCESS) {
    int n = sprintf(data, "ck%d t=%lu\r\n",
                    chk_num, (unsigned long)tx_time_get());
    fx_file_write(&chk, data, n);
    fx_file_close(&chk);
    /* v68 strategy A: NO explicit fx_media_flush. When need_open=0
       (CHK inside CAFU), the file lives in cache and is committed by
       CAFU's fx_media_close. When need_open=1 (CHK in fx_thread),
       the fx_media_close below provides one internal flush. */
  }

  if (need_open) {
    fx_media_close(&sdio_disk);
  }
}

static void WriteFwInfoFile(void)
{
  FX_FILE info_file;
  CHAR info[200];
  CHAR fw_info_name[24];

  extern uint32_t _sidata, _sdata, _edata;
  ULONG flash_end_addr = (ULONG)&_sidata + ((ULONG)&_edata - (ULONG)&_sdata);
  ULONG flash_kb = (flash_end_addr - 0x08000000UL + 1023) / 1024;

  sprintf(fw_info_name, "FW_INFO_v%u.TXT", (unsigned)FW_BUILD_NUM);

  INT len = sprintf(info,
    "fw: v%u (" DIAG_VERSION ") build %s %s\r\nGPS %uHz | AUDIO=%d BATTERY=%d | flash ~%luKB\r\n",
    (unsigned)FW_BUILD_NUM,
    __DATE__, __TIME__,
    (unsigned)GPS_RATE_HZ,
    STBOX1_LOG_AUDIO, STBOX1_LOG_BATTERY,
    (unsigned long)flash_kb);

  /* Walk the SD root and collect every FW_INFO* filename we find, then
     delete them all. Two-pass so the directory iterator isn't
     invalidated mid-iteration. The legacy fixed-name FW_INFO.TXT and
     any FW_INFO_v*.TXT from previously-flashed firmware both match the
     7-char prefix. v66 also deletes CHK*.TXT diagnostic markers from
     the previous boot — fresh per-boot checkpoint set. Up to 16 stale
     entries cleaned per boot. */
  {
    CHAR  stale[16][24];
    UINT  count = 0;
    CHAR  name[24];

    if (fx_directory_first_entry_find(&sdio_disk, name) == FX_SUCCESS)
    {
      do {
        if (count < 16 &&
            (strncmp(name, "FW_INFO", 7) == 0 ||
             strncmp(name, "CHK", 3) == 0)) {
          strncpy(stale[count], name, 23);
          stale[count][23] = '\0';
          count++;
        }
      } while (fx_directory_next_entry_find(&sdio_disk, name) == FX_SUCCESS);
    }

    for (UINT i = 0; i < count; i++) {
      fx_file_delete(&sdio_disk, stale[i]);
    }
  }

  if (fx_file_create(&sdio_disk, fw_info_name) != FX_SUCCESS)
    return;
  if (fx_file_open(&sdio_disk, &info_file, fw_info_name, FX_OPEN_FOR_WRITE)
      != FX_SUCCESS)
    return;

  fx_file_write(&info_file, info, len);
  fx_file_close(&info_file);
  /* v68 strategy A: NO explicit fx_media_flush here. Back-to-back
     flushes hang the driver on this hardware (issue #12 / Peter's box
     reproduced locally on Zeno's box too). The trailing fx_media_close
     in CheckAndApplyFirmwareUpdate provides a single internal flush
     that commits FW_INFO + all CHK files in one shot. */
  /* v66 CHK01: WriteFwInfoFile completed (media still open). */
  WriteCheckpointFile(1, 0);
}

/**
  * @brief  Check SD card for firmware.bin and flash it to the other bank
  *
  * On boot, opens the SD card and looks for "firmware.bin". If found:
  *   1. Detects current flash bank (option bytes)
  *   2. Erases the other bank
  *   3. Programs firmware.bin to the other bank (16-byte aligned writes)
  *   4. Renames firmware.bin → firmware.done
  *   5. Swaps banks via option bytes and resets
  *
  * To update firmware: copy firmware.bin to SD card, power-cycle the device.
  * The red LED blinks during the update. If no firmware.bin is found,
  * normal SD logging continues.
  *
  * Always writes FW_INFO.TXT before the firmware-update check, so the
  * field tester can see which firmware is actually running on every
  * boot — including after a failed bank-swap update where the chip
  * keeps booting from the old bank.
  *
  * @retval 0 = no update found, continues normally
  *         Does not return on successful update (system reset)
  */
static INT CheckAndApplyFirmwareUpdate(void)
{
  UINT status;
  FX_FILE fw_file;
  ULONG fw_size;
  ULONG bytes_read;

  /* Open SD card media */
  status = fx_media_open(&sdio_disk, "STM32_SDIO_DISK", fx_stm32_sd_driver,
                         0, (VOID *)media_memory, sizeof(media_memory));
  if (status != FX_SUCCESS)
  {
    /* No SD card or media error — skip update, continue to logging */
    return 0;
  }

  /* Write/overwrite FW_INFO.TXT so the currently-running firmware is
     visible immediately after boot — before any START_LOG, even if
     the user never starts a session. */
  WriteFwInfoFile();
  /* v66 CHK02: WriteFwInfoFile returned, before fx_file_open(firmware.bin). */
  WriteCheckpointFile(2, 0);

  /* Try to open firmware.bin */
  status = fx_file_open(&sdio_disk, &fw_file, "firmware.bin", FX_OPEN_FOR_READ);
  if (status != FX_SUCCESS)
  {
    /* v66 CHK03: firmware.bin not found, about to fx_media_close. */
    WriteCheckpointFile(3, 0);
    /* v72: SKIP fx_media_close. v68-71 confirmed that fx_media_close
       commits data to disk (CHK01-03 visible) but then hangs in its
       internal cleanup phase — fx_thread never reaches BSP_LED_On.
       Leaving media open lets fx_thread continue, and the
       COMMAND_START_LOG case can either reuse this open media or do
       its own open. CHK files in cache will probably NOT be visible
       on next read since they're not committed — that's the cost of
       the diagnostic. Issue #12. */
    /* fx_media_close(&sdio_disk); */
    return 0;
  }

  fw_size = (ULONG)fw_file.fx_file_current_file_size;
  STBOX1_PRINTF("\r\n*** SD Firmware Update ***\r\n");
  STBOX1_PRINTF("Found firmware.bin (%lu bytes)\r\n", fw_size);

  /* Distinct "firmware detected" signal: alternate green+red 10x rapid,
     ~2 s total. Easy to distinguish from the normal 1/2/3 green-blink
     boot sequence — tells the field tester immediately that an SD-based
     update was picked up (vs "no firmware.bin, boot straight to logging"). */
  for (int i = 0; i < 10; i++) {
    BSP_LED_On(LED_GREEN);
    BSP_LED_Off(LED_RED);
    HAL_Delay(100);
    BSP_LED_Off(LED_GREEN);
    BSP_LED_On(LED_RED);
    HAL_Delay(100);
  }
  BSP_LED_Off(LED_GREEN);
  BSP_LED_Off(LED_RED);

  if (fw_size == 0 || fw_size > (1024 * 1024 - 8192))
  {
    /* Empty or too large (max ~1016 KB, leave room for FW ID page) */
    STBOX1_PRINTF("ERROR: Invalid firmware size\r\n");
    fx_file_close(&fw_file);
    fx_media_close(&sdio_disk);
    return 0;
  }

  /* Detect current active bank from option bytes */
  int32_t CurrentBank;
  {
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();
    HAL_FLASHEx_OBGetConfig(&OBInit);
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    if ((OBInit.USERConfig & OB_SWAP_BANK_ENABLE) == OB_SWAP_BANK_ENABLE)
      CurrentBank = 2;
    else
      CurrentBank = 1;
  }
  STBOX1_PRINTF("Current bank: %ld, writing to bank %ld\r\n",
                CurrentBank, (CurrentBank == 1) ? 2L : 1L);

  /* Erase the other bank */
  STBOX1_PRINTF("Erasing target bank...\r\n");
  BSP_LED_On(LED_RED);
  {
    FLASH_EraseInitTypeDef EraseInit;
    uint32_t SectorError = 0;
    uint32_t num_pages = (fw_size + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;

    EraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInit.Banks = (CurrentBank == 1) ? FLASH_BANK_2 : FLASH_BANK_1;
    EraseInit.Page = 0;
    EraseInit.NbPages = num_pages;

    if (HAL_ICACHE_Disable() != HAL_OK)
    {
      STBOX1_PRINTF("ERROR: ICache disable failed\r\n");
      goto update_error;
    }
    HAL_FLASH_Unlock();

    if (HAL_FLASHEx_Erase(&EraseInit, &SectorError) != HAL_OK)
    {
      STBOX1_PRINTF("ERROR: Flash erase failed at page %lu\r\n", SectorError);
      HAL_FLASH_Lock();
      HAL_ICACHE_Enable();
      goto update_error;
    }
    STBOX1_PRINTF("Erased %lu pages (%lu KB)\r\n", num_pages,
                  (num_pages * FLASH_PAGE_SIZE) / 1024);
  }

  /* Program firmware from SD to flash, 16 bytes at a time */
  STBOX1_PRINTF("Programming flash...\r\n");
  {
    /* OTA writes to physical Bank 2 address (0x08100000) regardless of
       which bank is currently active — the hardware swap maps it correctly */
    uint32_t write_addr = 0x08100000;
    ULONG total_written = 0;
    uint8_t read_buf[512];
    uint32_t quad_buf[4];  /* 16-byte aligned write buffer */

    fx_file_seek(&fw_file, 0);

    while (total_written < fw_size)
    {
      /* Read a chunk from SD */
      status = fx_file_read(&fw_file, read_buf, sizeof(read_buf), &bytes_read);
      if (status != FX_SUCCESS && status != FX_END_OF_FILE)
      {
        STBOX1_PRINTF("ERROR: SD read failed at offset %lu\r\n", total_written);
        HAL_FLASH_Lock();
        HAL_ICACHE_Enable();
        goto update_error;
      }
      if (bytes_read == 0)
        break;

      /* Write in 16-byte (quadword) chunks */
      for (ULONG i = 0; i < bytes_read; i += 16)
      {
        /* Zero-fill the quadword buffer for the last partial chunk */
        memset(quad_buf, 0xFF, 16);
        ULONG chunk = bytes_read - i;
        if (chunk > 16) chunk = 16;
        memcpy(quad_buf, read_buf + i, chunk);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, write_addr,
                              (uint32_t)quad_buf) != HAL_OK)
        {
          STBOX1_PRINTF("ERROR: Flash program failed at 0x%08lX\r\n", write_addr);
          HAL_FLASH_Lock();
          HAL_ICACHE_Enable();
          goto update_error;
        }
        write_addr += 16;
      }

      total_written += bytes_read;

      /* Blink red LED as progress indicator */
      BSP_LED_Toggle(LED_RED);
    }

    HAL_FLASH_Lock();
    /* v64: do NOT re-enable ICACHE here. The Reference Manual mandates
       ICACHE off during ALL flash programming including option-byte
       programming below. The previous re-enable would cause the
       HAL_FLASHEx_OBProgram call below to silently fail (returns
       HAL_OK but the option byte is not actually written), so
       HAL_FLASH_OB_Launch then reloads the OLD swap-bank value, the
       chip boots from the still-active bank with the still-old
       firmware, and DFU subsequently writes to whichever bank the
       chip ROM bootloader picks — which can be the wrong one.
       Symptom: identical hang behaviour after a series of "successful"
       firmware updates. ICACHE stays off until after OB_Launch (which
       triggers system reset, so re-enable would never run anyway). */

    STBOX1_PRINTF("Programmed %lu bytes\r\n", total_written);
  }

  /* Close firmware file and rename to firmware.done */
  fx_file_close(&fw_file);
  fx_file_rename(&sdio_disk, "firmware.bin", "firmware.done");
  fx_media_flush(&sdio_disk);
  fx_media_close(&sdio_disk);

  STBOX1_PRINTF("Swapping banks and resetting...\r\n");
  BSP_LED_Off(LED_RED);

  /* v64: leave ICACHE disabled through the green-success blinks too
     (HAL_Delay just spins on uwTick, doesn't need ICACHE). The blinks
     are the "we successfully programmed flash" signal — duration is
     visible to the field tester but no flash operations happen here. */

  /* "Firmware flashed successfully" signal: 3 slow green blinks (~1.8 s
     total) before the reset triggers. Clear "we succeeded" feedback for
     the field tester — distinct from both the rapid "detected" signal
     above and the single solid-green "logging active" post-boot state. */
  for (int i = 0; i < 3; i++) {
    BSP_LED_On(LED_GREEN);
    HAL_Delay(300);
    BSP_LED_Off(LED_GREEN);
    HAL_Delay(300);
  }
  BSP_LED_On(LED_GREEN);

  /* Swap flash banks via option bytes — triggers system reset.
     v64: keeps ICACHE disabled (still off from the earlier
     HAL_ICACHE_Disable above). Adds read-back verify after
     HAL_FLASHEx_OBProgram so a silent failure is detectable.
     If verify fails, retry up to 3 times. Logs progress to the
     diagnostic ring (.noinit) so the symptom is recoverable
     post-mortem even if the bank swap itself silently fails. */
  {
    FLASH_OBProgramInitTypeDef OBInit;
    DIAG_CHECKPOINT(0x030001U);  /* SD-update bank-swap entry */
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();
    HAL_FLASHEx_OBGetConfig(&OBInit);

    /* Compute target SWAP_BANK value: opposite of current */
    uint32_t want_swap = ((OBInit.USERConfig & FLASH_OPTR_SWAP_BANK) == FLASH_OPTR_SWAP_BANK)
                         ? 0U
                         : FLASH_OPTR_SWAP_BANK;

    OBInit.OptionType = OPTIONBYTE_USER;
    OBInit.USERType = OB_USER_SWAP_BANK;
    /* v64: use bitwise update instead of overwriting USERConfig
       wholesale — preserves any other USER bits even though
       HAL_FLASHEx_OBProgram only programs the SWAP_BANK bit per
       USERType (the other bits in USERConfig are ignored, but
       safer to keep them consistent). */
    OBInit.USERConfig = (OBInit.USERConfig & ~FLASH_OPTR_SWAP_BANK) | want_swap;

    /* Retry up to 3 times in case of silent program failure. */
    HAL_StatusTypeDef ob_status = HAL_ERROR;
    for (int attempt = 0; attempt < 3; attempt++)
    {
      DIAG_CHECKPOINT(0x030010U | (uint32_t)attempt);
      ob_status = HAL_FLASHEx_OBProgram(&OBInit);
      if (ob_status != HAL_OK)
      {
        STBOX1_PRINTF("ERROR: OBProgram returned non-OK (attempt %d)\r\n", attempt);
        continue;
      }
      /* Read-back verify */
      FLASH_OBProgramInitTypeDef OBCheck;
      HAL_FLASHEx_OBGetConfig(&OBCheck);
      if ((OBCheck.USERConfig & FLASH_OPTR_SWAP_BANK) == want_swap)
      {
        DIAG_CHECKPOINT(0x030020U | (uint32_t)attempt);  /* verified */
        ob_status = HAL_OK;
        break;
      }
      DIAG_CHECKPOINT(0x0300F0U | (uint32_t)attempt);  /* silent fail */
      STBOX1_PRINTF("WARN: OB readback mismatch (attempt %d)\r\n", attempt);
      HAL_Delay(50);
      ob_status = HAL_ERROR;
    }

    if (ob_status != HAL_OK)
    {
      DIAG_CHECKPOINT(0x0300FFU);  /* gave up after 3 tries */
      STBOX1_PRINTF("FATAL: OB program/verify failed after 3 attempts — "
                    "next boot will continue from current bank, "
                    "firmware update LOST\r\n");
      /* Fall through to reset anyway. The chip will boot from the
         original bank; the new firmware sits in the inactive bank
         and is invisible. The user will see the boot marker still
         report the OLD version. Recovery: DFU mass-erase + reflash. */
    }

    DIAG_CHECKPOINT(0x030030U);  /* about to OB_Launch */
    /* This triggers a system reset */
    HAL_FLASH_OB_Launch();

    /* Should not reach here */
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    HAL_NVIC_SystemReset();
  }

  /* Never reached */
  return 0;

update_error:
  STBOX1_PRINTF("Firmware update FAILED — continuing normal boot\r\n");
  fx_file_close(&fw_file);
  fx_media_close(&sdio_disk);
  BSP_LED_Off(LED_RED);
  return 0;
}

static void read_thread_entry(ULONG thread_input)
{
  MessageData_t *Msg;
  INT LogCommandType = COMMAND_STOP_LOG;

  /* Auto-start logging on power-on — only in LOG mode (issue #14
   * mode-switch design). In BLE mode the box sits idle in BLE
   * advertising, fx_thread waits for nothing, and the user starts
   * logging via the iPhone app's START command (which writes
   * BKP1R=LOG_MAGIC + duration → reboots → comes back here in LOG
   * mode → auto-start fires). */
  if (g_app_mode == APP_MODE_LOG)
  {
    Msg = (MessageData_t *)SensorBufferWritingPointer;
    SensorBufferWritingPointer += sizeof(MessageData_t);
    if (SensorBufferWritingPointer == SensorBufferEndPointer)
    {
      SensorBufferWritingPointer = SensorBuffer;
    }
    Msg->CommandType = LogCommandType = COMMAND_START_LOG;
    if (tx_queue_send(&MessageQueue, &Msg, TX_WAIT_FOREVER) != TX_SUCCESS)
    {
      Error_Handler(__FILE__, __LINE__);
    }
    MessagePushed++;
    STBOX1_PRINTF("Auto-start SD logging (LOG mode)\r\n");
    ErrorLog_Write("mode: LOG — autostart logging");
  }
  else
  {
    STBOX1_PRINTF("BLE mode — logger idle, waiting for START command\r\n");
    ErrorLog_Write("mode: BLE — logger idle, advertising");
  }

  while (1)
  {
    tx_semaphore_get(&SemaphorePtr, TX_WAIT_FOREVER);

    Msg = (MessageData_t *)SensorBufferWritingPointer;
    SensorBufferWritingPointer += sizeof(MessageData_t);
    if (SensorBufferWritingPointer == SensorBufferEndPointer)
    {
      SensorBufferWritingPointer = SensorBuffer;
    }

    if (UserButtonPressed)
    {
      static ULONG ButtonPressedTime = 0;
      ULONG NewTime;
      UserButtonPressed = 0;

      NewTime = tx_time_get();
      /* For avoiding a double click */
      if ((NewTime - ButtonPressedTime) > 100)
      {
        ButtonPressedTime = NewTime;
        /* Issue #15: in BLE mode the button must NOT start a logging
         * session — sessions are GUI-driven only (iPhone writes
         * OP_START_LOG). Button only meaningful in LOG mode where it
         * lets the field tester abort a session early. */
        if (g_app_mode != APP_MODE_LOG)
        {
          STBOX1_PRINTF("BLE mode: button ignored (sessions are GUI-driven)\r\n");
          ErrorLog_Write("button: ignored in BLE mode");
          continue;
        }
        if (LogCommandType == COMMAND_STOP_LOG)
        {
          Msg->CommandType = LogCommandType = COMMAND_START_LOG;
        }
        else
        {
          Msg->CommandType = LogCommandType = COMMAND_STOP_LOG;
        }

        /* Send message to MessageQueue.  */
        if (tx_queue_send(&MessageQueue, &Msg, TX_WAIT_FOREVER) != TX_SUCCESS)
        {
          STBOX1_PRINTF("Error Queue Max: %4d\r\n", MessageMaxSize);
          Error_Handler(__FILE__, __LINE__);
        }
        MessagePushed++;
        if ((MessagePushed - MessageRemoved) > MessageMaxSize)
        {
          MessageMaxSize = MessagePushed - MessageRemoved;
        }
      }
    }
    else
    {
      if (SensorsFileOpen == 1)
      {
        /* Read Sensors' Value */
        Msg->CommandType = COMMAND_SAVE_SENSORS;
        Msg->MsgTime = tx_time_get();
        BSP_ENV_SENSOR_GetValue(STTS22H_0, ENV_TEMPERATURE, &Msg->payload.sensors.temperature);
        BSP_ENV_SENSOR_GetValue(LPS22DF_0, ENV_PRESSURE, &Msg->payload.sensors.pressure);
        BSP_MOTION_SENSOR_GetAxes(LSM6DSV16X_0, MOTION_ACCELERO, &Msg->payload.sensors.acc);
        BSP_MOTION_SENSOR_GetAxes(LSM6DSV16X_0, MOTION_GYRO, &Msg->payload.sensors.gyro);
        BSP_MOTION_SENSOR_GetAxes(LIS2MDL_0, MOTION_MAGNETO, &Msg->payload.sensors.mag);

        /* Send message to MessageQueue.  */
        if (tx_queue_send(&MessageQueue, &Msg, TX_WAIT_FOREVER) != TX_SUCCESS)
        {
          STBOX1_PRINTF("Error Queue Max: %4d\r\n", MessageMaxSize);
          Error_Handler(__FILE__, __LINE__);
        }
        MessagePushed++;
        if ((MessagePushed - MessageRemoved) > MessageMaxSize)
        {
          MessageMaxSize = MessagePushed - MessageRemoved;
        }
      }
    }
  }
}


/**
  * @brief Half Transfer user callback, called by BSP functions.
  * @param None
  * @retval None
  */
void BSP_AUDIO_IN_HalfTransfer_CallBack(uint32_t Instance)
{
  UNUSED(Instance);
  if (AudioFileOpen)
  {
    AudioProcess_SD_Recording(OnBoard_PCM_Buffer, PCM_AUDIO_IN_SAMPLES);
  }
}

/**
  * @brief Transfer Complete user callback, called by BSP functions.
  * @param None
  * @retval None
  */
void BSP_AUDIO_IN_TransferComplete_CallBack(uint32_t Instance)
{
  UNUSED(Instance);
  if (AudioFileOpen)
  {
    AudioProcess_SD_Recording(OnBoard_PCM_Buffer, PCM_AUDIO_IN_SAMPLES);
  }
}


/**
  * @brief  Management of the audio data logging
  * @param  pInBuff     points to the audio buffer.
  * @param  len         number of samples to process.
  * @retval None
  */
static void AudioProcess_SD_Recording(uint16_t *pInBuff, uint32_t len)
{
  if (SkipFirst200mS > 0)
  {
    /* Tmp workaround for Initial Mic glitch */
    SkipFirst200mS--;
    return;
  }
  /* Accumulate audio buffer into local ping-pong buffer before writing */
  memcpy(Audio_OUT_Buff + WriteIndexBufferAudio, pInBuff, len * sizeof(uint16_t));
  WriteIndexBufferAudio += len;

  /* WriteIndexBufferAudio &=AUDIO_BUFF_SIZE_MASK; */
  WriteIndexBufferAudio %= AUDIO_BUFF_SIZE;

  if (WriteIndexBufferAudio == (AUDIO_BUFF_SIZE / 2))
  {
    MessageData_t *Msg;
    Msg = (MessageData_t *)SensorBufferWritingPointer;
    SensorBufferWritingPointer += sizeof(MessageData_t);
    if (SensorBufferWritingPointer == SensorBufferEndPointer)
    {
      SensorBufferWritingPointer = SensorBuffer;
    }
    Msg->CommandType = COMMAND_SAVE_AUDIO;
    /* first half */
    ReadIndexBufferAudio = 0;

    /* Send the Message for writing out the 1/2 buffer */
    if (tx_queue_send(&MessageQueue, &Msg, TX_NO_WAIT) != TX_SUCCESS)
    {
      STBOX1_PRINTF("Error Queue Max: %4d\r\n", MessageMaxSize);
      Error_Handler(__FILE__, __LINE__);
    }
    MessagePushed++;
    if ((MessagePushed - MessageRemoved) > MessageMaxSize)
    {
      MessageMaxSize = MessagePushed - MessageRemoved;
    }
  }
  else if (WriteIndexBufferAudio == 0)
  {
    MessageData_t *Msg;
    Msg = (MessageData_t *)SensorBufferWritingPointer;
    SensorBufferWritingPointer += sizeof(MessageData_t);
    if (SensorBufferWritingPointer == SensorBufferEndPointer)
    {
      SensorBufferWritingPointer = SensorBuffer;
    }
    Msg->CommandType = COMMAND_SAVE_AUDIO;
    /* second half */
    ReadIndexBufferAudio = AUDIO_BUFF_SIZE / 2;
    /* Send the Message for writing out the 1/2 buffer */
    if (tx_queue_send(&MessageQueue, &Msg, TX_NO_WAIT) != TX_SUCCESS)
    {
      STBOX1_PRINTF("Error Queue Max: %4d\r\n", MessageMaxSize);
      Error_Handler(__FILE__, __LINE__);
    }
    MessagePushed++;
    if ((MessagePushed - MessageRemoved) > MessageMaxSize)
    {
      MessageMaxSize = MessagePushed - MessageRemoved;
    }
  }

  /* Control section */
  if (ReadIndexBufferAudio == 0)
  {
    /* We need to read the First half */
    if (WriteIndexBufferAudio < (AUDIO_BUFF_SIZE / 2))
    {
      BSP_LED_Toggle(LED_RED);
    }
  }
  else if (ReadIndexBufferAudio == (AUDIO_BUFF_SIZE / 2))
  {
    /* We need to Read the Second half */
    if (WriteIndexBufferAudio > (AUDIO_BUFF_SIZE / 2))
    {
      BSP_LED_Toggle(LED_RED);
    }
  }
}

/**
  * @brief  Initialize the wave header file
  * @param  pHeader: Header Buffer to be filled
  * @param  pWaveFormatStruct: Pointer to the wave structure to be filled.
  * @retval 0 if passed, !0 if failed.
  */
static uint32_t WavProcess_HeaderInit(void)
{
  uint16_t   BitPerSample = 16;
  uint32_t   ByteRate = AUDIO_IN_SAMPLING_FREQUENCY * (BitPerSample / 8);

  uint32_t   SampleRate = AUDIO_IN_SAMPLING_FREQUENCY;
  uint16_t   BlockAlign = BitPerSample / 8;

  /* Write chunkID, must be 'RIFF'  ------------------------------------------*/
  pAudioHeader[0] = 'R';
  pAudioHeader[1] = 'I';
  pAudioHeader[2] = 'F';
  pAudioHeader[3] = 'F';

  /* Write the file length ----------------------------------------------------*/
  /* The sampling time: this value will be be written back at the end of the
  recording operation.  Example: 661500 Bytes = 0x000A17FC, byte[7]=0x00, byte[4]=0xFC */
  pAudioHeader[4] = 0x00;
  pAudioHeader[5] = 0x4C;
  pAudioHeader[6] = 0x1D;
  pAudioHeader[7] = 0x00;

  /* Write the file format, must be 'WAVE' -----------------------------------*/
  pAudioHeader[8]  = 'W';
  pAudioHeader[9]  = 'A';
  pAudioHeader[10] = 'V';
  pAudioHeader[11] = 'E';

  /* Write the format chunk, must be'fmt ' -----------------------------------*/
  pAudioHeader[12]  = 'f';
  pAudioHeader[13]  = 'm';
  pAudioHeader[14]  = 't';
  pAudioHeader[15]  = ' ';

  /* Write the length of the 'fmt' data, must be 0x10 ------------------------*/
  pAudioHeader[16]  = 0x10;
  pAudioHeader[17]  = 0x00;
  pAudioHeader[18]  = 0x00;
  pAudioHeader[19]  = 0x00;

  /* Write the audio format, must be 0x01 (PCM) ------------------------------*/
  pAudioHeader[20]  = 0x01;
  pAudioHeader[21]  = 0x00;

  /* Write the number of channels, ie. 0x01 (Mono) ---------------------------*/
  pAudioHeader[22]  = 1;
  pAudioHeader[23]  = 0x00;

  /* Write the Sample Rate in Hz ---------------------------------------------*/
  /* Write Little Endian ie. 8000 = 0x00001F40 => byte[24]=0x40, byte[27]=0x00*/
  pAudioHeader[24]  = (uint8_t)((SampleRate & 0xFF));
  pAudioHeader[25]  = (uint8_t)((SampleRate >> 8) & 0xFF);
  pAudioHeader[26]  = (uint8_t)((SampleRate >> 16) & 0xFF);
  pAudioHeader[27]  = (uint8_t)((SampleRate >> 24) & 0xFF);

  /* Write the Byte Rate -----------------------------------------------------*/
  pAudioHeader[28]  = (uint8_t)((ByteRate & 0xFF));
  pAudioHeader[29]  = (uint8_t)((ByteRate >> 8) & 0xFF);
  pAudioHeader[30]  = (uint8_t)((ByteRate >> 16) & 0xFF);
  pAudioHeader[31]  = (uint8_t)((ByteRate >> 24) & 0xFF);

  /* Write the block alignment -----------------------------------------------*/
  pAudioHeader[32]  = BlockAlign;
  pAudioHeader[33]  = 0x00;

  /* Write the number of bits per sample -------------------------------------*/
  pAudioHeader[34]  = BitPerSample;
  pAudioHeader[35]  = 0x00;

  /* Write the Data chunk, must be 'data' ------------------------------------*/
  pAudioHeader[36]  = 'd';
  pAudioHeader[37]  = 'a';
  pAudioHeader[38]  = 't';
  pAudioHeader[39]  = 'a';

  /* Write the number of sample data -----------------------------------------*/
  /* This variable will be written back at the end of the recording operation */
  pAudioHeader[40]  = 0x00;
  pAudioHeader[41]  = 0x4C;
  pAudioHeader[42]  = 0x1D;
  pAudioHeader[43]  = 0x00;

  /* Return 0 if all operations are OK */
  return 0;
}

/**
  * @brief  Initialize the wave header file
  * @param  pHeader: Header Buffer to be filled
  * @param  pWaveFormatStruct: Pointer to the wave structure to be filled.
  * @retval 0 if passed, !0 if failed.
  */
static uint32_t WavProcess_HeaderUpdate(uint32_t len)
{
  /* Write the file length ----------------------------------------------------*/
  /* The sampling time: this value will be be written back at the end of the
  recording operation.  Example: 661500 Bytes = 0x000A17FC, byte[7]=0x00, byte[4]=0xFC */
  pAudioHeader[4] = (uint8_t)(len);
  pAudioHeader[5] = (uint8_t)(len >> 8);
  pAudioHeader[6] = (uint8_t)(len >> 16);
  pAudioHeader[7] = (uint8_t)(len >> 24);
  /* Write the number of sample data -----------------------------------------*/
  /* This variable will be written back at the end of the recording operation */
  len -= 44;
  pAudioHeader[40] = (uint8_t)(len);
  pAudioHeader[41] = (uint8_t)(len >> 8);
  pAudioHeader[42] = (uint8_t)(len >> 16);
  pAudioHeader[43] = (uint8_t)(len >> 24);
  /* Return 0 if all operations are OK */
  return 0;
}


/**
  * @brief  GPS poll thread.
  *         Polls GPS_GetLatestFix() at 1 Hz and, when a new fix is ready,
  *         pushes a COMMAND_SAVE_GPS message onto the shared FileX queue.
  *         Skips when GpsFileOpen==0 (between stop and next start).
  *
  *         Uses a private slot in SensorBuffer to avoid clashing with the
  *         sensor reading thread's buffer rotation.
  */
static void gps_thread_entry(ULONG thread_input)
{
  (void)thread_input;
  /* Dedicated message slot for GPS — sits outside the 100-entry sensor ring
     buffer. Static keeps it alive across calls; the FileX writer copies the
     contents before acknowledging, so overwriting next tick is safe. */
  static MessageData_t GpsMsg;

  /* Poll the GPS fix buffer at the module's output rate. 1 tick = 10 ms,
     so for GPS_RATE_HZ = 10 (100 ms period) this sleeps 10 ticks = 100 ms.
     Before this change the sleep was hard-coded to 100 ticks (= 1 s) and
     silently dropped 9 of every 10 fixes when the module was pushed to
     10 Hz — the NMEA parser filled LatestFix 10×/s but the thread only
     read once per second. */
  const ULONG gps_poll_ticks = (GPS_MEAS_PERIOD_MS / 10) > 0 ? (GPS_MEAS_PERIOD_MS / 10) : 1;

  while (1)
  {
    tx_thread_sleep(gps_poll_ticks);

    /* Drain the UART4 RX ring and parse any complete $GNRMC / $GNGGA
       sentences at thread priority. Previously this work happened inside
       the UART4 IRQ (priority 6), which preempted SDMMC1 (priority 14)
       and at 10 Hz GPS starved the FileX writer — the sensor thread then
       fell from 100 Hz to ~7.7 Hz after ~90 s of logging. */
    GPS_Process();

    /* Seed FileX's wall-clock base from GPS UTC, once. Runs independently
       of GpsFileOpen so the base is ready as soon as the first fix lands
       — subsequent fx_media_flush calls (which run UpdateFileXClock) then
       stamp Sens/Gps/Bat directory entries with today's real date instead
       of the stale compile date. */
    if (!ClockSeededFromGPS)
    {
      uint32_t gy, gmo, gd, gh, gmi, gs;
      if (GPS_GetWallClock(&gy, &gmo, &gd, &gh, &gmi, &gs))
      {
        SetClockBaseFromGPS(gy, gmo, gd, gh, gmi, gs);
        CHAR m[96];
        sprintf(m, "clock: seeded from GPS %04lu-%02lu-%02lu %02lu:%02lu:%02lu UTC",
                (unsigned long)gy, (unsigned long)gmo, (unsigned long)gd,
                (unsigned long)gh, (unsigned long)gmi, (unsigned long)gs);
        ErrorLog_Write(m);
      }
    }

    if (!GpsFileOpen) continue;

    GPS_Fix_t fix;
    if (!GPS_GetLatestFix(&fix)) continue;
    if (!fix.Valid) continue;

    GpsMsg.CommandType   = COMMAND_SAVE_GPS;
    GpsMsg.MsgTime       = tx_time_get();
    GpsMsg.payload.gps   = fix;

    MessageData_t *pMsg = &GpsMsg;
    if (tx_queue_send(&MessageQueue, &pMsg, TX_NO_WAIT) == TX_SUCCESS)
    {
      MessagePushed++;
      if ((MessagePushed - MessageRemoved) > MessageMaxSize)
      {
        MessageMaxSize = MessagePushed - MessageRemoved;
      }
    }
    /* On queue full: drop this fix — the sensor data is higher priority. */
  }
}

/* USER CODE END 1 */

/**
  ******************************************************************************
  * @file    stm32u5xx_it.c
  * @author  MCD Application Team
  * @version V2.1.0
  * @date    20-May-2025
  * @brief   Interrupt Service Routines
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
#include "main.h"
#include "stm32u5xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "fx_stm32_sd_driver.h"
#include "SensorTileBoxPro_audio.h"
#include "gps_nmea.h"
#include "stbox1_config.h"
#if STBOX1_ENABLE_USB_CDC
#include "tusb.h"
#endif
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* v55: Fault-Handler-Instrumentation. Until now the four ARM core
   exception handlers were silent `while(1) {}` — meaning a hard fault
   in fx_thread (e.g. stack overflow inside FileX, misaligned access
   in SDMMC DMA, NULL deref on a corrupted handle) would freeze the
   chip with NO LED change and NO disk marker. That perfectly matches
   Peter's reservebox symptom ("4 lines, then alle LED dunkel"), so
   silent faults remain a live hypothesis.

   Each fault now:
     1. Snapshots PC/LR from the exception stack frame into a backup
        register and into a static struct (fault_info)
     2. Forces the green LED OFF (so we can tell it's a fault, not
        a "stopped logging" state)
     3. Blinks the red LED in a distinctive Morse-style pattern that
        identifies the fault type:
           HardFault   = 5 short pulses
           MemManage   = 6 short pulses
           BusFault    = 7 short pulses
           UsageFault  = 8 short pulses
           NMI         = 9 short pulses (also captures stack overflow)
        Pattern: N pulses (200ms on / 200ms off), then 1.5s pause,
        repeat. All within while(1) — never returns.

   The PC capture lets a future post-mortem (via SWD or a SD-write
   pre-fault) pinpoint the offending instruction. */

#include "stm32u5xx_hal.h"
#include "SensorTileBoxPro.h"

/* fault_info_t typedef is in main.h (so app_filex.c can read it).
   Placed in .noinit so values survive warm reset; ErrorLog_Open on
   the next boot reads g_fault_info.magic and emits a recovered
   FAULT line if valid. */
__attribute__((section(".noinit"))) fault_info_t g_fault_info;

/* v59: disk-less diagnostic ring. Each DIAG_CHECKPOINT(code) call
   appends to this 32-entry ring buffer in .noinit RAM. No SD I/O,
   no flush, cannot hang. ErrorLog_Open reads on next boot and emits
   the contents — so we can see exactly which checkpoint fx_thread
   reached before getting stuck. */
__attribute__((section(".noinit"))) diag_ring_t g_diag_ring;

void Diag_Checkpoint(uint32_t code)
{
  /* First call after a fresh power-up will see uninitialised .noinit
     contents — magic is random. We initialize lazily here on the
     first call by checking magic; if wrong, clear ring + set magic.
     Subsequent calls just append. */
  if (g_diag_ring.magic != DIAG_RING_MAGIC)
  {
    g_diag_ring.magic = DIAG_RING_MAGIC;
    g_diag_ring.head  = 0;
    g_diag_ring.wrap  = 0;
    /* Don't bother clearing entries — head/wrap define what's valid. */
  }
  uint32_t i = g_diag_ring.head;
  g_diag_ring.entries[i].tick = (uint32_t)tx_time_get();  /* 0 if pre-RTOS */
  g_diag_ring.entries[i].code = code;
  g_diag_ring.head = (i + 1U) % DIAG_RING_SIZE;
  if (g_diag_ring.head == 0U) g_diag_ring.wrap = 1U;
  /* Memory barrier so the .noinit write is committed before any
     subsequent code that might fault — important when this is used
     to track progress through a busy-wait region. */
  __asm volatile ("dsb" ::: "memory");
}

static void __attribute__((noreturn, used)) fault_morse_loop(uint32_t fault_id)
{
  /* Make sure LEDs are usable from the fault context — clocks should
     already be on, but call enable just in case. */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /* Force green LED OFF — distinguishes "fault" from "logger stopped". */
  BSP_LED_Off(LED_GREEN);

  /* Pattern: fault_id pulses (200ms on / 200ms off), 1.5s gap, repeat.
     Uses a delay loop based on systick because HAL_Delay needs SysTick
     working — which it is, but we keep it simple with HAL_Delay anyway
     since interrupts are disabled in fault context but SysTick is the
     CPU's own. */
  while (1) {
    for (uint32_t i = 0; i < fault_id; i++) {
      BSP_LED_On(LED_RED);
      for (volatile uint32_t d = 0; d < 1600000U; d++) { __NOP(); }  /* ~200ms @ 160MHz */
      BSP_LED_Off(LED_RED);
      for (volatile uint32_t d = 0; d < 1600000U; d++) { __NOP(); }  /* ~200ms */
    }
    for (volatile uint32_t d = 0; d < 12000000U; d++) { __NOP(); }   /* ~1.5s gap */
  }
}

/* Common entry from each fault stub. r0 = pointer to stacked frame
   (PSP or MSP depending on which mode was active). */
void fault_capture_and_morse(uint32_t fault_id, uint32_t *frame)
{
  /* ARM v7-M exception stack frame layout (when no FPU push):
     frame[0]=R0  [1]=R1  [2]=R2  [3]=R3  [4]=R12  [5]=LR  [6]=PC  [7]=xPSR */
  g_fault_info.magic    = 0xFA0177ABU;
  g_fault_info.fault_id = fault_id;
  g_fault_info.pc       = frame[6];
  g_fault_info.lr       = frame[5];
  g_fault_info.psr      = frame[7];
  g_fault_info.cfsr     = SCB->CFSR;
  g_fault_info.hfsr     = SCB->HFSR;
  g_fault_info.mmfar    = SCB->MMFAR;
  g_fault_info.bfar     = SCB->BFAR;

  fault_morse_loop(fault_id);
}

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
  /* v55: capture PC + morse 9 pulses */
  __asm volatile (
    "tst lr, #4 \n"
    "ite eq \n"
    "mrseq r1, msp \n"
    "mrsne r1, psp \n"
    "mov r0, #5 \n"        /* fault_id = 5 (NMI) */
    "b fault_capture_and_morse \n"
  );
  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  __asm volatile (
    "tst lr, #4 \n"
    "ite eq \n"
    "mrseq r1, msp \n"
    "mrsne r1, psp \n"
    "mov r0, #1 \n"        /* fault_id = 1 (HardFault) */
    "b fault_capture_and_morse \n"
  );
  /* USER CODE END HardFault_IRQn 0 */
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
  __asm volatile (
    "tst lr, #4 \n"
    "ite eq \n"
    "mrseq r1, msp \n"
    "mrsne r1, psp \n"
    "mov r0, #2 \n"        /* fault_id = 2 (MemManage) */
    "b fault_capture_and_morse \n"
  );
  /* USER CODE END MemoryManagement_IRQn 0 */
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
  __asm volatile (
    "tst lr, #4 \n"
    "ite eq \n"
    "mrseq r1, msp \n"
    "mrsne r1, psp \n"
    "mov r0, #3 \n"        /* fault_id = 3 (BusFault) */
    "b fault_capture_and_morse \n"
  );
  /* USER CODE END BusFault_IRQn 0 */
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  __asm volatile (
    "tst lr, #4 \n"
    "ite eq \n"
    "mrseq r1, msp \n"
    "mrsne r1, psp \n"
    "mov r0, #4 \n"        /* fault_id = 4 (UsageFault) */
    "b fault_capture_and_morse \n"
  );
  /* USER CODE END UsageFault_IRQn 0 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32U5xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32u5xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles TIM6 global interrupt.
  */
void TIM6_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_IRQn 0 */

  /* USER CODE END TIM6_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_IRQn 1 */

  /* USER CODE END TIM6_IRQn 1 */
}

/* USER CODE BEGIN 1 */
void SDMMC1_IRQHandler(void)
{
  BSP_SD_IRQHandler(FX_STM32_SD_INSTANCE);
}

/* USER CODE END 1 */

/**
  * @brief This function handles EXTI Line13 interrupt.
  */
void EXTI13_IRQHandler(void)
{
  HAL_EXTI_IRQHandler(&H_EXTI_13);
}

/**
* @brief This function handles MDF DMA interrupt request.
* @param None
* @retval None
*/
void GPDMA1_Channel1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(DMic_OnBoard_MDFFilter.hdma);
}

/**
* @brief UART4 global interrupt (GPS module reception).
*/
#if STBOX1_ENABLE_USB_CDC
/**
  * @brief This function handles USB OTG FS global interrupt.
  *
  * Forwards to TinyUSB's device interrupt handler. NVIC priority is set
  * to 13 in usb_cdc.c::usb_cdc_hw_init — between SDMMC1 (14) and UART4-GPS
  * (6) so neither path can be starved by USB activity.
  */
void OTG_FS_IRQHandler(void)
{
  tud_int_handler(0);
}
#endif

void UART4_IRQHandler(void)
{
  GPS_UART_IRQHandler();
}

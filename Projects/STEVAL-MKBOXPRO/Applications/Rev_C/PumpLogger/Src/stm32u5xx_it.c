/**
  ******************************************************************************
  * @file    stm32u5xx_it.c
  * @brief   Cortex + STM32 system interrupt handlers.
  *
  *          Only SysTick is wired up here — per F-ARCH-6 we run no
  *          application-level ISRs. SysTick is the system time base; HAL
  *          chains the SysTick callback into sched.c which advances the
  *          tick counter.
  ******************************************************************************
  */

#include "main.h"

/* Cortex handlers (default — let HAL/CMSIS provide the bodies). */
void NMI_Handler(void)            { while (1) {} }
void HardFault_Handler(void)      { while (1) {} }
void MemManage_Handler(void)      { while (1) {} }
void BusFault_Handler(void)       { while (1) {} }
void UsageFault_Handler(void)     { while (1) {} }
void SVC_Handler(void)            { }
void DebugMon_Handler(void)       { }
void PendSV_Handler(void)         { }

/* System tick — HAL needs this to drive HAL_GetTick(), and we hook
   HAL_SYSTICK_Callback() in sched.c for the scheduler tick. */
void SysTick_Handler(void)
{
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
}

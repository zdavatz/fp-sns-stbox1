/**
  ******************************************************************************
  * @file    main.c
  * @brief   PumpLogger entry point. Bare-metal cooperative scheduler.
  *
  *          Phase 2 deliverable: cold-boot → SystemClock_Config → GPIOs +
  *          buzzer init → boot beep → main loop that toggles the green
  *          LED at 0.5 Hz. Demonstrates that the scheduler ticks, the
  *          buzzer works, and the firmware runs cleanly with zero
  *          application-level ISRs.
  *
  *          Subsequent phases bolt logger / BLE / watchdog tasks onto the
  *          same scheduler skeleton.
  ******************************************************************************
  */

#include "main.h"
#include "sched.h"
#include "buzzer.h"

static void gpio_init_leds(void);

int main(void)
{
  /* HAL_MspInit (defined below) lands SMPS + VDDIO2 + UCPD-dead-battery
     setup INSIDE HAL_Init() so the chip is ready for scale 1 / 160 MHz
     before any other HAL call. Required on STM32U5: doing the PWR
     config *after* HAL_Init hangs SystemClock_Config at the voltage-
     stabilization wait. */
  HAL_Init();
  SystemClock_Config();

  /* Red LED on while we set up peripherals; green LED idle off. */
  gpio_init_leds();
  HAL_GPIO_WritePin(PL_LED_RED_PORT,   PL_LED_RED_PIN,   GPIO_PIN_SET);
  HAL_GPIO_WritePin(PL_LED_GREEN_PORT, PL_LED_GREEN_PIN, GPIO_PIN_RESET);

  /* Buzzer last — depends on TIM1 + GPIOE clocks. */
  Buzzer_Init();
  Buzzer_BootDone();

  /* Hand-off: red off, green on. From here on the scheduler drives
     everything via SysTick. */
  HAL_GPIO_WritePin(PL_LED_RED_PORT,   PL_LED_RED_PIN,   GPIO_PIN_RESET);
  HAL_GPIO_WritePin(PL_LED_GREEN_PORT, PL_LED_GREEN_PIN, GPIO_PIN_SET);

  for (;;)
  {
    if (sched_should_run(PL_CADENCE_LED_BLINK)) {
      HAL_GPIO_TogglePin(PL_LED_GREEN_PORT, PL_LED_GREEN_PIN);
    }

    /* Phase 3 hooks land here: logger_tick(), gps_tick(), battery_tick(), ... */
    /* Phase 4 hooks land here: ble_tick(). */

    sched_wait_next_tick();
  }
}

/* ----------------------------------------------------------------------- */

static void gpio_init_leds(void)
{
  __HAL_RCC_GPIOF_CLK_ENABLE();   /* green LED on PF6 */
  __HAL_RCC_GPIOH_CLK_ENABLE();   /* red LED on PH11 */

  GPIO_InitTypeDef gpio = {0};
  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;

  gpio.Pin = PL_LED_GREEN_PIN;
  HAL_GPIO_Init(PL_LED_GREEN_PORT, &gpio);

  gpio.Pin = PL_LED_RED_PIN;
  HAL_GPIO_Init(PL_LED_RED_PORT, &gpio);
}

/* HSI → 160 MHz sysclk. Lifted from SDDataLogFileX (verified working).
   HSE is unreliable on this board's 3.3 V mod — see CLAUDE.md. */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef     osc  = {0};
  RCC_ClkInitTypeDef     clk  = {0};
  RCC_PeriphCLKInitTypeDef pc = {0};

  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
    Error_Handler(__FILE__, __LINE__);
  }

  osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI48 | RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI;
  osc.HSIState            = RCC_HSI_ON;
  osc.HSI48State          = RCC_HSI48_ON;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.LSIState            = RCC_LSI_ON;
  osc.LSIDiv              = RCC_LSI_DIV1;
  osc.PLL.PLLState        = RCC_PLL_ON;
  osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
  osc.PLL.PLLMBOOST       = RCC_PLLMBOOST_DIV1;
  osc.PLL.PLLM            = 1;
  osc.PLL.PLLN            = 10;
  osc.PLL.PLLP            = 1;
  osc.PLL.PLLQ            = 2;
  osc.PLL.PLLR            = 1;
  osc.PLL.PLLRGE          = RCC_PLLVCIRANGE_1;
  osc.PLL.PLLFRACN        = 0;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
    Error_Handler(__FILE__, __LINE__);
  }

  clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3;
  clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
  clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  clk.APB2CLKDivider = RCC_HCLK_DIV1;
  clk.APB3CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
    Error_Handler(__FILE__, __LINE__);
  }

  /* PLL2/PLL3 for ADCDAC/MDF1 — kept identical to SDDataLogFileX so
     future phases (battery ADC, mic if ever re-enabled) work without
     reconfiguring. */
  pc.PeriphClockSelection = RCC_PERIPHCLK_MDF1 | RCC_PERIPHCLK_ADF1 | RCC_PERIPHCLK_ADCDAC;
  pc.Mdf1ClockSelection   = RCC_MDF1CLKSOURCE_PLL3;
  pc.Adf1ClockSelection   = RCC_ADF1CLKSOURCE_PLL3;
  pc.AdcDacClockSelection = RCC_ADCDACCLKSOURCE_PLL2;
  pc.PLL3.PLL3Source      = RCC_PLLSOURCE_HSI;
  pc.PLL3.PLL3M           = 2;
  pc.PLL3.PLL3N           = 48;
  pc.PLL3.PLL3P           = 2;
  pc.PLL3.PLL3Q           = 25;
  pc.PLL3.PLL3R           = 2;
  pc.PLL3.PLL3RGE         = RCC_PLLVCIRANGE_1;
  pc.PLL3.PLL3FRACN       = 0;
  pc.PLL3.PLL3ClockOut    = RCC_PLL3_DIVQ;
  pc.PLL2.PLL2Source      = RCC_PLLSOURCE_HSI;
  pc.PLL2.PLL2M           = 2;
  pc.PLL2.PLL2N           = 48;
  pc.PLL2.PLL2P           = 2;
  pc.PLL2.PLL2Q           = 7;
  pc.PLL2.PLL2R           = 25;
  pc.PLL2.PLL2RGE         = RCC_PLLVCIRANGE_1;
  pc.PLL2.PLL2FRACN       = 0;
  pc.PLL2.PLL2ClockOut    = RCC_PLL2_DIVR;
  if (HAL_RCCEx_PeriphCLKConfig(&pc) != HAL_OK) {
    Error_Handler(__FILE__, __LINE__);
  }
}

void Error_Handler(const char *file, int line)
{
  (void)file; (void)line;
  __disable_irq();
  /* Solid red LED. No serial / no log — Phase 2 keeps it bare. */
  HAL_GPIO_WritePin(PL_LED_RED_PORT, PL_LED_RED_PIN, GPIO_PIN_SET);
  for (;;) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file; (void)line;
  Error_Handler((const char *)file, (int)line);
}
#endif

/* Override the weak HAL_MspInit so PWR config lands INSIDE HAL_Init.
   Same body as SDDataLogFileX's HAL_MspInit — disable the UCPD
   dead-battery pull-downs, enable VddIO2 (for PG[15:2] / PI pins),
   and switch the regulator from LDO to SMPS so the chip can deliver
   the higher current required at scale 1 / 160 MHz. */
void HAL_MspInit(void)
{
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWREx_DisableUCPDDeadBattery();
  HAL_PWREx_EnableVddIO2();
  HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY);
}

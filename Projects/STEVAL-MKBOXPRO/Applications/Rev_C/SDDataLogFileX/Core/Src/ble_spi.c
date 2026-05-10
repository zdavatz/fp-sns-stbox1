/**
  ******************************************************************************
  * @file    ble_spi.c
  * @brief   Isolated SPI1 driver for the BlueNRG (STM32WB07_06) HCI link.
  *          See header for rationale.
  ******************************************************************************
  */

#include "ble_spi.h"
#include <stdio.h>

static SPI_HandleTypeDef hspi_ble;
static uint8_t initialized;

static void ble_spi_msp_init(void)
{
  GPIO_InitTypeDef gpio = {0};
  RCC_PeriphCLKInitTypeDef pclk = {0};

  /* DO NOT call HAL_RCCEx_PeriphCLKConfig here — on STM32U5, the call
   * (even with PeriphClockSelection=SPI1 only) reads other CCIPRx fields
   * via __HAL_RCC_GET_*_SOURCE() and writes them back, which can clobber
   * the ICLK selector that USB FS (HSI48) depends on. After this call the
   * OTG_FS clock dropped to default (HSI 16 MHz) and the host-side enum
   * stalled with -110 timeouts. SPI1 defaults to PCLK2 which is fine for
   * the BlueNRG-LP HCI link at 1.25 MHz (160 MHz / 128 prescaler). */
  (void) pclk;

  __HAL_RCC_SPI1_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* PA7 = MOSI (AF5) */
  gpio.Pin       = GPIO_PIN_7;
  gpio.Mode      = GPIO_MODE_AF_PP;
  gpio.Pull      = GPIO_NOPULL;
  gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
  gpio.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* PA5 = SCK (AF5, pull-up to keep idle high for mode 3) */
  gpio.Pin   = GPIO_PIN_5;
  gpio.Pull  = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &gpio);

  /* PA6 = MISO (AF5) */
  gpio.Pin  = GPIO_PIN_6;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &gpio);
}

int32_t BleSpi_Init(void)
{
  if (initialized) {
    return 0;
  }

  ble_spi_msp_init();

  hspi_ble.Instance                         = SPI1;
  hspi_ble.Init.Mode                        = SPI_MODE_MASTER;
  hspi_ble.Init.Direction                   = SPI_DIRECTION_2LINES;
  hspi_ble.Init.DataSize                    = SPI_DATASIZE_8BIT;
  hspi_ble.Init.CLKPolarity                 = SPI_POLARITY_HIGH;  /* mode 3 — matches BlueNRG */
  hspi_ble.Init.CLKPhase                    = SPI_PHASE_2EDGE;
  hspi_ble.Init.NSS                         = SPI_NSS_SOFT;       /* CS driven manually by hci_tl */
  hspi_ble.Init.BaudRatePrescaler           = SPI_BAUDRATEPRESCALER_128;
  hspi_ble.Init.FirstBit                    = SPI_FIRSTBIT_MSB;
  hspi_ble.Init.TIMode                      = SPI_TIMODE_DISABLE;
  hspi_ble.Init.CRCCalculation              = SPI_CRCCALCULATION_DISABLE;
  hspi_ble.Init.CRCPolynomial               = 7;
  hspi_ble.Init.NSSPMode                    = SPI_NSS_PULSE_DISABLE;
  hspi_ble.Init.NSSPolarity                 = SPI_NSS_POLARITY_LOW;
  hspi_ble.Init.FifoThreshold               = SPI_FIFO_THRESHOLD_01DATA;
  hspi_ble.Init.MasterSSIdleness            = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi_ble.Init.MasterInterDataIdleness     = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi_ble.Init.MasterReceiverAutoSusp      = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi_ble.Init.MasterKeepIOState           = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi_ble.Init.IOSwap                      = SPI_IO_SWAP_DISABLE;
  hspi_ble.Init.ReadyMasterManagement       = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
  hspi_ble.Init.ReadyPolarity               = SPI_RDY_POLARITY_HIGH;

  if (HAL_SPI_Init(&hspi_ble) != HAL_OK) {
    return -1;
  }

  /* NOTE: HAL_SPIEx_SetConfigAutonomousMode(DISABLE) was tried in v32
     to mirror BSP_SPI1_Init exactly, but caused the FIRST HCI command
     to hang after payload xfer (slave responded with valid header but
     subsequent send-receive interaction broke). Reverted in v33. */

  initialized = 1;
  return 0;
}

int32_t BleSpi_SendRecv(uint8_t *tx, uint8_t *rx, uint16_t length)
{
  if (HAL_SPI_TransmitReceive(&hspi_ble, tx, rx, length, 0x1000U) != HAL_OK) {
    return -1;
  }
  return 0;
}

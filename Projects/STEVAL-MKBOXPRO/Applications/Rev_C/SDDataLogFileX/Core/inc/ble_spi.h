/**
  ******************************************************************************
  * @file    ble_spi.h
  * @brief   Isolated SPI1 driver for the BlueNRG (STM32WB07_06) HCI link.
  *
  * Lives in this app rather than the shared SensorTileBoxPro BSP so the BLE
  * file-sync feature can be added without touching the BSP layer that other
  * firmware in this repo depends on. Pin map (PA5=SCK / PA6=MISO / PA7=MOSI,
  * AF5) and SPI mode (master, mode 3, MSB-first, prescaler /128) come straight
  * from the BLEDualProgram BSP — same chip, same wiring, same settings.
  *
  * Used only by hci_tl_interface.c, which calls BleSpi_Init() once and then
  * BleSpi_SendRecv() on every HCI frame. Returns 0 on success, negative on
  * error so it slots cleanly into the BlueNRG return-code convention.
  ******************************************************************************
  */

#ifndef __BLE_SPI_H
#define __BLE_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32u5xx_hal.h"

/**
 * @brief  One-shot SPI1 init (peripheral clock, GPIO AF, SPI HAL).
 *         Idempotent — repeated calls are no-ops after the first success.
 * @retval 0 on success, -1 on failure.
 */
int32_t BleSpi_Init(void);

/**
 * @brief  Full-duplex blocking transfer, length bytes each direction.
 * @retval 0 on success, -1 on HAL error.
 */
int32_t BleSpi_SendRecv(uint8_t *tx, uint8_t *rx, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_SPI_H */

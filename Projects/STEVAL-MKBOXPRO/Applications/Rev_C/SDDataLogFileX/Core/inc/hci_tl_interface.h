/**
  ******************************************************************************
  * @file    hci_tl_interface.h
  * @brief   HCI transport-layer pin/port wiring for the BlueNRG (STM32WB07_06).
  *          Self-contained version — no dependency on steval_mkboxpro_bus.h
  *          (we don't ship that BSP in this app to avoid BSP_*-symbol collisions
  *          with the SensorTileBoxPro BSP). SPI1 is driven via the local
  *          ble_spi.{h,c} HAL wrapper instead.
  *
  * Pin map (matches BLEDualProgram, same hardware):
  *   PB11 — BLE IRQ (EXTI11, rising edge)
  *   PA2  — BLE CS  (push-pull, idle high)
  *   PD4  — BLE RST (push-pull)
  *   PA5/PA6/PA7 — SPI1 SCK/MISO/MOSI (managed in ble_spi.c)
  ******************************************************************************
  */

#ifndef HCI_TL_INTERFACE_H
#define HCI_TL_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32u5xx_hal.h"

/* IRQ from BlueNRG → MCU on PB11, EXTI11 */
#define HCI_TL_SPI_EXTI_PORT       GPIOB
#define HCI_TL_SPI_EXTI_PIN        GPIO_PIN_11
#define HCI_TL_SPI_EXTI_IRQ_N      EXTI11_IRQn
#define HCI_TL_SPI_IRQ_PORT        GPIOB
#define HCI_TL_SPI_IRQ_PIN         GPIO_PIN_11
#define BUS_EXTI_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()

/* Chip-select on PA2, manually driven (BlueNRG SPI uses NSS_SOFT) */
#define HCI_TL_SPI_CS_PORT         GPIOA
#define HCI_TL_SPI_CS_PIN          GPIO_PIN_2
#define BUS_CS_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOA_CLK_ENABLE()

/* Reset line on PD4 */
#define HCI_TL_RST_PORT            GPIOD
#define HCI_TL_RST_PIN             GPIO_PIN_4
#define BUS_RST_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOD_CLK_ENABLE()

/* Exported variables ------------------------------------------------------- */
extern volatile uint32_t hci_event;
extern EXTI_HandleTypeDef hexti11;
#define H_EXTI_11 hexti11
#define H_EXTI    hexti11
#define EXTI_LINE EXTI_LINE_11

/* Exported functions ------------------------------------------------------- */
int32_t hci_tl_spi_init(void *pConf);
int32_t hci_tl_spi_de_init(void);
int32_t hci_tl_spi_reset(void);
int32_t hci_tl_spi_send(uint8_t *buffer, uint16_t size);
void    hci_tl_lowlevel_init(void);
void    hci_tl_lowlevel_isr(void);

#ifdef __cplusplus
}
#endif
#endif /* HCI_TL_INTERFACE_H */

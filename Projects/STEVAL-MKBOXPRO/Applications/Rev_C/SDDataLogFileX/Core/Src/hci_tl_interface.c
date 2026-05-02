/**
  ******************************************************************************
  * @file    hci_tl_interface.c
  * @brief   STM32 ↔ BlueNRG HCI transport layer over SPI1.
  *
  * Adapted from BLEDualProgram's hci_tl_interface.c with two changes:
  *   - SPI1 calls go through ble_spi.{h,c} (HAL-direct) instead of
  *     BSP_SPI1_Init/SendRecv from the steval_mkboxpro BSP, so this app
  *     doesn't have to drag in a second BSP that would clash on BSP_*
  *     symbols with the existing SensorTileBoxPro BSP.
  *   - Pin macros come from our self-contained hci_tl_interface.h instead
  *     of steval_mkboxpro_bus.h.
  *
  * The SPI framing protocol itself (header bytes 0x0a/0x0b, 5-byte sync,
  * IRQ-line handshake, byte-count parsing) is identical to the upstream
  * BlueNRG driver — don't change it without checking the chip's HCI spec.
  ******************************************************************************
  */

#include "RTE_Components.h"
#include "hci_tl.h"
#include "hci_tl_interface.h"
#include "ble_spi.h"

#define HEADER_SIZE       5U
#define MAX_BUFFER_SIZE   255U
#define TIMEOUT_DURATION  15U
#define TIMEOUT_IRQ_HIGH  1000U

volatile uint32_t hci_event = 0;
EXTI_HandleTypeDef hexti11;

static void hci_tl_spi_enable_irq(void)  { HAL_NVIC_EnableIRQ(HCI_TL_SPI_EXTI_IRQ_N); }
static void hci_tl_spi_disable_irq(void) { HAL_NVIC_DisableIRQ(HCI_TL_SPI_EXTI_IRQ_N); }
static int32_t is_data_available(void)
{
  return (HAL_GPIO_ReadPin(HCI_TL_SPI_EXTI_PORT, HCI_TL_SPI_EXTI_PIN) == GPIO_PIN_SET);
}

int32_t hci_tl_spi_init(void *pConf)
{
  GPIO_InitTypeDef gpio = {0};
  (void)pConf;

  BUS_EXTI_GPIO_CLK_ENABLE();
  BUS_CS_GPIO_CLK_ENABLE();
  BUS_RST_GPIO_CLK_ENABLE();

  /* IRQ from BlueNRG */
  gpio.Pin  = HCI_TL_SPI_EXTI_PIN;
  gpio.Mode = GPIO_MODE_IT_RISING;
  gpio.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(HCI_TL_SPI_EXTI_PORT, &gpio);

  /* Reset to BlueNRG */
  gpio.Pin   = HCI_TL_RST_PIN;
  gpio.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio.Pull  = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HCI_TL_RST_PORT, &gpio);

  /* CS to BlueNRG (idle high to avoid spurious commands at boot) */
  gpio.Pin = HCI_TL_SPI_CS_PIN;
  HAL_GPIO_Init(HCI_TL_SPI_CS_PORT, &gpio);
  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);

  return BleSpi_Init();
}

int32_t hci_tl_spi_de_init(void)
{
  HAL_GPIO_DeInit(HCI_TL_SPI_EXTI_PORT, HCI_TL_SPI_EXTI_PIN);
  HAL_GPIO_DeInit(HCI_TL_SPI_CS_PORT,   HCI_TL_SPI_CS_PIN);
  HAL_GPIO_DeInit(HCI_TL_RST_PORT,      HCI_TL_RST_PIN);
  return 0;
}

int32_t hci_tl_spi_reset(void)
{
  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(HCI_TL_RST_PORT,    HCI_TL_RST_PIN,    GPIO_PIN_RESET);
  HAL_Delay(5);
  HAL_GPIO_WritePin(HCI_TL_RST_PORT,    HCI_TL_RST_PIN,    GPIO_PIN_SET);
  HAL_Delay(150);
  return 0;
}

int32_t hci_tl_spi_receive(uint8_t *buffer, uint16_t size)
{
  uint16_t byte_count;
  uint16_t len = 0;
  uint8_t  char_00 = 0x00;
  volatile uint8_t read_char;

  uint8_t header_master[HEADER_SIZE] = {0x0b, 0x00, 0x00, 0x00, 0x00};
  uint8_t header_slave[HEADER_SIZE];

  if (HAL_GPIO_ReadPin(HCI_TL_SPI_IRQ_PORT, HCI_TL_SPI_IRQ_PIN) != GPIO_PIN_SET) {
    return 0;
  }

  hci_tl_spi_disable_irq();
  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_RESET);

  BleSpi_SendRecv(header_master, header_slave, HEADER_SIZE);

  byte_count = (header_slave[4] << 8) | header_slave[3];

  if (byte_count > 0) {
    if (byte_count > size) {
      byte_count = size;
    }
    for (len = 0; len < byte_count; len++) {
      BleSpi_SendRecv(&char_00, (uint8_t *)&read_char, 1);
      buffer[len] = read_char;
    }
  }

  /* Wait for IRQ to drop, bounded by TIMEOUT_IRQ_HIGH */
  uint32_t tickstart = HAL_GetTick();
  while ((HAL_GetTick() - tickstart) < TIMEOUT_IRQ_HIGH) {
    if (HAL_GPIO_ReadPin(HCI_TL_SPI_IRQ_PORT, HCI_TL_SPI_IRQ_PIN) == GPIO_PIN_RESET) {
      break;
    }
  }

  hci_tl_spi_enable_irq();
  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
  return len;
}

int32_t hci_tl_spi_send(uint8_t *buffer, uint16_t size)
{
  int32_t result;
  uint16_t rx_bytes;
  uint8_t header_master[HEADER_SIZE] = {0x0a, 0x00, 0x00, 0x00, 0x00};
  uint8_t header_slave[HEADER_SIZE];
  static uint8_t read_char_buf[MAX_BUFFER_SIZE];
  uint32_t tickstart = HAL_GetTick();

  hci_tl_spi_disable_irq();

  do {
    uint32_t tickstart_data_available = HAL_GetTick();
    result = 0;

    HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_RESET);

    while (!is_data_available()) {
      if ((HAL_GetTick() - tickstart_data_available) > TIMEOUT_DURATION) {
        result = -3;
        break;
      }
    }
    if (result == -3) {
      HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
      break;
    }

    BleSpi_SendRecv(header_master, header_slave, HEADER_SIZE);
    rx_bytes = (((uint16_t)header_slave[2]) << 8) | ((uint16_t)header_slave[1]);

    if (rx_bytes >= size) {
      BleSpi_SendRecv(buffer, read_char_buf, size);
    } else {
      result = -2;
    }

    HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);

    if ((HAL_GetTick() - tickstart) > TIMEOUT_DURATION) {
      result = -3;
      break;
    }
  } while (result < 0);

  tickstart = HAL_GetTick();
  while ((HAL_GetTick() - tickstart) < TIMEOUT_IRQ_HIGH) {
    if (HAL_GPIO_ReadPin(HCI_TL_SPI_IRQ_PORT, HCI_TL_SPI_IRQ_PIN) == GPIO_PIN_RESET) {
      break;
    }
  }

  hci_tl_spi_enable_irq();
  return result;
}

void hci_tl_lowlevel_init(void)
{
  /* Stack init wires the EXTI callback in init_ble_int_for_blue_nrglp() in
     ble_implementation.c — nothing to do here. */
}

void hci_tl_lowlevel_isr(void)
{
  while (is_data_available()) {
    if (hci_notify_asynch_evt(NULL)) {
      return;
    }
  }
  hci_event = 1;
}

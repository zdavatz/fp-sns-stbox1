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
#include "tx_api.h"
#include <stdio.h>

#define HEADER_SIZE       5U
#define MAX_BUFFER_SIZE   255U
#define TIMEOUT_DURATION  15U
#define TIMEOUT_IRQ_HIGH  1000U

volatile uint32_t hci_event = 0;
EXTI_HandleTypeDef hexti11;

static void hci_tl_spi_enable_irq(void)  { HAL_NVIC_EnableIRQ(HCI_TL_SPI_EXTI_IRQ_N); }
static void hci_tl_spi_disable_irq(void) { HAL_NVIC_DisableIRQ(HCI_TL_SPI_EXTI_IRQ_N); }
int32_t is_data_available(void)
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
  extern void ErrorLog_Write(const char *msg);
  ErrorLog_Write("spi_reset: entered (v8-drain-fix)");
  /* Mask the chip's IRQ line during reset. init_ble_int_for_blue_nrglp
     already armed EXTI11 by the time this function runs, so when the
     chip is released from reset (RST high) it raises its IRQ line and
     EXTI11 fires immediately — the resulting hci_tl_lowlevel_isr then
     tries to read HCI bytes from a chip that hasn't finished booting,
     and that read does not return cleanly. We sidestep that by masking
     the IRQ during the entire reset pulse and unmasking after the chip
     has settled. */
  hci_tl_spi_disable_irq();
  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(HCI_TL_RST_PORT,    HCI_TL_RST_PIN,    GPIO_PIN_RESET);
  tx_thread_sleep(1);     /* >= 10 ms, was HAL_Delay(5) */
  ErrorLog_Write("spi_reset: post-sleep-1 (rst released next)");
  HAL_GPIO_WritePin(HCI_TL_RST_PORT,    HCI_TL_RST_PIN,    GPIO_PIN_SET);
  tx_thread_sleep(15);    /* 150 ms, was HAL_Delay(150) */
  ErrorLog_Write("spi_reset: post-sleep-2 (chip settled)");
  /* Clear any pending EXTI11 latched while masked.
     v38: do NOT re-enable EXTI here. EXTI gets re-armed after
     bluetooth_init by init_ble_int_for_blue_nrglp(). During init we
     drain in thread context. */
  __HAL_GPIO_EXTI_CLEAR_IT(HCI_TL_SPI_EXTI_PIN);
  ErrorLog_Write("spi_reset: exti cleared");
  HAL_NVIC_ClearPendingIRQ(HCI_TL_SPI_EXTI_IRQ_N);
  ErrorLog_Write("spi_reset: nvic cleared (v38: not re-enabling)");

  /* v35 hypothesis fix: while EXTI11 was masked above, the chip's
     IRQ line went HIGH (boot-ready event with pending data — we see
     7 bytes pending in slave[3..4]=07 00 on the very first SPI
     transaction afterward). EXTI11 is rising-edge-triggered, so once
     we re-enable, the already-high line never produces a fresh edge
     and the ISR never fires. The chip's pending data therefore
     stays in its TX FIFO forever, IRQ stays HIGH, and *no* future
     event can produce a rising edge. hci_send_req's busy-wait then
     loops forever waiting for the rx_queue to fill.
     Force-drain here: as long as the chip's IRQ pin is high, pull
     events through hci_notify_asynch_evt. Caps at 32 events to bound
     time. Counts get logged so we can verify in the error log
     whether the drain found anything. Cap is high because the chip
     can send multiple boot events on first power-up. */
  extern int32_t hci_notify_asynch_evt(void *pdata);
  int drain = 0;
  while (is_data_available() && drain < 32) {
    hci_notify_asynch_evt(NULL);
    drain++;
  }
  {
    char m[48];
    sprintf(m, "spi_reset: drained %d boot events", drain);
    ErrorLog_Write(m);
  }
  ErrorLog_Write("spi_reset: returning");
  return 0;
}

int32_t hci_tl_spi_receive(uint8_t *buffer, uint16_t size)
{
  extern void ErrorLog_Write(const char *msg);
  static uint32_t recv_call_count = 0;
  recv_call_count++;
  uint16_t byte_count;
  uint16_t len = 0;
  uint8_t  char_00 = 0x00;
  volatile uint8_t read_char;

  uint8_t header_master[HEADER_SIZE] = {0x0b, 0x00, 0x00, 0x00, 0x00};
  uint8_t header_slave[HEADER_SIZE];

  if (HAL_GPIO_ReadPin(HCI_TL_SPI_IRQ_PORT, HCI_TL_SPI_IRQ_PIN) != GPIO_PIN_SET) {
    return 0;
  }
  if (recv_call_count == 1U) ErrorLog_Write("recv[1]: IRQ high, CS LOW + xfer header next");

  hci_tl_spi_disable_irq();
  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_RESET);

  int32_t hdr_rc = BleSpi_SendRecv(header_master, header_slave, HEADER_SIZE);
  if (recv_call_count == 1U) {
    char m[80];
    sprintf(m, "recv[1]: header rc=%ld slave[3..4]=%02x %02x",
            (long)hdr_rc, header_slave[3], header_slave[4]);
    ErrorLog_Write(m);
  }

  byte_count = (header_slave[4] << 8) | header_slave[3];

  if (byte_count > 0) {
    if (byte_count > size) {
      byte_count = size;
    }
    if (recv_call_count == 1U) {
      char m[64];
      sprintf(m, "recv[1]: reading %u payload bytes", (unsigned)byte_count);
      ErrorLog_Write(m);
    }
    for (len = 0; len < byte_count; len++) {
      BleSpi_SendRecv(&char_00, (uint8_t *)&read_char, 1);
      buffer[len] = read_char;
    }
    if (recv_call_count == 1U) ErrorLog_Write("recv[1]: payload read complete");
  }

  /* Cycle-counted busy-wait for IRQ to drop. ~1 ms upper bound. */
  for (volatile uint32_t i = 0; i < 160000U; i++) {
    if (HAL_GPIO_ReadPin(HCI_TL_SPI_IRQ_PORT, HCI_TL_SPI_IRQ_PIN) == GPIO_PIN_RESET) {
      break;
    }
  }

  /* v38: do NOT re-enable EXTI11 here. Same reason as spi_send: during
     init we drain in thread context and don't need EXTI on. After
     bluetooth_init returns, init_ble_int_for_blue_nrglp() arms it. */
  HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
  if (recv_call_count == 1U) {
    char m[40];
    sprintf(m, "recv[1]: returning len=%u", (unsigned)len);
    ErrorLog_Write(m);
  }
  return len;
}

int32_t hci_tl_spi_send(uint8_t *buffer, uint16_t size)
{
  /* All timeouts here use tx_time_get (ThreadX ticks @ 10 ms each) instead
     of HAL_GetTick — the BLE thread has trouble seeing HAL ticks advance,
     so the original HAL_GetTick-based timeouts spin forever waiting for
     a chip response that never arrives. tx_time_get is reliable. */
  extern void ErrorLog_Write(const char *msg);
  int32_t result;
  uint16_t rx_bytes;
  uint8_t header_master[HEADER_SIZE] = {0x0a, 0x00, 0x00, 0x00, 0x00};
  uint8_t header_slave[HEADER_SIZE];
  static uint8_t read_char_buf[MAX_BUFFER_SIZE];
  ULONG tickstart = tx_time_get();

  hci_tl_spi_disable_irq();
  ErrorLog_Write("spi_send: irq disabled, CS low next");

  do {
    ULONG tickstart_data_available = tx_time_get();
    result = 0;

    HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_RESET);

    while (!is_data_available()) {
      /* TIMEOUT_DURATION is 15 ms — round up to 2 ticks (20 ms). */
      if ((tx_time_get() - tickstart_data_available) > 2U) {
        result = -3;
        break;
      }
    }
    if (result == -3) {
      ErrorLog_Write("spi_send: chip IRQ never went HIGH (chip silent or dead)");
      HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
      break;
    }

    ErrorLog_Write("spi_send: chip IRQ HIGH, sending header");
    int32_t hdr_rc = BleSpi_SendRecv(header_master, header_slave, HEADER_SIZE);
    {
      char m[64];
      sprintf(m, "spi_send: header xfer rc=%ld slave[1..4]=%02x %02x %02x %02x",
              (long)hdr_rc, header_slave[1], header_slave[2],
              header_slave[3], header_slave[4]);
      ErrorLog_Write(m);
    }
    rx_bytes = (((uint16_t)header_slave[2]) << 8) | ((uint16_t)header_slave[1]);

    if (rx_bytes >= size) {
      ErrorLog_Write("spi_send: rx_bytes ok, sending payload");
      int32_t pl_rc = BleSpi_SendRecv(buffer, read_char_buf, size);
      {
        char m[48];
        sprintf(m, "spi_send: payload xfer rc=%ld", (long)pl_rc);
        ErrorLog_Write(m);
      }
    } else {
      ErrorLog_Write("spi_send: rx_bytes too small, retrying");
      result = -2;
    }

    HAL_GPIO_WritePin(HCI_TL_SPI_CS_PORT, HCI_TL_SPI_CS_PIN, GPIO_PIN_SET);
    ErrorLog_Write("spi_send: CS HIGH after payload");

    if ((tx_time_get() - tickstart) > 2U) {
      result = -3;
      break;
    }
  } while (result < 0);
  ErrorLog_Write("spi_send: do-while exit");

  /* v37: removed tx_thread_sleep(2) — was suspected to hang.
     BLEDualProgram doesn't have it and works fine. The chip's IRQ
     drops within microseconds anyway. */

  /* v38: do NOT re-enable EXTI11 here. We saw v37 hang precisely at
     hci_tl_spi_enable_irq() — the EXTI->PR pending flag from the chip
     raising its IRQ during the just-completed SPI transaction caused
     the ISR to fire IMMEDIATELY upon enable, and the nested SPI access
     from hci_notify_asynch_evt collided with our thread-context state.
     EXTI gets re-armed after bluetooth_init returns by
     init_ble_int_for_blue_nrglp() in ble_sync.c. During init we drain
     in thread context only. */
  ErrorLog_Write("spi_send: skipping enable_irq (v38)");

  /* In-thread drain: pull any pending events the chip prepared while
     EXTI was masked. Caps at 32 to bound time. */
  extern int32_t hci_notify_asynch_evt(void *pdata);
  int drain = 0;
  while (is_data_available() && drain < 32) {
    hci_notify_asynch_evt(NULL);
    drain++;
  }
  {
    char m[48];
    sprintf(m, "spi_send: post-tx drained %d events", drain);
    ErrorLog_Write(m);
  }

  ErrorLog_Write("spi_send: returning");
  return result;
}

void hci_tl_lowlevel_init(void)
{
  /* Stack init wires the EXTI callback in init_ble_int_for_blue_nrglp() in
     ble_implementation.c — nothing to do here. */
}

void hci_tl_lowlevel_isr(void)
{
  /* Bound the drain loop. If the BlueNRG IRQ pin is stuck high (chip in a
     bad state, or it never boots cleanly), the original `while
     (is_data_available())` spins forever inside the ISR — at the original
     NVIC priority 0 that preempted SDMMC + SysTick + everything, and
     fx_thread could never resume mid-write. Caps the loop to 16 events
     per IRQ; if the line is still high after that, we just exit and the
     next pending edge will re-fire the handler at its (now lowered)
     priority, giving the rest of the system a chance to run. */
  for (uint8_t i = 0; i < 16U; i++) {
    if (!is_data_available()) {
      break;
    }
    if (hci_notify_asynch_evt(NULL)) {
      return;
    }
  }
  hci_event = 1;
}

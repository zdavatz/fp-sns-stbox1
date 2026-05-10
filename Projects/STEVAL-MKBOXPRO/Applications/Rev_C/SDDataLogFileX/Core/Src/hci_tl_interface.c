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
#include "tx_api.h"  /* tx_thread_sleep — yields in BLE busy-waits (issue #12) */
#include <stdio.h>   /* printf — diagnostic prints for issue #12 bisects */

#define HEADER_SIZE       5U
#define MAX_BUFFER_SIZE   255U
#define TIMEOUT_DURATION  15U
#define TIMEOUT_IRQ_HIGH  1000U

volatile uint32_t hci_event = 0;
EXTI_HandleTypeDef hexti11;

static void hci_tl_spi_enable_irq(void)
{
  /* Issue #12 / 2026-05-10 / BISECT-G: priority 14 (same as SDMMC1)
   * was NOT enough. ARM Cortex-M with NVIC_PRIORITYGROUP_4 means
   * priority value = preempt level. Same level = no preemption,
   * IRQs serialize. EXTI11 ISR (running 16 events × ~1 ms busy-wait =
   * ~16 ms) blocks SDMMC1 transfer-complete IRQ → fx_media_flush
   * semaphore never signals → fx_thread hangs. Set EXTI11 to
   * priority 15 (lowest possible in 4-bit group) so SDMMC1 (priority
   * 14) PREEMPTS EXTI11 ISR. Now SDMMC's transfer-complete signal
   * fires immediately even mid-EXTI11. */
  HAL_NVIC_SetPriority(HCI_TL_SPI_EXTI_IRQ_N, 15, 0);
  HAL_NVIC_EnableIRQ(HCI_TL_SPI_EXTI_IRQ_N);
}
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
  extern UINT ErrorLog_Flush(void);

  /* BISECT-C/D/E PASSED — RST low / RST high / RST high + 150 ms wait
   * are all safe. */
  HAL_GPIO_WritePin(HCI_TL_RST_PORT,    HCI_TL_RST_PIN,    GPIO_PIN_SET);
  tx_thread_sleep(15);    /* 150 ms wait for BlueNRG-LP boot */
  ErrorLog_Write("spi_reset: post-sleep-2 (chip settled)");
  ErrorLog_Flush();

  /* Clear any pending EXTI11 latched while masked. KEEP EXTI11 NVIC
   * MASKED throughout the entire BLE init sequence (probe + bluetooth_init).
   * Issue #12 / 2026-05-10: re-enabling EXTI11 NVIC while chip's IRQ
   * pin is HIGH (boot-pending events) wedges SDMMC's transfer-complete
   * IRQ servicing — fx_media_flush hangs. Even priority 15 doesn't
   * help. Solution: no enable_irq during init. The BLE thread arms
   * EXTI11 ONCE after bluetooth_init returns successfully (in
   * init_ble_int_for_blue_nrglp). All SPI traffic during init runs
   * with EXTI11 masked. The middleware polls for chip responses
   * (is_data_available) so it doesn't need ISR-driven notification
   * during init. */
  __HAL_GPIO_EXTI_CLEAR_IT(HCI_TL_SPI_EXTI_PIN);
  HAL_NVIC_ClearPendingIRQ(HCI_TL_SPI_EXTI_IRQ_N);
  ErrorLog_Write("spi_reset: exti cleared (NVIC stays masked through init)");
  ErrorLog_Flush();
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

  /* Issue #14 / 2026-05-10: re-enable EXTI11 NVIC like
   * BLEDualProgram's hci_tl_interface.c does. Without re-enable,
   * after the first receive EXTI11 stays disabled and no further
   * chip events get drained → middleware deadlock. The earlier issue
   * #12 SDMMC concern is now addressed at a different layer: BLE
   * thread suspends fx_thread + masks SDMMC1 IRQ for the bluetooth_init
   * window, so re-enable here is harmless during init. */
  hci_tl_spi_enable_irq();
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

    /* End-of-loop timeout removed (issue #12, 2026-05-10): after a
     * successful payload xfer result=0 → exits the do-while normally.
     * The original 20 ms (2-tick) end-check fired AFTER success and
     * clobbered result back to -3 because the multiple ErrorLog_Write
     * calls in the loop body each take several ms of SDMMC time. The
     * inner 20 ms timeout on `is_data_available` already bounds the
     * "chip silent" failure mode; nothing else justifies retrying with
     * a separate top-level wallclock cap. */
  } while (result < 0);

  /* Short sleep — chip's IRQ drops within microseconds normally. Was
   * a 1 s busy-wait on TIMEOUT_IRQ_HIGH which starved USB-CDC service
   * thread for the full timeout when the chip didn't actually need a
   * wait. Replaced with 20 ms tx_thread_sleep yield (Peter's v35). */
  tx_thread_sleep(2);

  /* Issue #14 / 2026-05-10: re-enable EXTI11 NVIC after each spi_send
   * to match BLEDualProgram's pattern. Without re-enable, EXTI11
   * stays disabled after the first send → no more chip events fire
   * the ISR → middleware's hci_send_req busy-waits forever on empty
   * rx_queue. Safe during init (fx_thread suspended, SDMMC1 IRQ
   * masked); harmless post-init since BLE events are infrequent. */
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
  /* Heavy drain restored (issue #14 / 2026-05-10). Middleware's
   * hci_send_req busy-waits on hci_read_pkt_rx_queue (Basic/hci_tl.c
   * line 320) and only progresses when the queue is populated — that
   * population is exactly what hci_notify_asynch_evt does. A thin ISR
   * that just set hci_event=1 broke this; nobody ever called
   * hci_user_evt_proc to drain because the BLE thread was stuck in
   * hci_send_req's busy-wait.
   *
   * SDMMC concurrency is now handled at a different layer: BLE thread
   * suspends fx_thread + masks SDMMC1 IRQ for the bluetooth_init
   * window (see ble_sync.c). Inside that window the ISR's busy-wait
   * is harmless since SDMMC isn't running. After bluetooth_init
   * returns, BLE chip events are infrequent and the ISR runs only
   * briefly. */
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

/* USB CDC ACM debug console for SDDataLogFileX (STM32U585).
 *
 * Brings up the OTG_FS peripheral via TinyUSB, exposes a CDC ACM virtual
 * COM port, and overrides __io_putchar so STBOX1_PRINTF / printf land on USB.
 *
 * Architecture:
 *   - UsbCdc_Init() runs from main() AFTER SystemClock_Config (HSI48 must
 *     be on) and BEFORE tx_kernel_enter.
 *   - UsbCdc_ThreadX_Init() is called from App_ThreadX_Init() and spawns
 *     a service thread (priority 13) that loops tud_task() + tx_thread_sleep(1).
 *   - The OTG_FS_IRQHandler in stm32u5xx_it.c forwards to tud_int_handler(0).
 *   - __io_putchar() at the bottom of this file pushes one char to the CDC
 *     TX FIFO non-blockingly. The newlib stdio plumbing in syscalls.c calls
 *     __io_putchar for each byte _write emits, so existing STBOX1_PRINTF
 *     just works.
 */
#include "stbox1_config.h"

#if STBOX1_ENABLE_USB_CDC

#include <string.h>
#include "tx_api.h"
#include "tusb.h"
#include "stm32u5xx_hal.h"
#include "main.h"

/* ---------------------------------------------------------------------------
 * USB peripheral hardware bring-up
 * --------------------------------------------------------------------------*/
static void usb_cdc_hw_init(void) {
  /* Enable VDDUSB power island. STM32U5 puts the USB transceiver behind a
   * separate supply rail — without this, OTG_FS register writes succeed but
   * the line stays floating and no host enumerates. */
  HAL_PWREx_EnableVddUSB();

  /* Route HSI48 to the USB / SDMMC / RNG intermediate clock (ICLK).
   * STM32U5 routes all three through one selector — SDMMC and RNG aren't
   * touched here because SDMMC is already running with its own dedicated
   * clock setup in SystemClock_Config / BSP_SD_Init. HSI48 is already ON
   * (RCC_OSCILLATORTYPE_HSI48 in SystemClock_Config). */
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ICLK;
  PeriphClkInit.IclkClockSelection   = RCC_ICLK_CLKSOURCE_HSI48;
  HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

  /* Enable peripheral clock for OTG_FS. STM32U5: SYSCFG_OTGHSPHYCR is for
   * the HS PHY which we don't use; the FS path lives behind RCC_AHB2ENR1. */
  __HAL_RCC_USB_OTG_FS_CLK_ENABLE();

  /* GPIO config — PA11 (DM) and PA12 (DP) on AF10 (USB_OTG_FS). */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
  GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull      = GPIO_NOPULL;
  GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF10_USB;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Arm the OTG_FS interrupt. Priority 13 — between SDMMC1 (14) so a USB
   * event can't preempt an in-flight SD write, and UART4-GPS (6) so a USB
   * burst doesn't drop NMEA bytes. tud_int_handler bound in stm32u5xx_it.c. */
  HAL_NVIC_SetPriority(OTG_FS_IRQn, 13, 0);
  HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
}

/* ---------------------------------------------------------------------------
 * TinyUSB device init — exported entry called from main()
 * --------------------------------------------------------------------------*/
void UsbCdc_Init(void) {
  usb_cdc_hw_init();
  /* Non-deprecated form of tud_init(0). */
  const tusb_rhport_init_t rh_init = {
    .role  = TUSB_ROLE_DEVICE,
    .speed = TUSB_SPEED_FULL,
  };
  tud_rhport_init(0, &rh_init);
}

/* ---------------------------------------------------------------------------
 * Service thread — drives tud_task() at ~1 kHz
 * --------------------------------------------------------------------------*/
#define USB_THREAD_STACK_SIZE  2048
static TX_THREAD usb_thread;
static UCHAR    *usb_thread_stack = NULL;

static void usb_thread_entry(ULONG arg) {
  (void) arg;
  for (;;) {
    tud_task();      /* Process control transfers, EP completions, etc. */
    tud_cdc_write_flush();  /* Push any buffered TX bytes to the host. */
    tx_thread_sleep(1);     /* 10 ms tick — plenty for printf / control. */
  }
}

unsigned int UsbCdc_ThreadX_Init(void *memory_ptr) {
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL *) memory_ptr;

  if (tx_byte_allocate(byte_pool, (VOID **) &usb_thread_stack,
                       USB_THREAD_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS) {
    return TX_POOL_ERROR;
  }

  return tx_thread_create(&usb_thread, "usb_cdc",
                          usb_thread_entry, 0,
                          usb_thread_stack, USB_THREAD_STACK_SIZE,
                          13, 13,            /* priority + preempt threshold */
                          TX_NO_TIME_SLICE,
                          TX_AUTO_START);
}

/* ---------------------------------------------------------------------------
 * Write API + __io_putchar override
 * --------------------------------------------------------------------------*/
size_t UsbCdc_Write(const void *buf, size_t len) {
  /* If the host hasn't opened the CDC port (no DTR), drop bytes silently.
   * Otherwise we'd back-pressure on the producer (printf in any thread or
   * IRQ context) and risk priority inversion against the SD writer. */
  if (!tud_cdc_connected()) return 0u;

  /* tud_cdc_write copies into TinyUSB's internal FIFO; if FIFO is full the
   * extra bytes are dropped. Same non-blocking guarantee as above. */
  return tud_cdc_write(buf, (uint32_t) len);
}

size_t UsbCdc_WriteString(const char *s) {
  return UsbCdc_Write(s, strlen(s));
}

/* Strong override of the weak _write in syscalls.c. The newlib stdio plumbing
 * (printf, puts, fwrite, etc.) lands here with one big buffer. We push the
 * whole batch into the CDC TX FIFO in one go — the service thread's
 * tud_cdc_write_flush() drains it to the host.
 *
 * NOT routing through BSP's __io_putchar (defined in
 * Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro.c when USE_COM_LOG=1) because
 * that one targets UART4 which is now owned by the GPS module — printf bytes
 * would corrupt NMEA. By taking _write directly, BSP's __io_putchar becomes
 * unreachable and the linker DCEs it. */
int _write(int file, char *ptr, int len) {
  (void) file;
  return (int) UsbCdc_Write(ptr, (size_t) len);
}

#endif /* STBOX1_ENABLE_USB_CDC */

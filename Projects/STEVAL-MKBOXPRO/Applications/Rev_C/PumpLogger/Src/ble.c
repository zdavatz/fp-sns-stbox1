/**
  ******************************************************************************
  * @file    ble.c
  * @brief   BlueNRG-LP HCI driver — minimal subset, polling only.
  *          Wiring (lifted from BLEDualProgram::hci_tl_interface.h):
  *            SPI1 SCK   PA5  AF5
  *            SPI1 MOSI  PA7  AF5
  *            SPI1 MISO  PA6  AF5
  *            CS         PA2  GPIO output, idle HIGH
  *            RESET      PD4  GPIO output, idle HIGH (active LOW)
  *            IRQ        PB11 GPIO input (NO NVIC — we poll the level
  *                            during BLE_Tick / inside SPI transactions)
  *
  *          BlueNRG SPI protocol (5-byte header):
  *            Write:  master sends {0x0a,0,0,0,0}, slave returns its
  *                    available-buffer-size in bytes 1-2 (LE).
  *            Read:   master sends {0x0b,0,0,0,0}, slave returns its
  *                    pending-byte-count in bytes 3-4 (LE).
  *
  *          HCI frame format on the wire:
  *            Command: 0x01 <opcode_lo> <opcode_hi> <param_len> <params...>
  *            Event:   0x04 <event_code> <param_len> <params...>
  *
  *          Minimum advertising sequence for Phase 4:
  *            HCI_RESET                       (opcode 0x0C03)
  *            HCI_LE_Set_Advertising_Params   (opcode 0x2006)
  *            HCI_LE_Set_Advertising_Data     (opcode 0x2008)
  *            HCI_LE_Set_Advertising_Enable   (opcode 0x200A)
  ******************************************************************************
  */
#include "main.h"
#include "ble.h"
#include "errlog.h"
#include <string.h>
#include <stdio.h>

/* ----- Pin / port macros ------------------------------------------------- */
#define BLE_SPI_SCK_PORT   GPIOA
#define BLE_SPI_SCK_PIN    GPIO_PIN_5
#define BLE_SPI_MISO_PORT  GPIOA
#define BLE_SPI_MISO_PIN   GPIO_PIN_6
#define BLE_SPI_MOSI_PORT  GPIOA
#define BLE_SPI_MOSI_PIN   GPIO_PIN_7
#define BLE_SPI_AF         GPIO_AF5_SPI1
#define BLE_CS_PORT        GPIOA
#define BLE_CS_PIN         GPIO_PIN_2
#define BLE_RST_PORT       GPIOD
#define BLE_RST_PIN        GPIO_PIN_4
#define BLE_IRQ_PORT       GPIOB
#define BLE_IRQ_PIN        GPIO_PIN_11

/* ----- HCI opcodes ------------------------------------------------------- */
#define HCI_OP_RESET                    0x0C03

/* ACI (BlueNRG-LP vendor) opcodes — OGF=0x3F, packed as (OGF<<10)|OCF.
   The advertising flow uses the BT 5.0 extended set:
     SET_ADVERTISING_CONFIGURATION → SET_ADVERTISING_DATA_NWK →
     SET_ADVERTISING_ENABLE
   Standard HCI_LE_Set_Adv_* commands are rejected (status 7) after
   aci_gap_init takes over the advertising machinery — verified in Build #17. */
#define ACI_OP_GAP_INIT                       0xFC8A   /* OCF=0x08A */
#define ACI_OP_GAP_SET_ADV_CONFIGURATION      0xFCAB   /* OCF=0x0AB */
#define ACI_OP_GAP_SET_ADV_ENABLE             0xFCAC   /* OCF=0x0AC */
#define ACI_OP_GAP_SET_ADV_DATA_NWK           0xFCAD   /* OCF=0x0AD */

/* ----- State ------------------------------------------------------------- */
static SPI_HandleTypeDef g_hspi1;
static uint8_t           g_advertising = 0;

#define BLE_ADV_NAME      "PumpTsueri"
#define BLE_ADV_NAME_LEN  10

/* ----- Pin helpers ------------------------------------------------------- */
static inline void cs_lo(void) { HAL_GPIO_WritePin(BLE_CS_PORT, BLE_CS_PIN, GPIO_PIN_RESET); }
static inline void cs_hi(void) { HAL_GPIO_WritePin(BLE_CS_PORT, BLE_CS_PIN, GPIO_PIN_SET); }
static inline int  irq_high(void) {
  return HAL_GPIO_ReadPin(BLE_IRQ_PORT, BLE_IRQ_PIN) == GPIO_PIN_SET;
}

/* ----- SPI transfer ------------------------------------------------------ */

static int spi_xfer(const uint8_t *tx, uint8_t *rx, uint16_t n)
{
  /* Caller must hold CS. We use HAL polling with a generous timeout —
     transfers are tiny (max ~64 bytes per HCI command). */
  HAL_StatusTypeDef s = HAL_SPI_TransmitReceive(&g_hspi1, (uint8_t *)tx, rx, n, 100);
  return (s == HAL_OK) ? 0 : -1;
}

/* Diagnostic snapshots of the last header exchanges — emitted to ErrLog
   during init so we can see exactly what the chip reports. */
static uint8_t  g_diag_last_send_hdr[5];
static uint8_t  g_diag_last_recv_hdr[5];

/* Send the BlueNRG 5-byte write-mode header. Returns chip's reported
   available write buffer size (bytes), or -1 on SPI error. */
static int ble_spi_send_header(void)
{
  uint8_t tx[5] = { 0x0a, 0, 0, 0, 0 };
  uint8_t rx[5] = { 0 };
  if (spi_xfer(tx, rx, 5) != 0) return -1;
  memcpy(g_diag_last_send_hdr, rx, 5);
  /* per ST source: rx_bytes = (header_slave[2] << 8) | header_slave[1] */
  return (int)((((uint16_t)rx[2]) << 8) | (uint16_t)rx[1]);
}

/* Send the BlueNRG 5-byte read-mode header. Returns chip's pending byte
   count, or -1 on SPI error. */
static int ble_spi_recv_header(void)
{
  uint8_t tx[5] = { 0x0b, 0, 0, 0, 0 };
  uint8_t rx[5] = { 0 };
  if (spi_xfer(tx, rx, 5) != 0) return -1;
  memcpy(g_diag_last_recv_hdr, rx, 5);
  /* per ST source: byte_count = (header_slave[4] << 8) | header_slave[3] */
  return (int)((((uint16_t)rx[4]) << 8) | (uint16_t)rx[3]);
}

/* Send a HCI command + payload to BlueNRG-LP. Loops with timeout until the
   chip is ready (IRQ HIGH + write-header reports enough buffer space). */
static int ble_hci_send(const uint8_t *frame, uint16_t len)
{
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < 100) {
    cs_lo();

    /* Wait for IRQ pin high — chip ready to be addressed. */
    uint32_t s2 = HAL_GetTick();
    while (!irq_high()) {
      if ((HAL_GetTick() - s2) > 15) { cs_hi(); goto retry; }
    }

    int rx_avail = ble_spi_send_header();
    if (rx_avail < (int)len) {
      cs_hi();
      goto retry;
    }

    /* HAL_SPI_TransmitReceive requires both buffers non-NULL — Build #15
       passed NULL for the discard buffer and got HAL_ERROR back. Use a
       static scratch buffer big enough for any HCI command (max ~260 B). */
    static uint8_t s_rx_discard[260];
    uint16_t xn = (len > sizeof(s_rx_discard)) ? sizeof(s_rx_discard) : len;
    if (spi_xfer(frame, s_rx_discard, xn) != 0) { cs_hi(); return -2; }

    cs_hi();
    /* Wait IRQ falls (transaction complete). */
    uint32_t s3 = HAL_GetTick();
    while (irq_high()) {
      if ((HAL_GetTick() - s3) > 100) break;
    }
    return 0;

retry:
    HAL_Delay(1);
  }
  return -3;
}

/* Drain one HCI event (if any) from the chip into `buf` (size cap). Returns
   bytes received (0 = no event yet, <0 = error). */
static int ble_hci_recv(uint8_t *buf, uint16_t cap)
{
  if (!irq_high()) return 0;

  cs_lo();
  int byte_count = ble_spi_recv_header();
  if (byte_count <= 0) { cs_hi(); return 0; }
  if (byte_count > cap) byte_count = cap;

  uint8_t zero = 0;
  for (int i = 0; i < byte_count; i++) {
    if (spi_xfer(&zero, &buf[i], 1) != 0) { cs_hi(); return -1; }
  }

  cs_hi();
  /* Wait IRQ falls. */
  uint32_t s = HAL_GetTick();
  while (irq_high()) { if ((HAL_GetTick() - s) > 100) break; }
  return byte_count;
}

/* Send a HCI command (opcode + optional payload) and wait for the matching
   CommandComplete event. Returns the status byte (0=OK) or -1 on timeout. */
static int ble_hci_cmd(uint16_t opcode, const uint8_t *payload, uint8_t plen)
{
  uint8_t frame[260];
  frame[0] = 0x01;                                 /* HCI command packet */
  frame[1] = (uint8_t)(opcode & 0xFF);
  frame[2] = (uint8_t)(opcode >> 8);
  frame[3] = plen;
  if (plen > 0 && payload) memcpy(&frame[4], payload, plen);

  if (ble_hci_send(frame, (uint16_t)(4 + plen)) != 0) return -10;

  /* Poll for CommandComplete event (opcode 0x0E). */
  uint8_t evt[260];
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < 500) {
    int n = ble_hci_recv(evt, sizeof(evt));
    if (n <= 0) { HAL_Delay(2); continue; }
    /* Event format: 0x04 <evt_code> <plen> <params> */
    if (n < 7) continue;
    if (evt[0] != 0x04) continue;
    if (evt[1] != 0x0E) continue;                /* not CommandComplete */
    /* CommandComplete params: <num_pkts> <opcode_lo> <opcode_hi> <status> */
    uint16_t evt_op = (uint16_t)(evt[4] | (evt[5] << 8));
    if (evt_op != opcode) continue;              /* status of another cmd */
    return (int)evt[6];
  }
  return -11;
}

/* ACI extended command frame:
     0x81 <opcode_lo> <opcode_hi> <plen_lo> <plen_hi> <params...>
   BlueNRG-LP vendor commands (OGF=0x3F + OCF) use this format with
   16-bit plen. Response comes back as a normal CommandComplete event
   (0x04 0x0E ...) keyed by the same opcode. */
static int ble_aci_cmd(uint16_t opcode, const uint8_t *payload, uint16_t plen)
{
  uint8_t frame[260];
  frame[0] = 0x81;
  frame[1] = (uint8_t)(opcode & 0xFF);
  frame[2] = (uint8_t)(opcode >> 8);
  frame[3] = (uint8_t)(plen & 0xFF);
  frame[4] = (uint8_t)(plen >> 8);
  if (plen > 0 && payload) memcpy(&frame[5], payload, plen);

  if (ble_hci_send(frame, (uint16_t)(5 + plen)) != 0) return -10;

  uint8_t evt[260];
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < 1000) {
    int n = ble_hci_recv(evt, sizeof(evt));
    if (n <= 0) { HAL_Delay(2); continue; }
    if (n < 7) continue;
    if (evt[0] != 0x04) continue;
    if (evt[1] != 0x0E) continue;
    uint16_t evt_op = (uint16_t)(evt[4] | (evt[5] << 8));
    if (evt_op != opcode) continue;
    return (int)evt[6];
  }
  return -11;
}

/* ----- HW init ----------------------------------------------------------- */

static int ble_hw_init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_SPI1_CLK_ENABLE();

  GPIO_InitTypeDef g = {0};

  /* SPI1 SCK + MOSI + MISO. */
  g.Mode      = GPIO_MODE_AF_PP;
  g.Pull      = GPIO_NOPULL;
  g.Speed     = GPIO_SPEED_FREQ_HIGH;
  g.Alternate = BLE_SPI_AF;
  g.Pin       = BLE_SPI_SCK_PIN; HAL_GPIO_Init(BLE_SPI_SCK_PORT, &g);
  g.Pin       = BLE_SPI_MISO_PIN; HAL_GPIO_Init(BLE_SPI_MISO_PORT, &g);
  g.Pin       = BLE_SPI_MOSI_PIN; HAL_GPIO_Init(BLE_SPI_MOSI_PORT, &g);

  /* CS — output, idle HIGH. */
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  g.Pin  = BLE_CS_PIN;
  HAL_GPIO_Init(BLE_CS_PORT, &g);
  cs_hi();

  /* RESET — output, idle HIGH (active LOW). */
  g.Pin  = BLE_RST_PIN;
  HAL_GPIO_Init(BLE_RST_PORT, &g);
  HAL_GPIO_WritePin(BLE_RST_PORT, BLE_RST_PIN, GPIO_PIN_SET);

  /* IRQ — input only, no NVIC. */
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_PULLDOWN;
  g.Pin  = BLE_IRQ_PIN;
  HAL_GPIO_Init(BLE_IRQ_PORT, &g);

  /* SPI1 config — copied from BSP_SPI1_Init in BLEDualProgram (the proven
     setup for BlueNRG-LP). KEY: SPI Mode 3 (CPOL=HIGH, CPHA=2EDGE) — Build
     #14 with Mode 0 returned garbage MISO bytes (0x7f 0x8e ...). The
     ReadyMaster fields drive the BlueNRG's hardware-handshake on the IRQ
     line. */
  memset(&g_hspi1, 0, sizeof(g_hspi1));
  g_hspi1.Instance               = SPI1;
  g_hspi1.Init.Mode              = SPI_MODE_MASTER;
  g_hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
  g_hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
  g_hspi1.Init.CLKPolarity       = SPI_POLARITY_HIGH;
  g_hspi1.Init.CLKPhase          = SPI_PHASE_2EDGE;
  g_hspi1.Init.NSS               = SPI_NSS_SOFT;
  g_hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;  /* 160 MHz / 128 = 1.25 MHz */
  g_hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
  g_hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
  g_hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
  g_hspi1.Init.CRCPolynomial     = 7;
  g_hspi1.Init.NSSPMode          = SPI_NSS_PULSE_DISABLE;
  g_hspi1.Init.NSSPolarity       = SPI_NSS_POLARITY_LOW;
  g_hspi1.Init.FifoThreshold     = SPI_FIFO_THRESHOLD_01DATA;
  g_hspi1.Init.MasterSSIdleness  = SPI_MASTER_SS_IDLENESS_00CYCLE;
  g_hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  g_hspi1.Init.MasterReceiverAutoSusp  = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  g_hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  g_hspi1.Init.IOSwap            = SPI_IO_SWAP_DISABLE;
  g_hspi1.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
  g_hspi1.Init.ReadyPolarity     = SPI_RDY_POLARITY_HIGH;
  if (HAL_SPI_Init(&g_hspi1) != HAL_OK) return -1;
  return 0;
}

static void ble_chip_reset(void)
{
  cs_hi();
  HAL_GPIO_WritePin(BLE_RST_PORT, BLE_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(10);
  HAL_GPIO_WritePin(BLE_RST_PORT, BLE_RST_PIN, GPIO_PIN_SET);
  /* ST middleware does 150 ms here, then sends hci_reset, then waits another
     2000 ms before issuing any other HCI command. Build #13 with only 200 ms
     here returned cc=-10 (send timeout) on hci_reset → chip wasn't actually
     ready. Bumped to 500 ms; the 2-second post-hci-reset settle is in the
     caller. */
  HAL_Delay(500);
}

/* Drain whatever the chip pushed at us during boot (startup-vendor-event,
   etc.) so the first real HCI exchange isn't fighting a backlog. Returns
   how many events were drained. */
static int ble_drain_pending(uint32_t window_ms)
{
  uint8_t buf[260];
  int drained = 0;
  uint32_t start = HAL_GetTick();
  while ((HAL_GetTick() - start) < window_ms) {
    if (!irq_high()) { HAL_Delay(2); continue; }
    cs_lo();
    int bc = ble_spi_recv_header();
    if (bc <= 0) { cs_hi(); HAL_Delay(2); continue; }
    if (bc > (int)sizeof(buf)) bc = sizeof(buf);
    uint8_t zero = 0;
    for (int i = 0; i < bc; i++) {
      if (spi_xfer(&zero, &buf[i], 1) != 0) break;
    }
    cs_hi();
    drained++;
    /* Brief wait for IRQ to fall before peeking again. */
    uint32_t s = HAL_GetTick();
    while (irq_high()) { if ((HAL_GetTick() - s) > 50) break; }
  }
  return drained;
}

/* ----- Public API -------------------------------------------------------- */

int BLE_Init(void)
{
  char buf[96];

  if (ble_hw_init() != 0) {
    ErrLog_Write("ble: hw_init FAIL");
    return -1;
  }
  ErrLog_Write("ble: spi1+gpio ok");

  ble_chip_reset();
  snprintf(buf, sizeof(buf), "ble: chip reset done irq=%d", irq_high());
  ErrLog_Write(buf);

  /* Drain any startup events the chip pushed at us during boot. ST's
     middleware sees these via the EXTI11 IRQ; we just read them with a
     short polling window. */
  int drained = ble_drain_pending(300);
  snprintf(buf, sizeof(buf), "ble: drained=%d irq=%d hdr=%02x%02x%02x%02x%02x",
           drained, irq_high(),
           g_diag_last_recv_hdr[0], g_diag_last_recv_hdr[1],
           g_diag_last_recv_hdr[2], g_diag_last_recv_hdr[3],
           g_diag_last_recv_hdr[4]);
  ErrLog_Write(buf);

  /* HCI_RESET — first command after reset, validates the SPI link. */
  int rc = ble_hci_cmd(HCI_OP_RESET, NULL, 0);
  snprintf(buf, sizeof(buf), "ble: hci_reset cc=%d sndhdr=%02x%02x%02x%02x%02x",
           rc,
           g_diag_last_send_hdr[0], g_diag_last_send_hdr[1],
           g_diag_last_send_hdr[2], g_diag_last_send_hdr[3],
           g_diag_last_send_hdr[4]);
  ErrLog_Write(buf);
  if (rc != 0) return -2;

  /* ST middleware waits 2 s here for the chip to be fully operational
     before issuing any further HCI commands. */
  HAL_Delay(2000);
  ErrLog_Write("ble: 2s settle done");

  /* aci_gap_init — REQUIRED for BlueNRG-LP before advertising will actually
     start. Standard HCI_LE_Set_Adv_Enable returns cc=0 without it but the
     chip never radiates. Per ble_manager.c:3564:
       role = peripheral, privacy = 0, name_len = strlen("PumpTsueri"),
       identity_address_type = STATIC_RANDOM_ADDR (0x01)
     Returns: status(1) + service_handle(2) + dev_name(2) + appearance(2) = 7 B */
  {
    uint8_t p[4] = {
      0x01,                       /* role: peripheral */
      0x00,                       /* privacy: disabled */
      BLE_ADV_NAME_LEN,           /* device name char len */
      0x01,                       /* identity addr type: static random */
    };
    rc = ble_aci_cmd(ACI_OP_GAP_INIT, p, sizeof(p));
    snprintf(buf, sizeof(buf), "ble: aci_gap_init cc=%d", rc);
    ErrLog_Write(buf);
    if (rc != 0) return -6;
  }

  /* ACI_GAP_SET_ADVERTISING_CONFIGURATION — extended-advertising config.
     Lifted from ble_manager.c::set_connectable_ble (BlueNRG-LP path).
     Payload layout (27 bytes total, little-endian):
       advertising_handle (1)      = 0
       discoverable_mode  (1)      = 0x02 (general discoverable)
       event_properties   (2)      = 0x0013 (CONNECTABLE|SCANNABLE|LEGACY)
       interval_min       (4)      = 0xA0 (160 × 0.625 ms = 100 ms)
       interval_max       (4)      = 0xA0
       channel_map        (1)      = 0x07 (all 3 channels)
       peer_address_type  (1)      = 0x00
       peer_address       (6)      = 0
       filter_policy      (1)      = 0x00 (allow any)
       tx_power           (1)      = 0 (0 dBm)
       primary_phy        (1)      = 0x01 (LE_1M)
       secondary_max_skip (1)      = 0
       secondary_phy      (1)      = 0x01 (LE_1M; unused for legacy)
       advertising_sid    (1)      = 0
       scan_req_notif_en  (1)      = 0
  */
  {
    uint8_t p[27] = {0};
    int i = 0;
    p[i++] = 0x00;                                      /* handle */
    p[i++] = 0x02;                                      /* general discoverable */
    p[i++] = 0x13; p[i++] = 0x00;                       /* event_properties LE */
    p[i++] = 0xA0; p[i++] = 0x00; p[i++] = 0x00; p[i++] = 0x00;  /* interval_min */
    p[i++] = 0xA0; p[i++] = 0x00; p[i++] = 0x00; p[i++] = 0x00;  /* interval_max */
    p[i++] = 0x07;                                      /* channel_map */
    p[i++] = 0x00;                                      /* peer_addr_type */
    for (int k = 0; k < 6; k++) p[i++] = 0;             /* peer_addr */
    p[i++] = 0x00;                                      /* filter_policy */
    p[i++] = 0x00;                                      /* tx_power 0 dBm */
    p[i++] = 0x01;                                      /* primary phy 1M */
    p[i++] = 0x00;                                      /* secondary skip */
    p[i++] = 0x01;                                      /* secondary phy 1M */
    p[i++] = 0x00;                                      /* SID */
    p[i++] = 0x00;                                      /* scan_req_notif */
    rc = ble_aci_cmd(ACI_OP_GAP_SET_ADV_CONFIGURATION, p, (uint16_t)i);
    snprintf(buf, sizeof(buf), "ble: adv_config cc=%d", rc);
    ErrLog_Write(buf);
    if (rc != 0) return -3;
  }

  /* ACI_GAP_SET_ADVERTISING_DATA_NWK
       handle (1) = 0
       operation (1) = 0x03 (complete data)
       adv_data_length (1)
       adv_data (N)
     AD structures inside adv_data:
       AD #1 Flags: [0x02, 0x01, 0x06] (LE general + BR/EDR off)
       AD #2 Name : [name_len+1, 0x09, 'P','u','m','p','T','s','u','e','r','i']
  */
  {
    uint8_t p[64];
    int i = 0;
    p[i++] = 0x00;                                      /* handle */
    p[i++] = 0x03;                                      /* complete data */
    uint8_t name_len = (uint8_t)BLE_ADV_NAME_LEN;
    uint8_t data_len = 3 + (2 + name_len);
    p[i++] = data_len;                                  /* adv_data_length */
    /* Flags */
    p[i++] = 0x02; p[i++] = 0x01; p[i++] = 0x06;
    /* Complete Local Name */
    p[i++] = (uint8_t)(1 + name_len); p[i++] = 0x09;
    memcpy(&p[i], BLE_ADV_NAME, name_len); i += name_len;
    rc = ble_aci_cmd(ACI_OP_GAP_SET_ADV_DATA_NWK, p, (uint16_t)i);
    snprintf(buf, sizeof(buf), "ble: adv_data cc=%d", rc);
    ErrLog_Write(buf);
    if (rc != 0) return -4;
  }

  /* ACI_GAP_SET_ADVERTISING_ENABLE
       enable (1) = 0x01
       number_of_sets (1) = 0x01
       set[0]: handle (1) = 0
               duration (2) = 0 (forever)
               max_extended_events (1) = 0 (unlimited)
  */
  {
    uint8_t p[6] = { 0x01, 0x01, 0x00, 0x00, 0x00, 0x00 };
    rc = ble_aci_cmd(ACI_OP_GAP_SET_ADV_ENABLE, p, sizeof(p));
    snprintf(buf, sizeof(buf), "ble: adv_enable cc=%d", rc);
    ErrLog_Write(buf);
    if (rc != 0) return -5;
  }

  g_advertising = 1;
  ErrLog_Write("ble: advertising as " BLE_ADV_NAME);
  return 0;
}

void BLE_Tick(void)
{
  /* Drain any pending HCI events. For Phase 4 we only care that the link
     stays alive and the chip isn't stuck — events get logged occasionally. */
  if (!g_advertising) return;
  if (!irq_high())   return;

  uint8_t evt[260];
  int n = ble_hci_recv(evt, sizeof(evt));
  if (n > 2) {
    char buf[64];
    snprintf(buf, sizeof(buf), "ble: evt code=0x%02x len=%d", evt[1], evt[2]);
    ErrLog_Write(buf);
  }
}

uint8_t BLE_IsAdvertising(void) { return g_advertising; }

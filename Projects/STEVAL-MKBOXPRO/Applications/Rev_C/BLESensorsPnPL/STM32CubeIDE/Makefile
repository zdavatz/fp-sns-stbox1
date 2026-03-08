# Makefile for BLESensorsPnPL - STEVAL-MKBOXPRO Rev_C
# Generated for arm-none-eabi-gcc command-line build

TOOLCHAIN = /Users/zdavatz/arm-gnu-toolchain/bin
PREFIX = $(TOOLCHAIN)/arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size

ROOT = ../../../../../..
APP = ..

CPU = -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

DEFS = -DUSE_HAL_DRIVER -DSTM32U585xx

INCLUDES = \
  -I$(APP)/Inc \
  -I$(ROOT)/Drivers/STM32U5xx_HAL_Driver/Inc \
  -I$(ROOT)/Drivers/STM32U5xx_HAL_Driver/Inc/Legacy \
  -I$(ROOT)/Drivers/CMSIS/Device/ST/STM32U5xx/Include \
  -I$(ROOT)/Drivers/CMSIS/Include \
  -I$(ROOT)/Middlewares/ST/STM32_BLE_Manager/Inc \
  -I$(ROOT)/Middlewares/Third_Party/parson \
  -I$(ROOT)/Middlewares/ST/STM32WB07_06/utils \
  -I$(ROOT)/Middlewares/ST/STM32WB07_06/includes \
  -I$(ROOT)/Middlewares/ST/STM32WB07_06/hci/hci_tl_patterns/Basic \
  -I$(ROOT)/Drivers/BSP/SensorTileBoxPro \
  -I$(ROOT)/Drivers/BSP/Components/Common \
  -I$(ROOT)/Drivers/BSP/Components/stc3115 \
  -I$(ROOT)/Middlewares/ST/PnPLCompManager/Inc \
  -I$(ROOT)/Drivers/BSP/Components/st25dvxxkc \
  -I$(ROOT)/Drivers/BSP/Components/lis2mdl \
  -I$(ROOT)/Drivers/BSP/Components/lps22df \
  -I$(ROOT)/Drivers/BSP/Components/lis2du12 \
  -I$(ROOT)/Drivers/BSP/Components/stts22h \
  -I$(ROOT)/Drivers/BSP/Components/lsm6dsv16x \
  -I$(ROOT)/Drivers/BSP/Components/ST25DV

CFLAGS = $(CPU) $(DEFS) $(INCLUDES) -Og -Wall -Wno-format -fdata-sections -ffunction-sections -g3

LDSCRIPT = STM32U585AIIXQ_FLASH.ld
LDFLAGS = $(CPU) -T$(LDSCRIPT) -Wl,--gc-sections -lm -lc --specs=nosys.specs --specs=nano.specs -u _printf_float -Wl,-Map=build/BLESensorsPnPL.map

BUILD_DIR = build
TARGET = BLESensorsPnPL

# Source files
C_SOURCES = \
  $(APP)/Src/app_blesensorspnpl.c \
  $(APP)/Src/app_model.c \
  $(APP)/Src/ble_function.c \
  $(APP)/Src/ble_implementation.c \
  $(APP)/Src/configuration_pn_p_l.c \
  $(APP)/Src/control_pn_p_l.c \
  $(APP)/Src/device_information_pn_p_l.c \
  $(APP)/Src/environmental_pn_p_l.c \
  $(APP)/Src/hci_tl_interface.c \
  $(APP)/Src/i_control.c \
  $(APP)/Src/inertial_pn_p_l.c \
  $(APP)/Src/main.c \
  $(APP)/Src/ota.c \
  $(APP)/Src/steval_mkboxpro.c \
  $(APP)/Src/steval_mkboxpro_bus.c \
  $(APP)/Src/stm32u5xx_hal_msp.c \
  $(APP)/Src/stm32u5xx_it.c \
  $(APP)/Src/system_stm32u5xx.c \
  Application/User/syscalls.c \
  Application/User/sysmem.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_cortex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_crc.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_crc_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dma.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dma_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_exti.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_flash.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_flash_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gpio.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gtzc.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_i2c.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_i2c_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_icache.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_mdf.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pcd.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pcd_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rng.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rng_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rtc.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rtc_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_sai.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_sai_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_spi.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_spi_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_uart.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_uart_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_dlyb.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_sdmmc.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_usb.c \
  $(ROOT)/Drivers/BSP/Components/stc3115/STC3115_Driver.c \
  $(ROOT)/Drivers/BSP/Components/lis2du12/lis2du12.c \
  $(ROOT)/Drivers/BSP/Components/lis2du12/lis2du12_reg.c \
  $(ROOT)/Drivers/BSP/Components/lis2mdl/lis2mdl.c \
  $(ROOT)/Drivers/BSP/Components/lis2mdl/lis2mdl_reg.c \
  $(ROOT)/Drivers/BSP/Components/lps22df/lps22df.c \
  $(ROOT)/Drivers/BSP/Components/lps22df/lps22df_reg.c \
  $(ROOT)/Drivers/BSP/Components/lsm6dsv16x/lsm6dsv16x.c \
  $(ROOT)/Drivers/BSP/Components/lsm6dsv16x/lsm6dsv16x_reg.c \
  $(ROOT)/Drivers/BSP/Components/stts22h/stts22h.c \
  $(ROOT)/Drivers/BSP/Components/stts22h/stts22h_reg.c \
  $(ROOT)/Drivers/BSP/Components/st25dvxxkc/st25dvxxkc.c \
  $(ROOT)/Drivers/BSP/Components/st25dvxxkc/st25dvxxkc_reg.c \
  $(ROOT)/Drivers/BSP/Components/ST25DV/st25dv.c \
  $(ROOT)/Drivers/BSP/Components/ST25DV/st25dv_reg.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_audio.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_env_sensors.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_env_sensors_ex.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_gg.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_motion_sensors.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_motion_sensors_ex.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_nfctag.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_nfctag_ex.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_sd.c \
  $(ROOT)/Middlewares/ST/PnPLCompManager/Src/IPnPLComponent.c \
  $(ROOT)/Middlewares/ST/PnPLCompManager/Src/PnPLCompManager.c \
  $(ROOT)/Middlewares/ST/STM32_BLE_Manager/Src/ble_manager.c \
  $(ROOT)/Middlewares/ST/STM32_BLE_Manager/Src/ble_battery.c \
  $(ROOT)/Middlewares/ST/STM32_BLE_Manager/Src/ble_environmental.c \
  $(ROOT)/Middlewares/ST/STM32_BLE_Manager/Src/ble_inertial.c \
  $(ROOT)/Middlewares/ST/STM32_BLE_Manager/Src/ble_pn_p_like.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/controller/stm32wb07_06_gap_aci.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/controller/stm32wb07_06_gatt_aci.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/controller/stm32wb07_06_hal_aci.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/controller/stm32wb07_06_l2cap_aci.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/hci_tl_patterns/Basic/hci_tl.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/stm32wb07_06_dev_config.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/stm32wb07_06_events.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/stm32wb07_06_events_cb.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/stm32wb07_06_hci_le.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/utils/ble_list.c \
  $(ROOT)/Middlewares/Third_Party/parson/parson.c

ASM_SOURCES = Application/Startup/startup_stm32u585aiixq.s

# Object files
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) -c $(CPU) -g3 $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O binary $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

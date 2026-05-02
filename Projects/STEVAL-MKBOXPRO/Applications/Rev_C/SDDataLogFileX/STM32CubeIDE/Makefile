# Makefile for SDDataLogFileX - STEVAL-MKBOXPRO Rev_C
# Generated for arm-none-eabi-gcc command-line build

ROOT = ../../../../../..
-include $(ROOT)/config.mk
TOOLCHAIN ?= $(HOME)/.software/arm-gnu-toolchain/bin
PREFIX = $(TOOLCHAIN)/arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
APP = ..

CPU = -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

# GPS measurement rate in Hz. Override at build time:
#   make GPS_RATE_HZ=5
# Valid 1..25 for single-GNSS on u-blox MAX-M10S. Above ~25 Hz the 38400-baud
# UART saturates even with only GGA+RMC enabled.
GPS_RATE_HZ ?= 10

DEFS = \
  -DUSE_HAL_DRIVER \
  -DSTM32U585xx \
  -DFX_INCLUDE_USER_DEFINE_FILE \
  -DTX_INCLUDE_USER_DEFINE_FILE \
  -DTX_SINGLE_MODE_NON_SECURE=1 \
  -DGPS_RATE_HZ=$(GPS_RATE_HZ)

ASM_DEFS = -DTX_SINGLE_MODE_NON_SECURE=1

INCLUDES = \
  -I$(APP)/FileX/App \
  -I$(APP)/FileX/Target \
  -I$(APP)/Core/inc \
  -I$(APP)/AZURE_RTOS/App \
  -I$(ROOT)/Drivers/STM32U5xx_HAL_Driver/Inc \
  -I$(ROOT)/Drivers/STM32U5xx_HAL_Driver/Inc/Legacy \
  -I$(ROOT)/Drivers/CMSIS/Device/ST/STM32U5xx/Include \
  -I$(ROOT)/Drivers/CMSIS/Include \
  -I$(ROOT)/Middlewares/ST/threadx/common/inc \
  -I$(ROOT)/Middlewares/ST/filex/common/inc \
  -I$(ROOT)/Middlewares/ST/filex/ports/generic/inc \
  -I$(ROOT)/Middlewares/ST/threadx/ports/cortex_m33/gnu/inc \
  -I$(ROOT)/Drivers/BSP/SensorTileBoxPro \
  -I$(ROOT)/Drivers/BSP/Components/Common \
  -I$(ROOT)/Drivers/BSP/Components/lsm6dsv16x \
  -I$(ROOT)/Drivers/BSP/Components/lis2mdl \
  -I$(ROOT)/Drivers/BSP/Components/lps22df \
  -I$(ROOT)/Drivers/BSP/Components/stts22h \
  -I$(ROOT)/Drivers/BSP/Components/stc3115 \
  -I$(ROOT)/Middlewares/ST/STM32_BLE_Manager/Inc \
  -I$(ROOT)/Middlewares/ST/STM32WB07_06/utils \
  -I$(ROOT)/Middlewares/ST/STM32WB07_06/includes \
  -I$(ROOT)/Middlewares/ST/STM32WB07_06/hci/hci_tl_patterns/Basic \
  -I$(ROOT)/Middlewares/Third_Party/parson

CFLAGS = $(CPU) $(DEFS) $(INCLUDES) -O2 -Wall -Wno-format -fdata-sections -ffunction-sections -g3

LDSCRIPT = STM32U585AIIXQ_FLASH.ld
LDFLAGS = $(CPU) -T$(LDSCRIPT) -Wl,--gc-sections -lm -lc --specs=nosys.specs --specs=nano.specs -u _printf_float -Wl,-Map=build/SDDataLogFileX.map

BUILD_DIR = build
TARGET = SDDataLogFileX

# Application source files
C_SOURCES = \
  $(APP)/Core/Src/main.c \
  $(APP)/Core/Src/app_threadx.c \
  $(APP)/Core/Src/ble_spi.c \
  $(APP)/Core/Src/ble_sync.c \
  $(APP)/Core/Src/ble_implementation.c \
  $(APP)/Core/Src/ble_function.c \
  $(APP)/Core/Src/ble_filesync.c \
  $(APP)/Core/Src/hci_tl_interface.c \
  $(APP)/Core/Src/gps_nmea.c \
  $(ROOT)/Middlewares/Third_Party/parson/parson.c \
  $(ROOT)/Middlewares/ST/STM32_BLE_Manager/Src/ble_manager.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/utils/ble_list.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/hci_parser.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/hci_tl_patterns/Basic/hci_tl.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/stm32wb07_06_dev_config.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/stm32wb07_06_events.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/stm32wb07_06_events_cb.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/stm32wb07_06_hci_le.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/controller/stm32wb07_06_gap_aci.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/controller/stm32wb07_06_gatt_aci.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/controller/stm32wb07_06_hal_aci.c \
  $(ROOT)/Middlewares/ST/STM32WB07_06/hci/controller/stm32wb07_06_l2cap_aci.c \
  $(APP)/Core/Src/stm32u5xx_hal_msp.c \
  $(APP)/Core/Src/stm32u5xx_hal_timebase_tim.c \
  $(APP)/Core/Src/stm32u5xx_it.c \
  $(APP)/Core/Src/system_stm32u5xx.c \
  $(APP)/AZURE_RTOS/App/app_azure_rtos.c \
  $(APP)/FileX/App/app_filex.c \
  $(APP)/FileX/Target/fx_stm32_sd_driver_glue.c \
  $(APP)/Patch/stm32u5xx_hal_sd_patch.c \
  Application/User/syscalls.c \
  Application/User/sysmem.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_adc.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_adc_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_cortex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dcache.c \
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
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_ospi.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_sd_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_spi.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_spi_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_uart.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_uart_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_dlyb.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_ll_sdmmc.c \
  $(ROOT)/Drivers/BSP/Components/lis2mdl/lis2mdl.c \
  $(ROOT)/Drivers/BSP/Components/lis2mdl/lis2mdl_reg.c \
  $(ROOT)/Drivers/BSP/Components/lps22df/lps22df.c \
  $(ROOT)/Drivers/BSP/Components/lps22df/lps22df_reg.c \
  $(ROOT)/Drivers/BSP/Components/lsm6dsv16x/lsm6dsv16x.c \
  $(ROOT)/Drivers/BSP/Components/lsm6dsv16x/lsm6dsv16x_reg.c \
  $(ROOT)/Drivers/BSP/Components/stts22h/stts22h.c \
  $(ROOT)/Drivers/BSP/Components/stts22h/stts22h_reg.c \
  $(ROOT)/Drivers/BSP/Components/stc3115/STC3115_Driver.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_audio.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_bus.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_env_sensors.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_gg.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_motion_sensors.c \
  $(ROOT)/Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_sd.c \
  $(ROOT)/Middlewares/ST/filex/common/drivers/fx_stm32_sd_driver.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_attributes_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_attributes_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_create.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_default_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_default_get_copy.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_default_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_delete.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_entry_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_entry_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_exFAT_entry_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_exFAT_entry_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_exFAT_free_search.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_exFAT_unicode_entry_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_first_entry_find.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_first_full_entry_find.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_free_search.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_information_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_local_path_clear.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_local_path_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_local_path_get_copy.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_local_path_restore.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_local_path_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_long_name_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_long_name_get_extended.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_name_extract.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_name_test.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_next_entry_find.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_next_full_entry_find.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_rename.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_search.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_short_name_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_directory_short_name_get_extended.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_add_FAT_log.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_add_bitmap_log.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_add_checksum_log.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_add_dir_log.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_apply_logs.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_calculate_checksum.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_cleanup_FAT_chain.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_create_log_file.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_enable.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_read_FAT.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_read_directory_sector.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_read_log_file.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_recover.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_reset_log_file.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_set_FAT_chain.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_transaction_end.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_transaction_fail.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_transaction_start.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_fault_tolerant_write_log_file.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_allocate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_attributes_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_attributes_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_best_effort_allocate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_close.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_create.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_date_time_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_delete.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_extended_allocate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_extended_best_effort_allocate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_extended_relative_seek.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_extended_seek.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_extended_truncate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_extended_truncate_release.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_open.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_relative_seek.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_rename.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_seek.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_truncate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_truncate_release.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_file_write_notify_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_abort.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_boot_info_extract.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_cache_invalidate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_check.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_check_FAT_chain_check.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_check_lost_cluster_check.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_close.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_close_notify_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_exFAT_format.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_extended_space_available.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_flush.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_format.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_format_oem_name_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_format_type_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_format_volume_id_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_open.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_open_notify_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_space_available.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_volume_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_volume_get_extended.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_volume_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_media_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_partition_offset_calculate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_ram_driver.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_system_date_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_system_date_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_system_initialize.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_system_time_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_system_time_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_system_timer_entry.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_trace_event_insert.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_trace_event_update.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_trace_object_register.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_trace_object_unregister.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_directory_create.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_directory_entry_change.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_directory_entry_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_directory_rename.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_directory_search.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_file_create.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_file_rename.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_length_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_length_get_extended.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_name_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_name_get_extended.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_short_name_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_unicode_short_name_get_extended.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_16_unsigned_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_16_unsigned_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_32_unsigned_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_32_unsigned_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_64_unsigned_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_64_unsigned_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_FAT_entry_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_FAT_entry_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_FAT_flush.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_FAT_map_flush.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_FAT_sector_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_absolute_path_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_allocate_new_cluster.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_bitmap_cache_prepare.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_bitmap_cache_update.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_bitmap_flush.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_bitmap_free_cluster_find.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_bitmap_initialize.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_bitmap_start_sector_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_cluster_free.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_cluster_state_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_cluster_state_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_geometry_check.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_name_hash_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_size_calculate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_system_area_checksum_verify.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_system_area_checksum_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_system_area_format.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_system_sector_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_unicode_name_hash_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_exFAT_upcase_table.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_logical_sector_cache_entry_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_logical_sector_flush.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_logical_sector_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_logical_sector_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_memory_copy.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_memory_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_string_length_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fx_utility_token_length_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_attributes_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_attributes_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_create.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_default_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_default_get_copy.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_default_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_delete.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_first_entry_find.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_first_full_entry_find.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_information_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_local_path_clear.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_local_path_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_local_path_get_copy.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_local_path_restore.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_local_path_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_long_name_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_long_name_get_extended.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_name_test.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_next_entry_find.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_next_full_entry_find.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_rename.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_short_name_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_directory_short_name_get_extended.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_fault_tolerant_enable.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_allocate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_attributes_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_attributes_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_best_effort_allocate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_close.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_create.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_date_time_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_delete.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_extended_allocate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_extended_best_effort_allocate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_extended_relative_seek.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_extended_seek.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_extended_truncate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_extended_truncate_release.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_open.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_relative_seek.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_rename.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_seek.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_truncate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_truncate_release.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_file_write_notify_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_abort.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_cache_invalidate.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_check.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_close.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_close_notify_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_exFAT_format.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_extended_space_available.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_flush.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_format.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_open.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_open_notify_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_read.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_space_available.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_volume_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_volume_get_extended.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_volume_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_media_write.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_system_date_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_system_date_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_system_time_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_system_time_set.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_unicode_directory_create.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_unicode_directory_rename.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_unicode_file_create.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_unicode_file_rename.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_unicode_name_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_unicode_name_get_extended.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_unicode_short_name_get.c \
  $(ROOT)/Middlewares/ST/filex/common/src/fxe_unicode_short_name_get_extended.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_block_allocate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_block_pool_cleanup.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_block_pool_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_block_pool_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_block_pool_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_block_pool_initialize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_block_pool_performance_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_block_pool_performance_system_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_block_pool_prioritize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_block_release.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_allocate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_pool_cleanup.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_pool_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_pool_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_pool_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_pool_initialize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_pool_performance_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_pool_performance_system_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_pool_prioritize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_pool_search.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_byte_release.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_event_flags_cleanup.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_event_flags_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_event_flags_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_event_flags_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_event_flags_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_event_flags_initialize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_event_flags_performance_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_event_flags_performance_system_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_event_flags_set.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_event_flags_set_notify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_initialize_high_level.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_initialize_kernel_enter.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_initialize_kernel_setup.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_misra.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_cleanup.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_initialize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_performance_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_performance_system_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_prioritize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_priority_change.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_mutex_put.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_cleanup.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_flush.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_front_send.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_initialize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_performance_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_performance_system_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_prioritize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_receive.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_send.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_queue_send_notify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_ceiling_put.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_cleanup.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_initialize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_performance_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_performance_system_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_prioritize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_put.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_semaphore_put_notify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_entry_exit_notify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_identify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_initialize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_performance_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_performance_system_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_preemption_change.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_priority_change.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_relinquish.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_reset.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_resume.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_shell_entry.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_sleep.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_stack_analyze.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_stack_error_handler.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_stack_error_notify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_suspend.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_system_preempt_check.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_system_resume.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_system_suspend.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_terminate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_time_slice.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_time_slice_change.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_timeout.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_thread_wait_abort.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_time_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_time_set.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_activate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_change.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_deactivate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_expiration_process.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_initialize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_performance_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_performance_system_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_system_activate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_system_deactivate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_timer_thread_entry.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_buffer_full_notify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_disable.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_enable.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_event_filter.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_event_unfilter.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_initialize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_interrupt_control.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_isr_enter_insert.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_isr_exit_insert.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_object_register.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_object_unregister.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/tx_trace_user_event_insert.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_block_allocate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_block_pool_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_block_pool_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_block_pool_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_block_pool_prioritize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_block_release.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_byte_allocate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_byte_pool_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_byte_pool_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_byte_pool_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_byte_pool_prioritize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_byte_release.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_event_flags_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_event_flags_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_event_flags_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_event_flags_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_event_flags_set.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_event_flags_set_notify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_mutex_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_mutex_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_mutex_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_mutex_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_mutex_prioritize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_mutex_put.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_queue_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_queue_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_queue_flush.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_queue_front_send.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_queue_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_queue_prioritize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_queue_receive.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_queue_send.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_queue_send_notify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_semaphore_ceiling_put.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_semaphore_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_semaphore_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_semaphore_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_semaphore_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_semaphore_prioritize.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_semaphore_put.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_semaphore_put_notify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_entry_exit_notify.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_info_get.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_preemption_change.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_priority_change.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_relinquish.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_reset.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_resume.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_suspend.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_terminate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_time_slice_change.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_thread_wait_abort.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_timer_activate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_timer_change.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_timer_create.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_timer_deactivate.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_timer_delete.c \
  $(ROOT)/Middlewares/ST/threadx/common/src/txe_timer_info_get.c

# Assembly sources (.s lowercase = GNU as, .S uppercase = GNU as with cpp)
ASM_SOURCES = \
  Application/Startup/startup_stm32u585aiix.s \
  $(APP)/Core/Src/tx_initialize_low_level.s

ASM_SOURCES_S = \
  $(ROOT)/Middlewares/ST/threadx/ports/cortex_m33/gnu/src/tx_thread_context_restore.S \
  $(ROOT)/Middlewares/ST/threadx/ports/cortex_m33/gnu/src/tx_thread_context_save.S \
  $(ROOT)/Middlewares/ST/threadx/ports/cortex_m33/gnu/src/tx_thread_interrupt_control.S \
  $(ROOT)/Middlewares/ST/threadx/ports/cortex_m33/gnu/src/tx_thread_interrupt_disable.S \
  $(ROOT)/Middlewares/ST/threadx/ports/cortex_m33/gnu/src/tx_thread_interrupt_restore.S \
  $(ROOT)/Middlewares/ST/threadx/ports/cortex_m33/gnu/src/tx_thread_schedule.S \
  $(ROOT)/Middlewares/ST/threadx/ports/cortex_m33/gnu/src/tx_thread_stack_build.S \
  $(ROOT)/Middlewares/ST/threadx/ports/cortex_m33/gnu/src/tx_thread_system_return.S \
  $(ROOT)/Middlewares/ST/threadx/ports/cortex_m33/gnu/src/tx_timer_interrupt.S

# Object files
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES_S:.S=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))
vpath %.S $(sort $(dir $(ASM_SOURCES_S)))

all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/firmware.bin

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) -c $(CPU) $(ASM_DEFS) -g3 $< -o $@

$(BUILD_DIR)/%.o: %.S | $(BUILD_DIR)
	$(AS) -c $(CPU) $(ASM_DEFS) -g3 $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O binary $< $@

# Copy also as firmware.bin so it can be dropped onto the SD card
# as-is — the SD-flash code in app_filex.c looks for exactly that name.
# Saves the field-tester from renaming every build manually.
$(BUILD_DIR)/firmware.bin: $(BUILD_DIR)/$(TARGET).bin
	cp $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

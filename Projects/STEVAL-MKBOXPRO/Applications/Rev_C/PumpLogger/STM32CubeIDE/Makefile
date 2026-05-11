# Makefile for PumpLogger — STEVAL-MKBOXPRO Rev_C
# Bare-metal CLI build with arm-none-eabi-gcc. Phase 2: produces a binary
# that boots, beeps, and blinks the green LED.

ROOT       = ../../../../../..
-include $(ROOT)/config.mk
TOOLCHAIN ?= $(HOME)/.software/arm-gnu-toolchain/bin
PREFIX     = $(TOOLCHAIN)/arm-none-eabi-
CC         = $(PREFIX)gcc
AS         = $(PREFIX)gcc -x assembler-with-cpp
CP         = $(PREFIX)objcopy
SZ         = $(PREFIX)size
APP        = ..

CPU = -mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard

DEFS = \
  -DUSE_HAL_DRIVER \
  -DSTM32U585xx

INCLUDES = \
  -I$(APP)/Inc \
  -I$(ROOT)/Drivers/STM32U5xx_HAL_Driver/Inc \
  -I$(ROOT)/Drivers/STM32U5xx_HAL_Driver/Inc/Legacy \
  -I$(ROOT)/Drivers/CMSIS/Device/ST/STM32U5xx/Include \
  -I$(ROOT)/Drivers/CMSIS/Include

CFLAGS = $(CPU) $(DEFS) $(INCLUDES) -O2 -Wall -Wno-format \
         -fdata-sections -ffunction-sections -g3

LDSCRIPT = STM32U585AIIXQ_FLASH.ld
LDFLAGS  = $(CPU) -T$(LDSCRIPT) -Wl,--gc-sections \
           -lm -lc --specs=nosys.specs --specs=nano.specs \
           -Wl,-Map=build/PumpLogger.map

BUILD_DIR = build
TARGET    = PumpLogger

# Application sources
C_SOURCES = \
  $(APP)/Src/main.c \
  $(APP)/Src/sched.c \
  $(APP)/Src/buzzer.c \
  $(APP)/Src/stm32u5xx_it.c \
  $(APP)/Src/system_stm32u5xx.c \
  Application/User/syscalls.c \
  Application/User/sysmem.c

# STM32 HAL — only what we link against in Phase 2. Adding TIM (buzzer) +
# the basics. Other HAL modules will be added as later phases need them.
C_SOURCES += \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_cortex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dma.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_dma_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_exti.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_flash.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_flash_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gpio.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_gtzc.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_pwr_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_rcc_ex.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim.c \
  $(ROOT)/Drivers/STM32U5xx_HAL_Driver/Src/stm32u5xx_hal_tim_ex.c

ASM_SOURCES = Application/Startup/startup_stm32u585aiixq.s

C_OBJS   = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
ASM_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))

vpath %.c $(sort $(dir $(C_SOURCES)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/firmware.bin

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(AS) -c $(CPU) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(C_OBJS) $(ASM_OBJS) | $(BUILD_DIR)
	$(CC) $^ $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(CP) -O binary $< $@

$(BUILD_DIR)/firmware.bin: $(BUILD_DIR)/$(TARGET).bin
	cp $< $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

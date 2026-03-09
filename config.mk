# ARM GNU Toolchain path (bin directory containing arm-none-eabi-gcc)
# Auto-detect platform: macOS vs Linux
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  # macOS: full ARM GNU Toolchain installed via .pkg
  TOOLCHAIN = $(HOME)/.software/arm-gnu-toolchain/bin
else
  # Linux: typically installed system-wide via apt/dnf
  TOOLCHAIN = /usr/bin
endif

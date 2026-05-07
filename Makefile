# This was pulled from Gemini
obj-m := dynaswap.o
dynaswap-y := dynaswap_drv.o dynamap.o

# 1. Detection Logic
# Check if the current kernel was built with Clang
IS_CLANG_KERNEL := $(shell grep -q "CONFIG_CC_IS_CLANG=y" /lib/modules/$(shell uname -r)/build/.config && echo "yes" || echo "no")

# 2. Set Flags Based on Environment
ifeq ($(IS_CLANG_KERNEL),yes)
    # Desktop Settings (Clang)
    COMPILER_FLAGS := LLVM=1 CC=clang KCFLAGS="-Qunused-arguments -Wno-unknown-warning-option"
else
    # Raspberry Pi Settings (GCC)
    # We leave it empty to use the system default (GCC)
    COMPILER_FLAGS :=
endif

JOBS := $(shell nproc)

all:
	$(MAKE) -j$(JOBS) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) $(COMPILER_FLAGS) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean

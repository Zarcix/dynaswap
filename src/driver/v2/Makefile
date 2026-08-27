# export LLVM = 1

obj-m := dynaswap.o
dynaswap-y := main.o device.o context.o fs/storage.o fs/slot.o

ccflags-y := -I$(src)/

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) CONFIG_DEBUG_INFO_BTF_MODULES= modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
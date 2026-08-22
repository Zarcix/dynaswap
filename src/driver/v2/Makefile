export LLVM = 1

obj-m := dynaswap.o
dynaswap-y := main.o block/mapper.o block/device.o	

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) CONFIG_DEBUG_INFO_BTF_MODULES= modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
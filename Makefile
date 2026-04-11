obj-m += dynaswap_drv.o

LLVM_FLAGS := LLVM=1 CC=clang

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) $(LLVM_FLAGS) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
obj-m := dynaswap.o
dynaswap-y := dynaswap_drv.o dynamap.o

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
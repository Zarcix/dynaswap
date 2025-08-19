ARCH ?= $(shell uname -m)
BUILD_DIR := build
OUTPUT := $(BUILD_DIR)/dynaswap_$(ARCH)
SRC := $(wildcard src/*.c)
CFLAGS := -Wall -Wextra
LIBS := -luuid -lproc2

all: dynaswap

init:
	@echo "Preparing for arch: $(ARCH)"
	mkdir -p $(BUILD_DIR)

libs:
	@pushd public/libconfig >> /dev/null && \
	git clean -xdf . >> /dev/null && \
	git reset --hard && \
	autoreconf >> /dev/null 2>&1 && \
	./configure --enable-static --disable-shared >> /dev/null 2>&1 && \
	make -s >> /dev/null 2>&1 && \
	popd

	@cp public/libconfig/lib/.libs/libconfig.a build/

dynaswap: init libs $(SRC)
	gcc \
		$(CFLAGS) \
		$(SRC) \
		$(wildcard build/*.a) \
		$(LIBS) \
		-o $(OUTPUT)

clean:
	rm -rf build/*

.PHONY: all init libs clean
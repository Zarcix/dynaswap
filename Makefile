ARCH ?= $(shell uname -m)
BUILD_DIR := build/$(ARCH)
OUTPUT := $(BUILD_DIR)/dynaswap
SRC := $(wildcard src/*.c)
CFLAGS := -Wall -Wextra
LIBS := -luuid -lproc2 -lconfig

all: $(OUTPUT)

$(OUTPUT): $(SRC)
	@echo "Building for architecture: $(ARCH)"
	mkdir -p $(BUILD_DIR)
	gcc $(CFLAGS) $(SRC) $(LIBS) -o $(OUTPUT)

clean:
	rm -rf build/*

.PHONY: all clean
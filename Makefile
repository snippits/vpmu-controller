-include path.mak
CC=gcc
ARM_CC=arm-linux-gnueabi-gcc
ARM_LD=arm-linux-gnueabi-ld
CFLAGS=-g -Wall -O1
LFLAGS=

SRCS=vpmu-control.c vpmu-control-lib.c efd.c
HEADERS=vpmu-control-lib.h
TARGETS=vpmu-control-arm vpmu-control-x86 vpmu-control-dry-run
ifneq ($(KERNELDIR_ARM),)
TARGETS +=device_driver/vpmu-device-arm.ko
DRIVER_SRC=$(wildcard device_driver/*.c)
DRIVER_HEADER=$(wildcard device_driver/*.h)
endif
ifneq ($(KERNELDIR_X86),)
TARGETS +=device_driver/vpmu-device-x86.ko
DRIVER_SRC=$(wildcard device_driver/*.c)
DRIVER_HEADER=$(wildcard device_driver/*.h)
endif

.PHONY: all clean

all:	$(TARGETS)

vpmu-control-x86:	$(SRCS) $(HEADERS)
	@echo "  CC      $@"
	@$(CC) $(SRCS) -o $@ $(CFLAGS) $(LFLAGS)

vpmu-control-dry-run:	$(SRCS) $(HEADERS)
	@echo "  CC      $@"
	@$(CC) $(SRCS) -o $@ $(CFLAGS) $(LFLAGS) -DDRY_RUN

vpmu-control-arm:	$(SRCS) $(HEADERS)
	@echo "  ARM_CC  $@"
	@$(ARM_CC) $(SRCS) -o $@ $(CFLAGS) $(LFLAGS)

device_driver/vpmu-device-arm.ko:	$(DRIVER_SRC) $(DRIVER_HEADER)
	@rm -f ./vpmu-device-arm.ko
	@echo "  BUILD   $@"
	@$(MAKE) BUILDSYSTEM_DIR=$(KERNELDIR_ARM) CC=$(ARM_CC) LD=$(ARM_LD) ARCH=arm -C $(shell dirname $@)
	@cp $@ ./

device_driver/vpmu-device-x86.ko:	$(DRIVER_SRC) $(DRIVER_HEADER)
	@rm -f ./vpmu-device-x86.ko
	@echo "  BUILD   $@"
	@$(MAKE) BUILDSYSTEM_DIR=$(KERNELDIR_X86) CC=$(CC) ARCH=x86 -C $(shell dirname $@)
	@cp $@ ./

clean:
	rm $(TARGETS) *.ko
	$(MAKE) -C device_driver clean


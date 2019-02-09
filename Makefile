-include path.mak
CC=gcc
ARM_CC=arm-linux-gnueabihf-gcc
ARM_LD=arm-linux-gnueabihf-ld
CFLAGS=-g -Wall -Wno-unused-result -O1
LFLAGS=

SRCS=vpmu-control-lib.c vpmu-elf.c
HEADERS=vpmu-control-lib.h vpmu-path-lib.h vpmu-elf.h
VPMU_CONTROL_SRCS=vpmu-control.c $(SRCS)
VPMU_PERF_SRCS=vpmu-perf.c $(SRCS)
TARGETS=vpmu-control-arm vpmu-control-x86 vpmu-control-dry-run
TARGETS+=vpmu-perf-arm vpmu-perf-x86 vpmu-perf-dry-run
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

vpmu-perf-x86:	$(VPMU_PERF_SRCS) $(HEADERS)
	@echo "  CC      $@"
	@$(CC) $(VPMU_PERF_SRCS) -o $@ $(CFLAGS) $(LFLAGS)

vpmu-perf-dry-run:	$(VPMU_PERF_SRCS) $(HEADERS)
	@echo "  CC      $@"
	@$(CC) $(VPMU_PERF_SRCS) -o $@ $(CFLAGS) $(LFLAGS) -DDRY_RUN

vpmu-perf-arm:	$(VPMU_PERF_SRCS) $(HEADERS)
	@echo "  ARM_CC  $@"
	@$(ARM_CC) $(VPMU_PERF_SRCS) -o $@ $(CFLAGS) $(LFLAGS)

vpmu-control-x86:	$(VPMU_CONTROL_SRCS) $(HEADERS)
	@echo "  CC      $@"
	@$(CC) $(VPMU_CONTROL_SRCS) -o $@ $(CFLAGS) $(LFLAGS)

vpmu-control-dry-run:	$(VPMU_CONTROL_SRCS) $(HEADERS)
	@echo "  CC      $@"
	@$(CC) $(VPMU_CONTROL_SRCS) -o $@ $(CFLAGS) $(LFLAGS) -DDRY_RUN

vpmu-control-arm:	$(VPMU_CONTROL_SRCS) $(HEADERS)
	@echo "  ARM_CC  $@"
	@$(ARM_CC) $(VPMU_CONTROL_SRCS) -o $@ $(CFLAGS) $(LFLAGS)

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


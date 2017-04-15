# Running make with arch speficy (add postfix)
ifneq ($(ARCH),)
	EXTRA_ARGS=ARCH=$(ARCH)
	KO_POSTFIX=-$(ARCH)
endif

TARGET_MODULE:=vpmu-device$(KO_POSTFIX)

ifneq ($(DRY_RUN),)
ccflags-y := -DDRY_RUN
endif

# If we running by kernel building system
ifneq ($(KERNELRELEASE),)
	$(TARGET_MODULE)-objs := main.o device_file.o
	obj-m := $(TARGET_MODULE).o

# If we are running without kernel build system
else
	BUILDSYSTEM_DIR?=/lib/modules/$(shell uname -r)/build
	PWD:=$(shell pwd)

all:
# run kernel build system to make module
	@$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules

clean:
# run kernel build system to cleanup in current directory
	@$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) clean

load:
	insmod ./$(TARGET_MODULE).ko

unload:
	rmmod ./$(TARGET_MODULE).ko

host_test:	all
	sudo insmod ./$(TARGET_MODULE)-x86.ko
	sudo rmmod ./$(TARGET_MODULE)-x86.ko
	dmesg

install:
# run kernel build system to install module
	@$(MAKE) -C $(BUILDSYSTEM_DIR) M=$(PWD) modules_install

endif


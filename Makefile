CC=gcc
ARM_CC=arm-none-linux-gnueabi-gcc
CFLAGS=-g -Wall -O1
LFLAGS=

SRCS=vpmu-control.c vpmu-control-lib.c efd.c
DEPS=$(patsubst %.c,%.d,$(SRCS))
TARGETS=vpmu-control-arm vpmu-control-x86

.PHONY: all clean

all:	$(TARGETS)

vpmu-control-x86:	$(SRCS)
	@echo "  CC      $@"
	@$(CC) $^ -o $@ $(CFLAGS) $(LFLAGS)

vpmu-control-arm:	$(SRCS)
	@echo "  ARM_CC  $@"
	@$(ARM_CC) $^ -o $@ $(CFLAGS) $(LFLAGS)

clean:
	rm $(TARGETS) $(DEPS)

%.d:	%.c
	@$(CC) -MM $^ > $@

# Include automatically generated dependency files
-include $(DEPS)

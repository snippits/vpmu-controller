CC=gcc
ARM_CC=arm-linux-gnueabi-gcc
CFLAGS=-g -Wall -O1
LFLAGS=

SRCS=vpmu-control.c vpmu-control-lib.c efd.c
HEADERS=vpmu-control-lib.h
TARGETS=vpmu-control-arm vpmu-control-x86 vpmu-control-dry-run

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

clean:
	rm $(TARGETS)


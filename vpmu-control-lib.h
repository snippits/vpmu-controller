#ifndef __VPMU_CONTROL_LIB_H
#define __VPMU_CONTROL_LIB_H
#include <sys/types.h>
#include <stdint.h>

#define VPMU_PHYSICAL_ADDRESS 0xf1000000

#define VPMU_MMAP_ENABLE            0x0000
#define VPMU_MMAP_MEMTRACE          0x0004
#define VPMU_MMAP_BYPASS_ISR_ADDR   0x0008
#define VPMU_MMAP_POWER_ENABLE      0x0100
#define VPMU_MMAP_BYPASS_CPU_UTIL   0x0104
#define VPMU_MMAP_SELECT_NET_MODE   0x0108
#define VPMU_MMAP_BATTERY_ENABLE    0x010c
#define VPMU_MMAP_SET_PROC_NAME     0x0040
#define VPMU_MMAP_SET_PROC_SIZE     0x0044
#define VPMU_MMAP_SET_PROC_BIN      0x0048

typedef struct VPMU_HANDLER {
    int fd;
    uintptr_t *ptr; //TODO make address alignment to 64
    int flag_jit, flag_trace;
} VPMU_HANDLER;
typedef VPMU_HANDLER* vpmu_handler_t;

#define STR_IS(__A, __B) (strcmp(__A, __B) == 0)
#define STR_IS_2(__A, __B, __B2) ((strcmp(__A, __B) == 0) || (strcmp(__A, __B2) == 0))

vpmu_handler_t vpmu_open(off_t vpmu_address);
void vpmu_close(vpmu_handler_t handler);
void vpmu_close(vpmu_handler_t handler);
void vpmu_print_help_message(char *self);
vpmu_handler_t vpmu_parse_arguments(int argc, char **argv);
int vpmu_read_file(char *path, char **buffer);
void vpmu_fork_exec(char *cmd);

#endif

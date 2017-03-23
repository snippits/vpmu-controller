#ifndef __VPMU_CONTROL_LIB_H
#define __VPMU_CONTROL_LIB_H
#include <sys/types.h>
#include <stdint.h>
#include "vpmu-device.h"

typedef struct VPMU_HANDLER {
    int        fd;
    uintptr_t *ptr;
    int32_t    flag_model;
    uint32_t   flag_jit, flag_trace, flag_monitor;
} VPMU_HANDLER;
typedef VPMU_HANDLER *vpmu_handler_t;

#define ANY_VALUE 0

#define STR_IS(__A, __B) (strcmp(__A, __B) == 0)
#define STR_IS_2(__A, __B, __B2) ((strcmp(__A, __B) == 0) || (strcmp(__A, __B2) == 0))
#define HW_W(ADDR, VAL) handler->ptr[ADDR / sizeof(uintptr_t)] = (uintptr_t)VAL
#define HW_R(ADDR) (uintptr_t) handler->ptr[ADDR / sizeof(uintptr_t)]

vpmu_handler_t vpmu_open(const char *dev_path, off_t address_offset);
void vpmu_close(vpmu_handler_t handler);
void vpmu_close(vpmu_handler_t handler);
void vpmu_print_help_message(char *self);
vpmu_handler_t vpmu_parse_arguments(int argc, char **argv);
int vpmu_read_file(char *path, char **buffer);
void vpmu_fork_exec(char *cmd);

#endif

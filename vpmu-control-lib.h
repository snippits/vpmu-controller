#ifndef __VPMU_CONTROL_LIB_H_
#define __VPMU_CONTROL_LIB_H_
#include <sys/types.h> // off_t
#include <stdint.h>    // uint64_t
#include <string.h>    // strcmp, etc.
#include <inttypes.h>  // PRIu64
#include <stdbool.h>   // bool, true, false

#include "vpmu-device.h" // HW address mapping of VPMU

#ifdef DRY_RUN
#pragma message "DRY_RUN is defined. Compiled with dry run!!"
#define DRY_MSG(str, ...) fprintf(stderr, "\033[1;32m" str "\033[0;00m", ##__VA_ARGS__)
#else
#define DRY_MSG(str, ...) DBG_MSG("\033[1;32m" str "\033[0;00m", ##__VA_ARGS__)
#endif

#define ERR_MSG(str, ...)                                                                \
    fprintf(stderr, "[vpmu-control]  \033[1;31mFatal\033[0;00m: " str "\n", ##__VA_ARGS__)

#define LOG_MSG(str, ...) printf("[vpmu-control]  " str "\n", ##__VA_ARGS__)

#ifndef DBG_MSG
#define DBG_MSG(str, ...)                                                                \
    do {                                                                                 \
        if (getenv("DEBUG") != NULL) printf(str, ##__VA_ARGS__);                         \
    } while (0)
#endif

#define HW_W(ADDR, VAL) handler.ptr[ADDR / sizeof(uintptr_t)] = (uintptr_t)VAL
#define HW_R(ADDR) (uintptr_t) handler.ptr[ADDR / sizeof(uintptr_t)]

#define VPMU_DONT_CARE 0 ///< This is more descriptive when passing value to VPMU

typedef struct VPMUHandler {
    int        fd;
    uintptr_t *ptr;
    uint32_t   flag_model;
    bool       flag_jit, flag_trace, flag_monitor, flag_remove;
} VPMUHandler;

typedef struct VPMUBinary {
    bool  is_script;
    char *libraries[512];
    char *absolute_dir;
    char *relative_dir;
    char *path;
    char *script_path;
    char *file_name;
    char *argv[256];
    int   argc;
    char *cmd;
} VPMUBinary;

size_t load_binary(const char *file_path, char **out_buffer);
bool arg_is(const char *args, const char *str);
bool arg_is_2(const char *args, const char *str1, const char *str2);

VPMUHandler vpmu_open(const char *dev_path);
void vpmu_close(VPMUHandler handler);
uintptr_t vpmu_read_value(VPMUHandler handler, uintptr_t index);
void vpmu_write_value(VPMUHandler handler, uintptr_t index, uintptr_t value);
void vpmu_print_report(VPMUHandler handler);
void vpmu_start_fullsystem_tracing(VPMUHandler handler);
void vpmu_end_fullsystem_tracing(VPMUHandler handler);
void vpmu_reset_counters(VPMUHandler handler);

bool is_ascii_file(const char *path);
char *read_first_line(const char *path);
bool is_dynamic_binary(const char *file_path);
void vpmu_update_library_list(VPMUBinary *binary);
void vpmu_load_and_send(VPMUHandler handler,
                        const char *binary_path,
                        const char *script_path);
void vpmu_load_and_send_libs(VPMUHandler handler, VPMUBinary *binary);

VPMUBinary *parse_all_paths_args(const char *cmd);
void free_vpmu_binary(VPMUBinary *bin);

void vpmu_execute_binary(VPMUBinary *binary);
void vpmu_monitor_binary(VPMUHandler handler, VPMUBinary *binary);
void vpmu_stop_monitoring_binary(VPMUHandler handler, VPMUBinary *binary);
void vpmu_profile_binary(VPMUHandler handler, VPMUBinary *binary);
void vpmu_do_exec(VPMUHandler handler, const char *cmd_str);

#endif

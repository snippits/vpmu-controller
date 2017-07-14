#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   // access()
#include <ctype.h>    // isspace()
#include <sys/mman.h> // mmap(), MAP_SHARED
#include <sys/wait.h> // waitpid()
#include <fcntl.h>    // open(), close()
#include <libgen.h>   // basename(), dirname()

#include "efd.h"
#include "vpmu-control-lib.h"
#include "vpmu-path-lib.h" // Helpers functions to parse string like shell

size_t load_binary(const char *file_path, char **out_buffer)
{
    FILE * fp     = fopen(file_path, "rb");
    size_t lSize  = 0;
    char * buffer = NULL;

    if (fp == NULL) {
        ERR_MSG("File '%s' not found\n", file_path);
        return 0;
    }
    // obtain file size:
    fseek(fp, 0, SEEK_END);
    lSize = ftell(fp);
    rewind(fp);
    // allocate memory to contain the whole file
    buffer = (char *)malloc(sizeof(char) * lSize);
    if (buffer == NULL) {
        ERR_MSG("Memory error");
        exit(4);
    }
    // Read
    fread(buffer, 1, lSize, fp);
    // Close up
    fclose(fp);

    *out_buffer = buffer;
    return lSize;
}

bool arg_is(const char *args, const char *str)
{
    return (strcmp(args, str) == 0);
}

bool arg_is_2(const char *args, const char *str1, const char *str2)
{
    return (strcmp(args, str1) == 0) || (strcmp(args, str2) == 0);
}

void vpmu_print_help_message(const char *self)
{
#define HELP_MESG                                                                        \
    "Usage: %s [options] {actions...}\n"                                                 \
    "Options:\n"                                                                         \
    "  --mem         Use /dev/mem instead of /dev/vpmu-device-0 for communication\n"     \
    ""                                                                                   \
    "  --jit         Enable just-in-time model selection on performance simulation\n"    \
    "  --trace       Enable VPMU event tracing and function tracking ability\n"          \
    "                If \"--trace\" is set, the process will be traced automatically\n"  \
    "  --phase       Enable VPMU phase detection, --trace will be forced to set\n"       \
    "  --[MODEL]     [MODEL] could be one of the following\n"                            \
    "                    inst, cache, branch, pipeline, all_models\n"                    \
    "  --monitor     Enable VPMU event tracing and set the binary without\n"             \
    "                executing them when using -e action\n"                              \
    "  --remove      Remove binary (specified by -e option) from monitoring list\n"      \
    "  --help        Show this message\n"                                                \
    "\n\n"                                                                               \
    "Actions:\n"                                                                         \
    "  -r, --read  <address>          Read data from <address> of VPMU and print it "    \
    "out with \"\\n\"\n"                                                                 \
    "  -w, --write <address> <data>   Write <data> to <address> of VPMU\n"               \
    "  --start       Start VPMU profiling with all performance simulators ON\n"          \
    "                If \"--trace\" is set, \"--start\" do nothing\n"                    \
    "  --end         End/Stop VPMU profiling and report the results\n"                   \
    "                If \"--trace\" is set, \"--end\" do nothing\n"                      \
    "  --report      Simply report the current results. It can be used while profiling " \
    "\n"                                                                                 \
    "  -e, --exec    Run the program/executable.\n"                                      \
    "                If \"--trace\" is set, the controller will also pass some of the "  \
    "sections\n"                                                                         \
    "                (i.e. symbol table, dynamic libraries) of target binary to VPMU.\n" \
    "\n"                                                                                 \
    "Example:\n"                                                                         \
    "    ./vpmu-control-arm --all_models --start --exec \"ls -la\" --end\n"

    printf(HELP_MESG, self);
}

VPMUHandler vpmu_open(const char *dev_path)
{
    VPMUHandler handler = {}; // Zero initialized
    // Set the offset to VPMU_DEVICE_BASE_ADDR if it is mem
    off_t offset = startwith(dev_path, "/dev/mem") ? VPMU_DEVICE_BASE_ADDR : 0;

#ifdef DRY_RUN
    handler.ptr = (uintptr_t *)malloc(1024);
    (void)offset; // For unused warning
#else
    handler.fd = open(dev_path, O_RDWR | O_SYNC);
    if (handler.fd < 0) {
        ERR_MSG("Open '%s' failed", dev_path);
        exit(4);
    }
    handler.ptr = (uintptr_t *)mmap(NULL,
                                    VPMU_DEVICE_IOMEM_SIZE,
                                    PROT_READ | PROT_WRITE,
                                    MAP_SHARED,
                                    handler.fd,
                                    offset);
    if (handler.ptr == MAP_FAILED) {
        ERR_MSG("mmap '%s' failed", dev_path);
        exit(4);
    }
#endif

    return handler;
}

void vpmu_close(VPMUHandler handler)
{
#ifdef DRY_RUN
    free(handler.ptr);
#else
    munmap(handler.ptr, VPMU_DEVICE_IOMEM_SIZE);
    close(handler.fd);
#endif
}

uintptr_t vpmu_read_value(VPMUHandler handler, uintptr_t index)
{
    DRY_MSG("read 0x%" PRIxPTR "\n", index);
    return HW_R(index);
}

void vpmu_write_value(VPMUHandler handler, uintptr_t index, uintptr_t value)
{
    DRY_MSG("write 0x%" PRIxPTR " at address 0x%" PRIxPTR "\n", value, index);
    HW_W(index, value);
}

void vpmu_print_report(VPMUHandler handler)
{
    DRY_MSG("--report\n");
    HW_W(VPMU_MMAP_REPORT, VPMU_DONT_CARE);
}

void vpmu_start_fullsystem_tracing(VPMUHandler handler)
{
    DRY_MSG("--start\n");
    HW_W(VPMU_MMAP_ENABLE, handler.flag_model);
}

void vpmu_end_fullsystem_tracing(VPMUHandler handler)
{
    DRY_MSG("--end\n");
    HW_W(VPMU_MMAP_DISABLE, VPMU_DONT_CARE);
    HW_W(VPMU_MMAP_REPORT, VPMU_DONT_CARE);
}

void vpmu_reset_counters(VPMUHandler handler)
{
    HW_W(VPMU_MMAP_SET_TIMING_MODEL, handler.flag_model);
    HW_W(VPMU_MMAP_RESET, VPMU_DONT_CARE);
}

bool is_dynamic_binary(const char *file_path)
{
    if (file_path == NULL) return false;
    size_t file_size = 0;
    char * buffer    = NULL;
    int    i         = 0;

    file_size = load_binary(file_path, &buffer);
    if (file_size == 0) return false;

    // Check the magic word
    if (startwith(&buffer[1], "ELF")) {
        for (i = 0; i < file_size; i++) {
            if (startwith(&buffer[i], "GLIBC_")) {
                free(buffer);
                return true;
            }
        }
    }
    free(buffer);
    return false;
}

void vpmu_update_library_list(VPMUBinary *binary)
{
    if (!is_dynamic_binary(binary->path)) return;
    char new_command[1024] = "LD_TRACE_LOADED_OBJECTS=1 ";
    char message[1024]     = {}; // No longer than 1024 characters per line

    FILE *fp  = NULL;
    int   cnt = 0;

    if (binary->path == NULL) return;
    strcat(new_command, binary->path);
    fp = popen(new_command, "r");
    if (fp == NULL) return;
    fseek(fp, 0, SEEK_SET);

    DRY_MSG("Found shared libraries in this binary\n");
    while (fgets(message, sizeof(message), fp) != NULL) {
        binary->libraries[cnt] = get_library_path(message);
        if (binary->libraries[cnt] != NULL) {
            DRY_MSG("    %d) %s\n", cnt, binary->libraries[cnt]);
            cnt++;
        }
    }
    binary->libraries[cnt] = NULL; // Terminate the list
    fclose(fp);
}

void vpmu_load_and_send(VPMUHandler handler, const char *file_path)
{
    // The final path
    char path[1024] = {};
    // The buffer of file
    char *buffer = NULL;
    // The size of buffer
    size_t size = 0;

    if (file_path == NULL) return;
    if (access(file_path, F_OK) != -1) {
        // File exist, send it even it's not executable
        strncpy(path, file_path, sizeof(path));
        DBG_MSG("%-30s%s\n", "[vpmu_load_and_send]", "file_path exists");
    } else { // Find executables in the $PATH
        char *basec = strdup(file_path);
        char *bpath = locate_binary(basename(basec));
        strncpy(path, bpath, sizeof(path));
        free(basec);
        free(bpath);
        DBG_MSG("%-30s%s\n", "[vpmu_load_and_send]", "Find in $PATH");
    }

    size = load_binary(path, &buffer);
    if (size > 0) {
        HW_W(VPMU_MMAP_ADD_PROC_NAME, path);
        HW_W(VPMU_MMAP_SET_PROC_SIZE, size);
        HW_W(VPMU_MMAP_SET_PROC_BIN, buffer);

        DBG_MSG("%-30ssend '%s'\n", "[vpmu_load_and_send]", path);
        DRY_MSG("    send binary path      : %s\n", path);
        DRY_MSG("    send binary size      : %" PRIxPTR "\n", size);
        DRY_MSG("    send buffer pointer   : %p\n", buffer);
        DRY_MSG("\n");
    }

    if (buffer) free(buffer);
}

void vpmu_load_and_send_libs(VPMUHandler handler, VPMUBinary *binary)
{
    int j = 0;
    for (j = 0; binary->libraries[j] != NULL; j++) {
        char *path = binary->libraries[j];
        if (path[0] != '/' && path[0] != '.') {
            // Skip libraries that are still just a name (not found)
            DBG_MSG("%-30sskip '%s'\n", "[vpmu_load_and_send_libs]", path);
            continue;
        } else {
            vpmu_load_and_send(handler, path);
        }
    }
}

static char *form_abs_path(VPMUBinary *binary)
{
    char *path = (char *)malloc(4096);
    path[0]    = '\0';
    if (binary->absolute_dir && strlen(binary->absolute_dir) > 0) {
        strcpy(path, binary->absolute_dir);
        strcat(path, "/");
    }
    if (binary->file_name) strcat(path, binary->file_name);
    return path;
}

static char *form_rel_path(VPMUBinary *binary)
{
    char *path = (char *)malloc(4096);
    path[0]    = '\0';
    if (binary->relative_dir && strlen(binary->relative_dir) > 0) {
        strcpy(path, binary->relative_dir);
        strcat(path, "/");
    }
    if (binary->file_name) strcat(path, binary->file_name);
    return path;
}

// "cmd" is an input argument, others are output arguments
VPMUBinary *parse_all_paths_args(const char *cmd)
{
    int i = 0;
    // Return value
    VPMUBinary *binary = (VPMUBinary *)malloc(sizeof(VPMUBinary));

    // Reset all pointers
    memset(binary, 0, sizeof(VPMUBinary));

    binary->cmd = strdup(cmd);
    // Tokenize the command string into argv
    binary->argc = tokenize_to_argv(cmd, binary->argv);

    if (binary->argv[0]) {
        char *dirc  = strdup(binary->argv[0]);
        char *basec = strdup(binary->argv[0]);
        char *dname = dirname(dirc);
        char *bname = basename(basec);
        char *path  = NULL;

        binary->absolute_dir = startwith(cmd, "/") ? strdup(dname) : locate_binary(bname);
        binary->relative_dir = startwith(cmd, "./") ? strdup(dname) : strdup("");
        binary->file_name    = strdup(bname);

        if (strlen(binary->relative_dir) > 0) {
            // Use relative path as long as it is set to some value
            path = form_rel_path(binary);
            if (access(path, F_OK) != -1) {
                binary->path = path;
            }
        } else {
            // Use the path found from $PATH
            if (strlen(binary->absolute_dir) > 0) {
                path = form_abs_path(binary);
                if (access(path, F_OK) != -1) {
                    binary->path = path;
                }
            }
        }

        free(dirc);
        free(basec);
    }

    DRY_MSG("Command String   : '%s'\n", binary->cmd);
    DRY_MSG("Absolute Dir     : '%s'\n", binary->absolute_dir);
    DRY_MSG("Relative Dir     : '%s'\n", binary->relative_dir);
    DRY_MSG("Binary name      : '%s'\n", binary->file_name);
    DRY_MSG("# of arguments   : %d\n", binary->argc);
    for (i = 0; i < binary->argc; i++) {
        DRY_MSG("    ARG[%d]      : '%s'\n", i, binary->argv[i]);
    }
    if (binary->path == NULL) {
        DBG_MSG("%-30sFile path '%s' does not exist.\n",
                "[parse_all_paths_args]",
                binary->argv[0]);
    }

    return binary;
}

void free_vpmu_binary(VPMUBinary *bin)
{
    int i = 0;

    for (i = 0; i < 512; i++)
        if (bin->libraries[i]) free(bin->libraries[i]);
    if (bin->absolute_dir) free(bin->absolute_dir);
    if (bin->relative_dir) free(bin->relative_dir);
    if (bin->path) free(bin->path);
    if (bin->file_name) free(bin->file_name);
    if (bin->argv[0]) free(bin->argv[0]);
    if (bin->cmd) free(bin->cmd);
    if (bin) free(bin);
}

void vpmu_execute_binary(VPMUBinary *binary)
{
    pid_t pid = fork();

    if (binary == NULL || binary->path == NULL || strlen(binary->path) == 0) {
        ERR_MSG("Error, command is empty");
        return;
    }

    if (pid == -1) {
        ERR_MSG("Error, failed to fork()");
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        LOG_MSG("Executing '%s'", binary->path);
        // we are the child
        execvp(binary->path, binary->argv);
        _exit(EXIT_FAILURE); // exec never returns
    }
}

void vpmu_monitor_binary(VPMUHandler handler, VPMUBinary *binary)
{
    if (binary->path) {
        // Send the libraries to VPMU
        vpmu_load_and_send_libs(handler, binary);
        // Send the main program to VPMU (this must be the last one)
        vpmu_load_and_send(handler, binary->path);
        LOG_MSG("Monitoring: '%s'", binary->path);
    } else {
        // Just tell VPMU to monitor the process with the name
        HW_W(VPMU_MMAP_ADD_PROC_NAME, binary->argv[0]);
        LOG_MSG("Monitoring: '%s'", binary->argv[0]);
    }
    vpmu_reset_counters(handler);
    LOG_MSG("Please use controller to print report when need");
}

void vpmu_stop_monitoring_binary(VPMUHandler handler, VPMUBinary *binary)
{
    if (binary->path) {
        HW_W(VPMU_MMAP_REMOVE_PROC_NAME, binary->path);
        LOG_MSG("Stop Monitoring: '%s'", binary->path);
    } else {
        HW_W(VPMU_MMAP_REMOVE_PROC_NAME, binary->argv[0]);
        LOG_MSG("Stop Monitoring: '%s'", binary->argv[0]);
    }
}

void vpmu_profile_binary(VPMUHandler handler, VPMUBinary *binary)
{
    if (binary->path == NULL) {
        ERR_MSG("Can't find and execute '%s'", binary->argv[0]);
        return;
    }
    // Send the libraries to VPMU
    vpmu_load_and_send_libs(handler, binary);
    // Send the main program to VPMU (this must be the last one)
    vpmu_load_and_send(handler, binary->path);

    vpmu_reset_counters(handler);
    vpmu_execute_binary(binary);

    HW_W(VPMU_MMAP_REMOVE_PROC_NAME, binary->path);
    HW_W(VPMU_MMAP_REPORT, VPMU_DONT_CARE);
}

void vpmu_do_exec(VPMUHandler handler, const char *cmd_str)
{
    VPMUBinary *binary = NULL;
    { // Parse command string to VPMU binary struct
        char *cmd = trim(cmd_str);
        binary    = parse_all_paths_args(cmd);
        vpmu_update_library_list(binary);
        free(cmd);
    }

    if (handler.flag_monitor) {
        vpmu_monitor_binary(handler, binary);
    } else if (handler.flag_remove) {
        vpmu_stop_monitoring_binary(handler, binary);
    } else if (handler.flag_trace) {
        vpmu_profile_binary(handler, binary);
    } else {
        vpmu_execute_binary(binary);
    }
    free_vpmu_binary(binary);
}

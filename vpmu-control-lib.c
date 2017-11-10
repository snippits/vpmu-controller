#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   // access()
#include <ctype.h>    // isspace()
#include <sys/mman.h> // mmap(), MAP_SHARED
#include <sys/wait.h> // waitpid()
#include <fcntl.h>    // open(), close()
#include <libgen.h>   // basename(), dirname()

#include "vpmu-control-lib.h" // Main headers
#include "vpmu-path-lib.h"    // Helpers functions to parse string like shell
#include "vpmu-elf.h"         // Helpers functions for ELF formats

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

bool is_ascii_file(const char *path)
{
    if (path == NULL) return NULL;
    FILE *fp = fopen(path, "rt");

    if (fp) {
        int  i;
        char buffer[8192]; // 8K bytes checking
        int  len = fread(buffer, 1, sizeof(buffer), fp);
        fclose(fp);

        for (i = 0; i < len; i++) {
            // Return false if hit any non-character word
            if (!isascii(buffer[i])) {
                return false;
            }
        }
    }

    return true;
}

char *read_first_line(const char *path)
{
    if (path == NULL) return NULL;
    FILE *fp     = fopen(path, "rt");
    char *output = NULL;

    if (fp) {
        int  i;
        char buffer[128];
        int  len = fread(buffer, 1, sizeof(buffer), fp);
        fclose(fp);

        for (i = 0; i < len; i++) {
            // Break if hit any non-character word
            if (!isascii(buffer[i])) {
                // Return fail if ASCII-check fails
                if (output) {
                    free(output);
                    output = NULL;
                }
                break;
            }
            // Find the character of nextline
            if (output == NULL && buffer[i] == '\n') {
                // Replace it with End of String
                buffer[i] = '\0';
                // Return the copy of line
                output = strdup(buffer);
                // Do not break here to allow checking ASCII on the whole string
            }
        }
    }

    return output;
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

void vpmu_load_and_send(VPMUHandler handler,
                        const char *binary_path,
                        const char *script_path)
{
    // The final path
    char path[1024] = {};
    // The buffer of file
    char *buffer = NULL;
    // The size of buffer
    size_t size = 0;

    if (binary_path == NULL) return;
    if (access(binary_path, F_OK) != -1) {
        // File exist, send it even it's not executable
        strncpy(path, binary_path, sizeof(path));
        DBG_MSG("%-30s%s\n", "[vpmu_load_and_send]", "binary_path exists");
    } else { // Find executables in the $PATH
        char *basec = strdup(binary_path);
        char *bpath = locate_path(basename(basec));
        strncpy(path, bpath, sizeof(path));
        free(basec);
        free(bpath);
        DBG_MSG("%-30s%s\n", "[vpmu_load_and_send]", "Find in $PATH");
    }

    size = load_binary(path, &buffer);
    if (size > 0) {
        if (script_path) {
            // Use script path if there is one
            HW_W(VPMU_MMAP_ADD_PROC_NAME, script_path);
        } else {
            HW_W(VPMU_MMAP_ADD_PROC_NAME, path);
        }
        // Always pass main (real) binary even it's a script
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
            vpmu_load_and_send(handler, path, NULL);
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

static void set_binary_as_a_script(VPMUBinary *binary, const char *bin_path)
{
    binary->is_script   = true;
    binary->script_path = binary->path;     // Reset path to script path
    binary->path        = strdup(bin_path); // Set the real binary path
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

    // This is just a double check in case cmd is an empty string
    if (binary->argv[0]) {
        char *dirc  = strdup(binary->argv[0]);
        char *basec = strdup(binary->argv[0]);
        char *dname = dirname(dirc);
        char *bname = basename(basec);
        char *path  = NULL;

        binary->file_name    = strdup(bname);
        binary->absolute_dir = startwith(cmd, "/") ? strdup(dname) : locate_path(bname);
        binary->relative_dir = startwith(cmd, "./")
                                 ? strdup(dname)
                                 : startwith(cmd, "../") ? strdup(dname) : strdup("");

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

    char *line = read_first_line(binary->path);
    if (line) {
        if (startwith(line, "#!/usr/bin/env")) {
            char *path = locate_binary(&line[strlen("#!/usr/bin/env ")]);
            set_binary_as_a_script(binary, path);
            free(path);
        } else if (startwith(line, "#!/")) {
            set_binary_as_a_script(binary, &line[2]); // Skip #!
        } else if (access(binary->path, X_OK) != -1 && is_ascii_file(binary->path)) {
            ERR_MSG("Fallback detect as a bash script!!\n\n");
            set_binary_as_a_script(binary, "/bin/bash");
        }
        free(line);
    }

    DRY_MSG("Command String   : '%s'\n", binary->cmd);
    DRY_MSG("Absolute Dir     : '%s'\n", binary->absolute_dir);
    DRY_MSG("Relative Dir     : '%s'\n", binary->relative_dir);
    DRY_MSG("Binary name      : '%s'\n", binary->file_name);
    DRY_MSG("Binary Path      : '%s'\n", binary->path);
    if (binary->is_script) DRY_MSG("Script Path      : '%s'\n", binary->script_path);
    DRY_MSG("# of arguments   : %d\n", binary->argc);
    for (i = 0; i < binary->argc; i++) {
        DRY_MSG("    ARG[%d]      : '%s'\n", i, binary->argv[i]);
    }
    if (binary->path == NULL) {
        DBG_MSG("%-30sFile path '%s' does not exist.\n",
                "[parse_all_paths_args]",
                binary->argv[0]);
    }

    // Check whether the target binary is executable if it exists
    if (access(binary->path, F_OK) != -1 && access(binary->path, X_OK) == -1) {
        ERR_MSG("Target binary '%s' is not executable!", binary->path);
        free(binary->path);
        binary->path = NULL;
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
    if (binary == NULL || binary->path == NULL || strlen(binary->path) == 0) {
        ERR_MSG("Error, command '%s' not found", binary->argv[0]);
        exit(4);
    }

    pid_t pid = fork();
    if (pid == -1) {
        ERR_MSG("Error, failed to fork()");
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        LOG_MSG("Executing '%s'", binary->path);
        // we are the child
        if (binary->is_script) {
            execvp(binary->script_path, binary->argv);
        } else {
            execvp(binary->path, binary->argv);
        }
        _exit(EXIT_FAILURE); // exec never returns
    }
}

void vpmu_monitor_binary(VPMUHandler handler, VPMUBinary *binary)
{
    if (binary->path) {
        // Send the libraries to VPMU
        vpmu_load_and_send_libs(handler, binary);
        // Send the main program to VPMU (this must be the last one)
        vpmu_load_and_send(handler, binary->path, binary->script_path);
        if (binary->is_script)
            LOG_MSG("Monitoring: '%s'", binary->script_path);
        else
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
        if (binary->is_script) {
            HW_W(VPMU_MMAP_REMOVE_PROC_NAME, binary->script_path);
            LOG_MSG("Stop Monitoring: '%s'", binary->script_path);
        } else {
            HW_W(VPMU_MMAP_REMOVE_PROC_NAME, binary->path);
            LOG_MSG("Stop Monitoring: '%s'", binary->path);
        }
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
    if (binary->is_script) {
        vpmu_load_and_send(handler, binary->path, binary->script_path);
    } else {
        vpmu_load_and_send(handler, binary->path, NULL);
    }

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
    if (binary == NULL) return;

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

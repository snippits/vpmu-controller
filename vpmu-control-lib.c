#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // access()
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>

#include <libgen.h> // basename(), dirname()

#include "vpmu-control-lib.h"
#include "efd.h"

#ifdef DRY_RUN
#pragma message "DRY_RUN is defined. Compiled with dry run!!"
#endif

#ifdef DRY_RUN
#define DRY_MSG(str, ...) fprintf(stderr, "\033[1;32m" str "\033[0;00m", ##__VA_ARGS__)
#else
#define DRY_MSG(str, ...)                                                                \
    {                                                                                    \
    }
#endif

vpmu_handler_t vpmu_open(const char *dev_path, off_t address_offset)
{
    vpmu_handler_t handler = (vpmu_handler_t)malloc(sizeof(VPMU_HANDLER));

#ifdef DRY_RUN
    handler->ptr = (uintptr_t *)malloc(1024);
#else
    handler->fd = open(dev_path, O_RDWR | O_SYNC);
    if (handler->fd < 0) {
        printf("ERROR: Open Failed\n");
        exit(-1);
    }
    handler->ptr = (uintptr_t *)mmap(NULL,
                                     VPMU_DEVICE_IOMEM_SIZE,
                                     PROT_READ | PROT_WRITE,
                                     MAP_SHARED,
                                     handler->fd,
                                     address_offset);
    if (handler->ptr == MAP_FAILED) {
        printf("ERROR: Map Failed\n");
        exit(-1);
    }
    handler->flag_model = 0;
    handler->flag_trace = 0;
    handler->flag_jit   = 0;
#endif

    return handler;
}

void vpmu_close(vpmu_handler_t handler)
{
#ifdef DRY_RUN
    free(handler->ptr);
#else
    munmap(handler->ptr, 1024);
    close(handler->fd);
#endif
    free(handler);
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

int startwith(const char *pre, const char *str)
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

static void trim_cmd(char *s)
{
    char *p = s;
    int   l = strlen(p);

    while (isspace(p[l - 1])) p[--l] = 0;
    while (*p && isspace(*p)) ++p, --l;

    memmove(s, p, l + 1);

    // Dealing with escape space at the end of command
    if (s[strlen(s) - 1] == '\\') {
        // Add another space if the last character is a space
        s[strlen(s)]     = ' ';
        s[strlen(s) + 1] = '\0';
    }
}

static char *locate_binary(const char *bname)
{
    char *sys_path        = NULL;
    char *pch             = NULL;
    char *out_path        = NULL;
    char  full_path[1024] = {0};

    if (bname == NULL) return strdup("");
    sys_path = strdup(getenv("PATH"));
    pch      = strtok(sys_path, ":");
    while (pch != NULL) {
        strcpy(full_path, pch);
        if (pch[strlen(pch) - 1] != '/') strcat(full_path, "/");
        strcat(full_path, bname);
        if (access(full_path, X_OK) != -1) { // File exist and is executable
            out_path = strdup(pch);
            // Remove the tailing slash for consistency
            if (out_path[strlen(out_path) - 1] == '/')
                out_path[strlen(out_path) - 1] = '\0';
            break;
        }
        pch = strtok(NULL, ":");
    }

    free(sys_path);

    if (out_path == NULL) return strdup("");
    return out_path;
}

static char *find_ld_path(char *message)
{
    int   i      = 0;
    int   offset = 0;
    int   index  = 0;
    char *pch    = strstr(message, "=>");

    if (pch == NULL) {
        // Not found in the path, it's just name
        for (i = 0; message[i] != '\0'; i++) {
            if (message[i] == '\\') continue;
            if (index == 0 && message[i] != ' ') index = i;
            if (index != 0 && message[i] == ' ') {
                message[i] = '\0';
                return &message[index];
            }
        }
    } else {
        // Found in the path
        offset = (int)(pch - message) + strlen("=>");
        for (i = offset; message[i] != '\0'; i++) {
            if (message[i] == '\\') continue;
            if (index == 0 && message[i] != ' ') index = i;
            if (index != 0 && message[i] == ' ') {
                message[i] = '\0';
                return &message[index];
            }
        }
    }
    return NULL;
}

static inline int isquote(char c)
{
    return (c == '"' || c == '\'');
}

// Tokenize string by spaces
static int tokenize(char *str)
{
    int i           = 0;
    int size        = 0;
    int escape_flag = 0;
    int cnt         = 1;

    if (str != NULL) size = strlen(str);
    // String tokenize
    for (i = 0; i < size; i++) {
        char c = str[i];
        // Skip escape characters
        if (!escape_flag && c == '\\') {
            i++;
            continue;
        }
        // Escape quated string
        if (!escape_flag && isquote(c)) {
            escape_flag = 1;
        } else if (escape_flag && isquote(c)) {
            escape_flag = 0;
        }
        // Set \0 to all spaces which is not escaped
        if (!escape_flag && isspace(c)) {
            str[i] = '\0';
            cnt++;
        }
    }

    return cnt;
}

static void vpmu_binary_tokenize(VPMUBinary *binary)
{
    char *cmd = binary->argv[0];
    int   len = strlen(cmd);
    int   i = 0, j = 0;

    binary->argc = tokenize(cmd);
    for (i = 0, j = 0; i < binary->argc; i++) {
        binary->argv[i] = &cmd[j];
        for (; j < len; j++) {
            if (cmd[j] == '\0') break;
        }
        j++; // Advance one step to the next character
        if (j == len) break;
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

int vpmu_read_file(const char *path, char **buffer)
{
    FILE *fp = NULL;
    int   file_size;
    char *sys_path = strdup(getenv("PATH"));
    char *pch;
    char  full_path[1024] = {0};

    if (path == NULL || strlen(path) == 0) return 0;
    fp = fopen(path, "rb");
    if (fp == NULL) {
        pch = strtok(sys_path, ":");
        while (pch != NULL) {
            strcpy(full_path, pch);
            int size = strlen(full_path);
            if (full_path[size - 1] != '/') {
                full_path[size]     = '/';
                full_path[size + 1] = '\0';
            }
            strcat(full_path, path);
            fp = fopen(full_path, "rb");
            if (fp != NULL) break;
            pch = strtok(NULL, ":");
        }
        if (fp == NULL) {
            free(sys_path);
            printf("vpmu-control: File (%s) not found\n", path);
            exit(-1);
        }
    }
    free(sys_path);

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *buffer = NULL;
    *buffer = (char *)malloc(file_size);
    if (*buffer == NULL) {
        printf("vpmu-control: Fail to allocate memory\n");
        exit(-1);
    }
    fread(*buffer, file_size, 1, fp);
    fclose(fp);

    return file_size;
}

void vpmu_fork_exec(VPMUBinary *binary)
{
    pid_t pid = fork();

    if (binary == NULL || strlen(binary->path) == 0) {
        printf("vpmu-control: error, command is empty\n");
        return;
    }

    if (pid == -1) {
        printf("vpmu-control: error, failed to fork()");
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        printf("vpmu-control: Executing '%s'\n", binary->path);
        // we are the child
        execvp(binary->path, binary->argv);
        _exit(EXIT_FAILURE); // exec never returns
    }
}

int is_dynamic_binary(char *file_path)
{
    FILE * fp         = fopen(file_path, "rb");
    size_t lSize      = 0;
    char * buffer     = NULL;
    int    is_dynamic = 0;
    int    i          = 0;

    if (fp == NULL) {
        printf("vpmu-control: File '%s' not found\n", file_path);
        exit(-1);
    }
    // obtain file size:
    fseek(fp, 0, SEEK_END);
    lSize = ftell(fp);
    rewind(fp);

    // allocate memory to contain the whole file
    buffer = (char *)malloc(sizeof(char) * lSize);
    if (buffer == NULL) {
        fprintf(stderr, "Memory error");
        exit(2);
    }

    fread(buffer, 1, lSize, fp);

    // Check the magic word
    if (lSize >= 3 && buffer[0] == 'E' && buffer[1] == 'L' && buffer[2] == 'F') {
        for (i = 0; i < lSize - 6; i++) {
            if (buffer[i] == 'G' // Compare to string GLIBC_
                && buffer[i + 1] == 'L'
                && buffer[i + 2] == 'I'
                && buffer[i + 3] == 'B'
                && buffer[i + 4] == 'C'
                && buffer[i + 5] == '_') {
                is_dynamic = 1;
                break;
            }
        }
    }

    free(buffer);
    fclose(fp);

    return is_dynamic;
}

char **get_library_list(const char *cmd)
{
    char   new_command[1024] = "LD_TRACE_LOADED_OBJECTS=1 ";
    FILE * fp                = NULL;
    size_t file_size         = 0;
    char   message[1024]     = {0}; // No longer than 1024 characters per line
    char **library_list      = NULL;
    int    cnt               = 0;
    int    size_of_list      = 128;

    strcat(new_command, cmd);
    fp = popen(new_command, "r");
    if (fp == NULL) return NULL;
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (file_size == 0) return NULL;
    library_list = (char **)malloc(sizeof(char *) * size_of_list);
    DRY_MSG("Find shared libraries in this binary\n");
    while (fgets(message, sizeof(message), fp) != NULL) {
        library_list[cnt] = strdup(find_ld_path(message));
        if (library_list[cnt] != NULL) {
            DRY_MSG("    %d) %s\n", cnt, library_list[cnt]);
            cnt++;
        }
        if (cnt >= size_of_list) {
            size_of_list *= 2;
            library_list = (char **)realloc(library_list, sizeof(char *) * size_of_list);
        }
    }
    library_list[cnt] = NULL; // Terminate the list
    return library_list;
}

void release_library_list(char **library_list)
{
    int i = 0;

    if (library_list == NULL) return;
    for (i = 0; library_list[i] != NULL; i++) free(library_list[i]);
    free(library_list);
}

void vpmu_load_and_send(vpmu_handler_t handler, const char *file_path)
{
    char *basec  = strdup(file_path);
    char *buffer = NULL;
    // The final path
    const char *path = NULL;
    // The size of buffer
    size_t size = 0;

    if (access(file_path, F_OK) != -1) {
        // File exist, send it even it's not executable
        size = vpmu_read_file(file_path, &buffer);
        path = file_path;
    } else { // Find executables in the $PATH
        path = locate_binary(basename(basec));
        size = vpmu_read_file(path, &buffer);
    }
    if (size == 0) return;

    HW_W(VPMU_MMAP_ADD_PROC_NAME, path);
    HW_W(VPMU_MMAP_SET_PROC_SIZE, size);
    HW_W(VPMU_MMAP_SET_PROC_BIN, buffer);

    DRY_MSG("    send binary path      : %s\n", path);
    DRY_MSG("    send binary size      : %lu\n", size);
    DRY_MSG("    send buffer pointer   : %p\n", buffer);
    DRY_MSG("\n");

    if (buffer) free(buffer);
    free(basec);
}

void vpmu_load_and_send_all(vpmu_handler_t handler, const char *cmd)
{
    int    j;
    char **library_list = NULL;

    library_list = get_library_list(cmd);

    for (j = 0; library_list[j] != NULL; j++) {
        if (library_list[j][0] != '/' && library_list[j][0] != '.') {
            // Skip libraries that are still just a name (not found)
            continue;
        } else {
            vpmu_load_and_send(handler, library_list[j]);
        }
    }

    release_library_list(library_list);
}

void free_vpmu_binary(VPMUBinary *bin)
{
    if (bin->absolute_dir) free(bin->absolute_dir);
    if (bin->relative_dir) free(bin->relative_dir);
    if (bin->path) free(bin->path);
    if (bin->file_name) free(bin->file_name);
    if (bin->argv[0]) free(bin->argv[0]);
    if (bin) free(bin);
}

// "cmd" is an input argument, others are output arguments
VPMUBinary *parse_all_paths_args(const char *cmd)
{
    int i = 0;
    // Return value
    VPMUBinary *binary = (VPMUBinary *)malloc(sizeof(VPMUBinary));

    // Reset all pointers
    memset(binary, 0, sizeof(VPMUBinary));

    // Tokenize the command string into argv
    binary->argv[0] = strdup(cmd);
    vpmu_binary_tokenize(binary);

    if (binary->argv[0]) {
        char *dirc  = strdup(binary->argv[0]);
        char *basec = strdup(binary->argv[0]);
        char *dname = dirname(dirc);
        char *bname = basename(basec);
        char *path  = NULL;

        binary->absolute_dir = startwith("/", cmd) ? strdup(dname) : locate_binary(bname);
        binary->relative_dir = startwith("./", cmd) ? strdup(dname) : strdup("");
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

    DRY_MSG("Command String   : '%s'\n", cmd);
    DRY_MSG("Absolute Dir     : '%s'\n", binary->absolute_dir);
    DRY_MSG("Relative Dir     : '%s'\n", binary->relative_dir);
    DRY_MSG("Binary name      : '%s'\n", binary->file_name);
    DRY_MSG("# of arguments   : %d\n", binary->argc);
    for (i = 0; i < binary->argc; i++) {
        DRY_MSG("    ARG[%d]      : '%s'\n", i, binary->argv[i]);
    }
    if (binary->path == NULL) {
        printf("vpmu-control: File '%s' does not exist.", binary->file_name);
        exit(-1);
    }

    return binary;
}

vpmu_handler_t vpmu_parse_arguments(int argc, char **argv)
{
    vpmu_handler_t handler;
    int            i;
    char           dev_path[256] = "/dev/vpmu-device-0";
    off_t          vpmu_offset   = 0;

    if (argc < 2) {
        vpmu_print_help_message(argv[0]);
        printf("vpmu-control: Too less arguments\n");
        exit(-1);
    }

    for (i = 0; i < argc; i++) {
        if (STR_IS(argv[i], "--mem")) {
            strcpy(dev_path, "/dev/mem");
            vpmu_offset = VPMU_DEVICE_BASE_ADDR;
        } else if (STR_IS(argv[i], "--help")) {
            vpmu_print_help_message(argv[0]);
            exit(0);
        } else if (STR_IS(argv[i], "-h")) {
            vpmu_print_help_message(argv[0]);
            exit(0);
        }
    }
    handler = vpmu_open(dev_path, vpmu_offset);

    /* First Parse Settings/Configurations */
    for (i = 0; i < argc; i++) {
        if (STR_IS(argv[i], "--jit")) {
            DRY_MSG("enable jit\n");
            handler->flag_jit = 1;
            handler->flag_model |= VPMU_JIT_MODEL_SELECT;
        } else if (STR_IS(argv[i], "--trace")) {
            DRY_MSG("enable trace\n");
            handler->flag_trace = 1;
            handler->flag_model |= VPMU_EVENT_TRACE;
        } else if (STR_IS(argv[i], "--monitor")) {
            DRY_MSG("enable monitoring\n");
            handler->flag_monitor = 1;
            handler->flag_trace   = 1;
            handler->flag_model |= VPMU_EVENT_TRACE;
        } else if (STR_IS(argv[i], "--remove")) {
            DRY_MSG("enable monitoring\n");
            handler->flag_remove = 1;
        } else if (STR_IS(argv[i], "--phase")) {
            DRY_MSG("enable phase\n");
            DRY_MSG("enable trace\n");
            handler->flag_trace = 1;
            handler->flag_model |= VPMU_EVENT_TRACE;
            handler->flag_model |= VPMU_PHASEDET;
        } else if (STR_IS(argv[i], "--inst")) {
            handler->flag_model |= VPMU_INSN_COUNT_SIM;
        } else if (STR_IS(argv[i], "--cache")) {
            handler->flag_model |= VPMU_ICACHE_SIM | VPMU_DCACHE_SIM;
        } else if (STR_IS(argv[i], "--branch")) {
            handler->flag_model |= VPMU_BRANCH_SIM;
        } else if (STR_IS(argv[i], "--pipeline")) {
            handler->flag_model |= VPMU_PIPELINE_SIM;
        } else if (STR_IS(argv[i], "--all_models")) {
            handler->flag_model |= VPMU_INSN_COUNT_SIM | VPMU_ICACHE_SIM | VPMU_DCACHE_SIM
                                   | VPMU_BRANCH_SIM | VPMU_PIPELINE_SIM;
        }
    }

    /* Then parse all the action arguments */
    for (i = 0; i < argc; i++) {
        if (STR_IS_2(argv[i], "--read", "-r")) {
            int index = atoi(argv[++i]);
            int value = HW_R(index);
            DRY_MSG("read %d\n", index);
            printf("%d\n", value);
        } else if (STR_IS_2(argv[i], "--write", "-w")) {
            int index = atoi(argv[++i]);
            int value = atoi(argv[++i]);
            DRY_MSG("write %d at address %d\n", value, index);
            HW_W(index, value);
        } else if (STR_IS(argv[i], "--start")) {
            DRY_MSG("--start\n");
            if (handler->flag_trace == 0) {
                HW_W(VPMU_MMAP_ENABLE, handler->flag_model);
            }
        } else if (STR_IS(argv[i], "--end")) {
            DRY_MSG("--end\n");
            if (handler->flag_trace == 0) {
                // Only disable VPMU manually when it's not in trace mode
                HW_W(VPMU_MMAP_DISABLE, ANY_VALUE);
                HW_W(VPMU_MMAP_REPORT, ANY_VALUE);
            }
        } else if (STR_IS(argv[i], "--report")) {
            DRY_MSG("--report\n");
            HW_W(VPMU_MMAP_REPORT, ANY_VALUE);
        } else if (STR_IS_2(argv[i], "--exec", "-e")) {
            VPMUBinary *binary = NULL;
            // Command string
            char *cmd = strdup(argv[++i]);

            trim_cmd(cmd);
            binary = parse_all_paths_args(cmd);

            if (handler->flag_monitor) {
                // Send the libraries to VPMU
                if (!is_dynamic_binary(binary->path)) {
                    vpmu_load_and_send_all(handler, binary->path);
                }
                // Send the main program to VPMU (this must be the last one)
                vpmu_load_and_send(handler, binary->path);
                HW_W(VPMU_MMAP_SET_TIMING_MODEL, handler->flag_model);
                HW_W(VPMU_MMAP_RESET, ANY_VALUE);

                printf("Monitoring: %s\n", cmd);
                printf("Please use controller to print report when need\n");

            } else if (handler->flag_remove) {
                HW_W(VPMU_MMAP_REMOVE_PROC_NAME, binary->path);
            } else if (handler->flag_trace) {
                // Send the libraries to VPMU
                if (!is_dynamic_binary(binary->path)) {
                    vpmu_load_and_send_all(handler, binary->path);
                }
                // Send the main program to VPMU (this must be the last one)
                vpmu_load_and_send(handler, binary->path);

                HW_W(VPMU_MMAP_SET_TIMING_MODEL, handler->flag_model);
                HW_W(VPMU_MMAP_RESET, ANY_VALUE);

                vpmu_fork_exec(binary);
                HW_W(VPMU_MMAP_REMOVE_PROC_NAME, binary->path);
                HW_W(VPMU_MMAP_REPORT, ANY_VALUE);
            } else {
                vpmu_fork_exec(binary);
            }
            free_vpmu_binary(binary);
            free(cmd);
        }
    }

    return handler;
}

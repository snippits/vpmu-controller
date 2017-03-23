#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>

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

void vpmu_print_help_message(char *self)
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

static int locate_binary(char *path, char *out_path)
{
    FILE *fp       = NULL;
    char *sys_path = strdup(getenv("PATH"));
    char *pch;
    char  full_path[1024] = {0};

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
    }

    free(sys_path);
    if (fp != NULL)
        fclose(fp);
    else
        return -1;
    if (strlen(full_path) == 0) {
        strcpy(out_path, path);
    } else {
        strcpy(out_path, full_path);
    }
    return 0;
}

int vpmu_read_file(char *path, char **buffer)
{
    FILE *fp = NULL;
    int   file_size;
    char *sys_path = strdup(getenv("PATH"));
    char *pch;
    char  full_path[1024] = {0};

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

static void trim(char *s)
{
    char *p = s;
    int   l = strlen(p);

    while (isspace(p[l - 1])) p[--l] = 0;
    while (*p && isspace(*p)) ++p, --l;

    memmove(s, p, l + 1);
}

void vpmu_fork_exec(char *cmd)
{
    pid_t pid       = fork();
    char *args[128] = {NULL};
    int   i, idx = 0;
    int   size        = strlen(cmd);
    int   flag_string = 0;
    char *pch;

    if (pid == -1) {
        printf("vpmu-control: error, failed to fork()");
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    } else {
        printf("vpmu-control: Executing command '%s'\n", cmd);
        // String tokenize
        for (i = 1; i < size; i++) {
            if (flag_string == 0 && (cmd[i] == '"' || cmd[i] == '\'')) {
                flag_string = 1;
            } else if (flag_string == 1 && (cmd[i] == '"' || cmd[i] == '\'')) {
                flag_string = 0;
            }
            if (flag_string == 0 && cmd[i] == ' ' && cmd[i - 1] != '\\') {
                cmd[i] = '\0';
            }
        }

        pch = &cmd[0];
        i   = 0;
        while (pch != NULL) {
            if (pch[0] != '\0') {
                // printf("pch: %s\n", pch);
                args[idx++] = pch;
            }
            for (; i < size && cmd[i] != '\0'; i++)
                ;
            if (i == size)
                pch = NULL;
            else
                pch = &cmd[i + 1];
            i++;
        }
#ifdef DRY_RUN
        for (i = 0; i < 128; i++) {
            if (strlen(args[i]) > 0) DRY_MSG("%s\n", args[i]);
        }
#endif
        // we are the child
        execvp(args[0], args);
        _exit(EXIT_FAILURE); // exec never returns
    }
}

char *find_path(char *message)
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

    free(buffer);
    fclose(fp);

    return is_dynamic;
}

char **get_library_list(char *cmd)
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
        library_list[cnt] = strdup(find_path(message));
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
    for (i = 0; library_list[i] != NULL; i++) free(library_list[i]);
    free(library_list);
}

void load_and_send_to_vpmu(vpmu_handler_t handler, char *file_path)
{
    char   full_path[2048] = {0};
    char * buffer          = NULL;
    size_t size            = 0;

    // The full path would be set as a relative path if it's not found
    locate_binary(file_path, full_path);
    size = vpmu_read_file(file_path, &buffer);
    if (size == 0) return;

    HW_W(VPMU_MMAP_ADD_PROC_NAME, full_path);
    HW_W(VPMU_MMAP_SET_PROC_SIZE, size);
    HW_W(VPMU_MMAP_SET_PROC_BIN, buffer);

    DRY_MSG("    send binary path      : %s\n", full_path);
    DRY_MSG("    send binary size      : %lu\n", size);
    DRY_MSG("    send buffer pointer   : %p\n", buffer);
    DRY_MSG("\n");

    if (buffer) free(buffer);
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
        // fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);
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
            char *cmd = strdup(argv[++i]);
            trim(cmd);
            // Dealing with escape space at the end of command
            if (cmd[strlen(cmd) - 1] == '\\') {
                cmd[strlen(cmd)]     = ' ';
                cmd[strlen(cmd) + 1] = '\0';
            }

            if (handler->flag_trace) {
                // Names and paths
                char full_path[2048] = {0}, file_path[1024] = {0};
                char exec_name[512] = {0}, args[1024] = {0};
                // Indices
                int index = 0, index_path_end = 0;
                int j;
                // The pointer to a list of shared libraries
                char **library_list = NULL;

                // Locate the last string in the path
                for (j = 1; j < strlen(cmd); j++) {
                    if (cmd[j] == ' ' && cmd[j - 1] != '\\') {
                        break;
                    }
                    if (cmd[j] == '/' && cmd[j - 1] != '\\') {
                        index = j + 1;
                    }
                }
                // Find the end of the path
                for (j = index; j < strlen(cmd) && cmd[j] != ' '; j++)
                    ;
                index_path_end = j;

                strncpy(file_path, cmd, index_path_end);
                strncpy(args, cmd + index_path_end, strlen(cmd) - index_path_end);
                strncpy(exec_name, cmd + index, index_path_end - index);

                // The full path would be set as a relative path if it's not found
                locate_binary(file_path, full_path);

                DRY_MSG("command          : %s\n", cmd);
                DRY_MSG("binary name      : %s\n", exec_name);

                if (!is_dynamic_binary(full_path)) {
                    library_list = get_library_list(cmd);
                    for (j = 0; library_list[j] != NULL; j++) {
                        if (library_list[j][0] != '/' && library_list[j][0] != '.') {
                            // Skip libraries that are still just a name (not found)
                            continue;
                        } else {
                            load_and_send_to_vpmu(handler, library_list[j]);
                        }
                    }
                }

                // Send the main program to VPMU (this must be the last one)
                load_and_send_to_vpmu(handler, file_path);

                HW_W(VPMU_MMAP_SET_TIMING_MODEL, handler->flag_model);
                HW_W(VPMU_MMAP_RESET, ANY_VALUE);
                if (handler->flag_monitor) {
                    printf("Monitoring: %s\n", cmd);
                    printf("Please use controller to print report when need\n");
                } else {
                    vpmu_fork_exec(cmd);
                    HW_W(VPMU_MMAP_REMOVE_PROC_NAME, full_path);
                    HW_W(VPMU_MMAP_REPORT, ANY_VALUE);
                }
                if (library_list != NULL) release_library_list(library_list);
            } else {
                vpmu_fork_exec(cmd);
            }
            free(cmd);
        }
    }

    return handler;
}

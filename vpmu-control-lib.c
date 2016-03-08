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

vpmu_handler_t vpmu_open(off_t vpmu_address)
{
    vpmu_handler_t handler = (vpmu_handler_t)malloc(sizeof(VPMU_HANDLER));

    handler->fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (handler->fd < 0) {
        printf("ERROR: Open Failed\n");
        exit(-1);
    }
    handler->ptr = (unsigned int *)mmap(
            NULL,
            1024,
            PROT_READ | PROT_WRITE, MAP_SHARED,
            handler->fd,
            vpmu_address);
    if (handler->ptr == MAP_FAILED) {
        printf("ERROR: Map Failed\n");
        exit(-1);
    }
    handler->flag_trace = 0;
    handler->flag_jit = 0;

    return handler;
}

void vpmu_close(vpmu_handler_t handler)
{
    close(handler->fd);
    free(handler);
}


void vpmu_print_help_message(char *self)
{
#define HELP_MESG \
    "Usage: %s [options] {actions...}\n" \
    "Options:\n" \
    "  --jit         Enable just-in-time model selection on performance simulation\n" \
    "  --trace       Enable VPMU SET and user process function tracking ability\n" \
    "                If \"--trace\" is set, the process will be traced automatically\n" \
    "  --help        Show this message\n" \
    "\n\n" \
    "Actions:\n" \
    "  --read  <address>          Read data from <address> of VPMU and print it out with \"\\n\"\n" \
    "  --write <address> <data>   Write <data> to <address> of VPMU\n" \
    "  --start       Start VPMU profiling with all performance simulators ON\n" \
    "                If \"--trace\" is set, \"--start\" do nothing\n" \
    "  --end         End/Stop VPMU profiling and report the results\n" \
    "                If \"--trace\" is set, \"--end\" do nothing\n" \
    "  --report      Simply report the current results. It can be used while profiling \n" \
    "  --exec        Run the program/executable.\n" \
    "                If \"--trace\" is set, the controller will also pass some of the sections\n" \
    "                (i.e. symbol table, dynamic libraries) of target binary to VPMU.\n" \
    "\n"

    printf(HELP_MESG, self);
}

int vpmu_read_file(char *path, char **buffer)
{
    FILE *fp = NULL;
    int file_size;
    char *sys_path = getenv("PATH");
    char *pch;
    char full_path[1024] = {0};

    fp = fopen(path, "rb");
    if (fp == NULL) {
        pch = strtok(sys_path, ":");
        while (pch != NULL) {
            strcpy(full_path, pch);
            int size = strlen(full_path);
            if (full_path[size - 1] != '/') {
                full_path[size] = '/';
                full_path[size + 1] = '\0';
            }
            strcat(full_path, path);
            fp = fopen(full_path, "rb");
            if (fp != NULL) break;
            pch = strtok(NULL, ":");
        }
        if (fp == NULL) {
            printf("vpmu-control: File not found\n");
            exit(-1);
        }
    }

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *buffer = NULL;
    *buffer = (char *)malloc(file_size);
    if (*buffer == NULL) {
        printf("Fail to allocate memory\n");
        exit(-1);
    }
    fread(*buffer, file_size, 1, fp);
    fclose(fp);

    return file_size;
}

static void trim(char *s) {
    char * p = s;
    int l = strlen(p);

    while(isspace(p[l - 1])) p[--l] = 0;
    while(* p && isspace(* p)) ++p, --l;

    memmove(s, p, l + 1);
}

void vpmu_fork_exec(char *cmd)
{
    pid_t pid = fork();
    char *args[64] = {NULL};
    int i, idx = 0;
    int size = strlen(cmd);
    char *pch;

    if (pid == -1) {
        printf("error, failed to fork()");
    }
    else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
    else {
        printf("command: %s\n", cmd);
        //String tokenize
        for (i = 1; i < size; i++) {
            if (cmd[i] == ' ' && cmd[i - 1] != '\\') {
                cmd[i] = '\0';
            }
        }

        pch = &cmd[0];
        i = 0;
        while (pch != NULL) {
            if (pch[0] != '\0') {
                //printf("pch: %s\n", pch);
                args[idx++] = pch;
            }
            for (; i < size && cmd[i] != '\0'; i++);
            if (i == size) pch = NULL;
            else pch = &cmd[i + 1];
            i++;
        }

        // we are the child
        execvp(args[0], args);
        _exit(EXIT_FAILURE);   // exec never returns
    }
}

vpmu_handler_t vpmu_parse_arguments(int argc, char **argv)
{
    vpmu_handler_t handler;
    int i;
    char *buffer = NULL;

    if (argc < 2) {
        vpmu_print_help_message(argv[0]);
        printf("Too less arguments\n");
        exit(-1);
    }
    handler = vpmu_open(VPMU_PHYSICAL_ADDRESS);

    /* First Parse Settings/Configurations */
    for (i = 0; i < argc; i++) {
        if (STR_IS(argv[i], "--jit")) {
            handler->flag_jit = 1;
        }
        else if (STR_IS(argv[i], "--trace")) {
            handler->flag_trace = 1;
        }
        else if (STR_IS(argv[i], "--help")) {
            vpmu_print_help_message(argv[0]);
            exit(0);
        }
        else if (STR_IS(argv[i], "-h")) {
            vpmu_print_help_message(argv[0]);
            exit(0);
        }
    }

    /* Then parse all the action arguments */
    for (i = 0; i < argc; i++) {
        //fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);
        if (STR_IS(argv[i], "--read")) {
            int index = atoi(argv[++i]);
            int value = handler->ptr[index];
            printf("%d\n", value);
        }
        else if (STR_IS(argv[i], "--write")) {
            int index = atoi(argv[++i]);
            int value = atoi(argv[++i]);
            handler->ptr[index] = value;
        }
        else if (STR_IS(argv[i], "--start")) {
            if (handler->flag_trace == 0) {
                if (handler->flag_jit)
                    handler->ptr[0] = 8;
                else
                    handler->ptr[0] = 6;
            }
        }
        else if (STR_IS(argv[i], "--end")) {
            if (handler->flag_trace == 0) {
                handler->ptr[0] = 1;
            }
        }
        else if (STR_IS(argv[i], "--report")) {
            handler->ptr[0] = 11;
        }
        else if (STR_IS(argv[i], "--exec")) {
            char *cmd = strdup(argv[++i]);
            trim(cmd);
            //Dealing with escape space at the end of command
            if (cmd[strlen(cmd) - 1] == '\\') {
                cmd[strlen(cmd)] = ' ';
                cmd[strlen(cmd) + 1] = '\0';
            }

            if (handler->flag_trace) {
                char exec_name[256] = {0}, file_path[256] = {0}, args[256] = {0};
                int index = 0, index_path_end = 0;
                int j;
                //Locate the last string in the path
                for (j = 1; j < strlen(cmd); j++) {
                    if (cmd[j] == '/' && cmd[j - 1] != '\\') {
                        index = j + 1;
                    }
                }
                //Find the end of the path
                for (j = index; j < strlen(cmd) && cmd[j] != ' '; j++);
                index_path_end = j;

                strncpy(file_path, cmd, index_path_end);
                strncpy(args, cmd + index_path_end, strlen(cmd) - index_path_end);

                strncpy(exec_name, cmd + index, index_path_end - index);
                handler->ptr[16] = (int)exec_name;

                int size = vpmu_read_file(file_path, &buffer);
                handler->ptr[17] = (int)size;
                handler->ptr[18] = (int)buffer;
            }
            vpmu_fork_exec(cmd);
            char null_str = '\0';
            handler->ptr[16] = (int)&null_str;
            free(cmd);
        }
    }

    return handler;
}



#include <stdio.h>
#include <stdlib.h>

#include "vpmu-control-lib.h"

static void check_arg_and_exit(int argc, char **argv, int cur_idx, int req_num)
{
    if ((cur_idx + req_num) >= argc) {
        ERR_MSG("# of argument is not enough!");
        ERR_MSG("This is happened due to %dth argument, '%s'.", cur_idx, argv[cur_idx]);
        exit(4);
    }
}

static void parse_options(VPMUHandler *handler, int argc, char **argv)
{
    int i = 0; // Declaring i here for C98

    for (i = 0; i < argc; i++) {
        if (arg_is(argv[i], "--jit")) {
            DRY_MSG("enable jit\n");
            handler->flag_jit = true;
            handler->flag_model |= VPMU_JIT_MODEL_SELECT;
        } else if (arg_is(argv[i], "--trace")) {
            DRY_MSG("enable trace\n");
            handler->flag_trace = true;
            handler->flag_model |= VPMU_EVENT_TRACE;
        } else if (arg_is(argv[i], "--monitor")) {
            DRY_MSG("enable monitoring\n");
            handler->flag_monitor = true;
            handler->flag_trace   = true;
            handler->flag_model |= VPMU_EVENT_TRACE;
        } else if (arg_is(argv[i], "--remove")) {
            DRY_MSG("enable monitoring\n");
            handler->flag_remove = true;
        } else if (arg_is(argv[i], "--phase")) {
            DRY_MSG("enable phase\n");
            DRY_MSG("enable trace\n");
            handler->flag_trace = true;
            handler->flag_model |= VPMU_EVENT_TRACE;
            handler->flag_model |= VPMU_PHASEDET;
        } else if (arg_is(argv[i], "--inst")) {
            handler->flag_model |= VPMU_INSN_COUNT_SIM;
        } else if (arg_is(argv[i], "--cache")) {
            handler->flag_model |= VPMU_ICACHE_SIM | VPMU_DCACHE_SIM;
        } else if (arg_is(argv[i], "--branch")) {
            handler->flag_model |= VPMU_BRANCH_SIM;
        } else if (arg_is(argv[i], "--pipeline")) {
            handler->flag_model |= VPMU_PIPELINE_SIM;
        } else if (arg_is(argv[i], "--all_models")) {
            handler->flag_model |= VPMU_INSN_COUNT_SIM | VPMU_ICACHE_SIM | VPMU_DCACHE_SIM
                                   | VPMU_BRANCH_SIM | VPMU_PIPELINE_SIM;
        }
    }
}

void print_help_message(const char *self)
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
    "    %s --all_models --start --exec \"ls -la\" --end\n"                              \
    "    %s --all_models --phase -e \"ls -la\"\n"                                        \
    "    %s --all_models --monitor -e ls\n"

    printf(HELP_MESG, self, self, self, self);
}

int main(int argc, char **argv)
{
    // Initialize handler with zeros
    VPMUHandler handler = {};
    // Default device
    char dev_path[256] = "/dev/vpmu-device-0";
    // Declaring i here for C98
    int i = 0;

    if (argc < 2) {
        print_help_message(argv[0]);
        ERR_MSG("Too less arguments\n");
        exit(-1);
    }

    for (i = 0; i < argc; i++) {
        if (arg_is(argv[i], "--mem")) {
            strcpy(dev_path, "/dev/mem");
        } else if (arg_is_2(argv[i], "--help", "-h")) {
            print_help_message(argv[0]);
            exit(0);
        }
    }
    // After parsing the real path of vpmu-device and help message,
    // we can do the initialization now.
    handler = vpmu_open(dev_path);

    // First Parse Settings/Configurations
    parse_options(&handler, argc, argv);

    // Then parse all the action arguments
    for (i = 0; i < argc; i++) {
        if (arg_is_2(argv[i], "--read", "-r")) {
            check_arg_and_exit(argc, argv, i, 1);
            uintptr_t index = atoll(argv[++i]);
            uintptr_t value = vpmu_read_value(handler, index);
            printf("%zu\n", value);
        } else if (arg_is_2(argv[i], "--write", "-w")) {
            check_arg_and_exit(argc, argv, i, 2);
            uintptr_t index = atoll(argv[++i]);
            uintptr_t value = atoll(argv[++i]);
            vpmu_write_value(handler, index, value);
        } else if (arg_is(argv[i], "--start")) {
            // Only do this when it's not in trace mode
            if (handler.flag_trace == false) vpmu_start_fullsystem_tracing(handler);
        } else if (arg_is(argv[i], "--end")) {
            // Only do this when it's not in trace mode
            if (handler.flag_trace == false) vpmu_end_fullsystem_tracing(handler);
        } else if (arg_is(argv[i], "--report")) {
            vpmu_print_report(handler);
        } else if (arg_is_2(argv[i], "--exec", "-e")) {
            check_arg_and_exit(argc, argv, i, 1);
            vpmu_do_exec(handler, argv[++i]);
        }
    }

    vpmu_close(handler);
    return 0;
}

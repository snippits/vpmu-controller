#include <stdio.h>
#include <stdlib.h>

#include "vpmu-control-lib.h"

void check_arg_and_exit(int argc, int cur_idx, int req_num)
{
    if ((cur_idx + req_num) >= argc) {
        ERR_MSG("# of argument is not enough!");
        ERR_MSG("This is happened at %dth argument", cur_idx + 1);
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

int main(int argc, char **argv)
{
    // Initialize handler with zeros
    VPMUHandler handler = {};
    // Default device
    char dev_path[256] = "/dev/vpmu-device-0";
    // Declaring i here for C98
    int i = 0;

    if (argc < 2) {
        vpmu_print_help_message(argv[0]);
        ERR_MSG("Too less arguments\n");
        exit(-1);
    }

    for (i = 0; i < argc; i++) {
        if (arg_is(argv[i], "--mem")) {
            strcpy(dev_path, "/dev/mem");
        } else if (arg_is_2(argv[i], "--help", "-h")) {
            vpmu_print_help_message(argv[0]);
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
            check_arg_and_exit(argc, i, 1);
            uint64_t index = atoll(argv[++i]);
            uint64_t value = vpmu_read_value(handler, index);
            printf("%" PRIu64 "\n", value);
        } else if (arg_is_2(argv[i], "--write", "-w")) {
            check_arg_and_exit(argc, i, 2);
            uint64_t index = atoll(argv[++i]);
            uint64_t value = atoll(argv[++i]);
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
            check_arg_and_exit(argc, i, 1);
            vpmu_profile(handler, argv[++i]);
        }
    }

    vpmu_close(handler);
    return 0;
}

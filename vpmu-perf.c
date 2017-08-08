#include <stdio.h>
#include <stdlib.h>

#include "vpmu-path-lib.h"
#include "vpmu-control-lib.h"

void print_help_message(const char *self)
{
#define HELP_MESG                                                                        \
    "Usage: %s [options] COMMAND [ARGS]\n"                                               \
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
    "Example:\n"                                                                         \
    "    %s --all_models ls -al\n"                                                       \
    "    %s --all_models --monitor ls -al\n"

    printf(HELP_MESG, self, self, self);
}

static int parse_options(VPMUHandler *handler, int argc, char **argv)
{
    int i = 0; // Declaring i here for C98

    for (i = 1; i < argc; i++) {
        // Return at first non -- argument
        if (!startwith(argv[i], "--")) return i;
        if (arg_is(argv[i], "--help")) {
            print_help_message(argv[0]);
            exit(0);
        } else if (arg_is(argv[i], "--jit")) {
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
    return i;
}

void profile_binary(VPMUHandler handler, int argc, char **argv)
{
    if (argc == 0 || argv == NULL) return;

    // Parse command string to VPMU binary struct
    VPMUBinary *binary = parse_all_paths_args(argv[0]);
    if (binary == NULL) return;
    // Copy all the arguments to VPMU binary struct
    int i;
    binary->argc = argc;
    // Since argv[0] has been set already, no need to set it again here.
    for (i = 1; i < argc; i++) {
        binary->argv[i] = argv[i]; // VPMU control lib does not free argv idx>=1
        DRY_MSG("    ARG[%d]      : '%s'\n", i, binary->argv[i]);
    }
    vpmu_update_library_list(binary);

    if (handler.flag_monitor) {
        vpmu_monitor_binary(handler, binary);
    } else if (handler.flag_remove) {
        vpmu_stop_monitoring_binary(handler, binary);
    } else if (handler.flag_trace) {
        vpmu_profile_binary(handler, binary);
    } else {
        vpmu_start_fullsystem_tracing(handler);
        vpmu_execute_binary(binary);
        vpmu_end_fullsystem_tracing(handler);
    }
    free_vpmu_binary(binary);
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
        exit(-1);
    }

    for (i = 0; i < argc; i++) {
        if (arg_is(argv[i], "--mem")) {
            strcpy(dev_path, "/dev/mem");
        }
    }
    // After parsing the real path of vpmu-device and help message,
    // we can do the initialization now.
    handler = vpmu_open(dev_path);

    // First Parse Settings/Configurations
    int cmd_idx = parse_options(&handler, argc, argv);

    if (cmd_idx == argc) {
        ERR_MSG("No command specified!");
    } else {
        DRY_MSG("Command '%s' at index %d\n", argv[cmd_idx], cmd_idx);
        profile_binary(handler, argc - cmd_idx, &argv[cmd_idx]);
    }

    vpmu_close(handler);
    return 0;
}

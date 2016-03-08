#include <stdio.h>
#include <stdlib.h>

#include "vpmu-control-lib.h"

int main(int argc, char **argv)
{
    vpmu_handler_t handler = vpmu_parse_arguments(argc, argv);
    vpmu_close(handler);
    return 0;
}



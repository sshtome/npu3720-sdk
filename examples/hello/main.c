#include "npu3720_sdk.h"

void kmain(void)
{
    npu_init("NPU3720-HELLO");
    npu_log_puts("Hello from bare-metal NPU 3720 SDK!\n");

    for (;;)
        npu_poll();
}

#include "gpu.h"

int main(void)
{
    *gpu_addr = 0; // Reset GPU
    *gpu_control = 0x4400;
    return 0;
}

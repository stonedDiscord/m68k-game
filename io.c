#include "io.h"

uint8_t io1_in_raw[16];
uint16_t mux[8];

uint8_t inputs;

/*
 * scan_inputs – read 16 bits into io1_in_raw[].
 *
 */
void scan_inputs(void)
{
    uint8_t i;
    uint8_t j;

    /* read in 16 bits of all 8 channels */
    for (i = 0; i < 16; i++) {
        io1_in_raw[i] = *io1_device;   /* latch current channel */
    }

    for (i = 0; i < 8; i++) {
        mux[i] = 0x0000;
    }

    /* Demux into 8 16-bit words */
    for (i = 0; i < 16; i++) {
        for (j = 0; j < 8; j++)
        {
            mux[j] = (mux[j] << 1) | ((io1_in_raw[i] >> j) & 0x01);
        }
    }
}

uint16_t read_input(uint8_t n)
{
    return mux[n];
}

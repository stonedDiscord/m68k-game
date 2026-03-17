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

void uint16_to_hex(uint16_t value, char *out_str, size_t out_len)
{
    if (out_len < 5) {  // Need at least 4 hex digits + null terminator
        return;
    }
    for (int i = 0; i < 4; i++) {
        uint8_t nibble = (value >> (12 - 4*i)) & 0xF;
        out_str[i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
    }
    out_str[4] = '\0';
}
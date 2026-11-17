#include "io.h"

uint8_t io1_in_raw[16];
uint16_t mux[8];

uint8_t inputs;

/* SP/ME I clocks eight parallel serial chains; MUX1/MUX2 are bits 5/7.
 * Each lamp chain is 16 bits: LS0..11, LZ0..2, then EnSDAp (kept low).
 * disc4000.bin: FUN_00001068 shifts 16 bytes to 0x800101, then raises
 * ENMUX at 0x8000c1. See also stellafr.cpp mux_w/mux2_w/lamps_w.
 * Only lamp strobes are raised: coin, motor and display latches stay low. */
void lamp_test_tick(void)
{
    enum { LAMP_COLUMNS = 12, LAMP_COUNT = 16 * LAMP_COLUMNS,
           SNAKE_LENGTH = 8, STEP_MS = 128 };
    static uint8_t row;
    static uint8_t elapsed;
    static uint16_t first_lamp;
    uint16_t mux1 = (uint16_t)row << 12;
    uint16_t mux2 = mux1;

    for (uint8_t i = 0; i < SNAKE_LENGTH; i++) {
        uint16_t lamp = first_lamp + i;
        if (lamp >= LAMP_COUNT)
            lamp -= LAMP_COUNT;
        uint8_t lamp_row = lamp / LAMP_COLUMNS;
        uint16_t column = (uint16_t)1 << (lamp % LAMP_COLUMNS);
        if (lamp_row == row)
            mux1 |= column;
        if (lamp_row == row + 8)
            mux2 |= column;
    }

    *o2_device = 0;
    for (uint16_t bit = 0x8000; bit != 0; bit >>= 1) {
        uint8_t data = 0;
        if (mux1 & bit)
            data |= 0x20;
        if (mux2 & bit)
            data |= 0x80;
        *io1_device = data;
    }
    *o2_device = 0xa0; /* Rising ENMUX1/ENMUX2 edges latch both boards. */

    row = (row + 1) & 7;
    if (++elapsed == STEP_MS) {
        elapsed = 0;
        if (++first_lamp == LAMP_COUNT)
            first_lamp = 0;
    }
}

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

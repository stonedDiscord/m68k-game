#include "io.h"
#include <stdbool.h>

uint8_t io1_in_raw[16];
uint16_t mux[8];

uint8_t inputs;

/* SP/ME I clocks eight parallel serial chains; ANZ1/ANZ2 are bits 4/6, MUX1/MUX2
 * are bits 5/7.
 * Each lamp chain is 16 bits: LS0..11, LZ0..2, then EnSDAp (kept low).
 * disc4000.bin: FUN_00001068 shifts 16 bytes to 0x800101, then raises
 * ENMUX at 0x8000c1. See also stellafr.cpp mux_w/mux2_w/lamps_w.
 * ANZ is latched after each group of eight writes; ENMUX after sixteen.
 * Coin and motor latches stay low. */
static uint8_t digit_line(uint8_t chain, uint8_t field, uint8_t segment, uint8_t phase)
{
    /* Raw segment bytes count up modulo 256. Offset every position across
     * both chains so each output can be identified in the emulator. */
    uint8_t line = 0;
    uint8_t mask = (uint8_t)1 << segment;
    uint8_t value = phase + chain * 32 + field * 8;

    for (uint8_t bit = 0x80; bit != 0; bit >>= 1, value++) {
        if (value & mask)
            line |= bit;
    }
    return line;
}

void output_test_tick(void)
{
    enum { LAMP_COLUMNS = 12, LAMP_COUNT = 16 * LAMP_COLUMNS,
           SNAKE_LENGTH = 14, STEP_MS = 128 };
    static uint8_t row = 7;
    static uint8_t cycle;
    static uint8_t phase = 1;
    static uint8_t elapsed;
    static uint16_t first_lamp;
    static uint16_t next_lamp = SNAKE_LENGTH;
    static uint16_t lamp_rows[16];
    static bool lamps_ready;

    /* Keep the snake as row masks instead of rebuilding it with division
     * and variable shifts on every 1 ms interrupt. */
    if (!lamps_ready) {
        for (uint8_t i = 0; i < SNAKE_LENGTH; i++)
            lamp_rows[i / LAMP_COLUMNS] |= (uint16_t)1 << (i % LAMP_COLUMNS);
        lamps_ready = true;
    }
    uint16_t mux1 = ((uint16_t)row << 12) | lamp_rows[row];
    uint16_t mux2 = ((uint16_t)row << 12) | lamp_rows[row + 8];

    uint8_t pos = 7 - row;
    uint8_t field = (cycle << 1) | (pos >> 2);
    uint8_t segment = (pos & 3) * 2;
    uint8_t line1 = digit_line(0, field, segment, phase);
    uint8_t line2 = digit_line(1, field, segment, phase);
    uint8_t digit_bit = 0x80;

    *o2_device = 0;
    for (uint16_t bit = 0x8000; bit != 0; bit >>= 1) {
        uint8_t data = 0;
        if (mux1 & bit)
            data |= 0x20;
        if (mux2 & bit)
            data |= 0x80;
        if (line1 & digit_bit)
            data |= 0x10;
        if (line2 & digit_bit)
            data |= 0x40;
        *io1_device = data;
        digit_bit >>= 1;
        if (bit == 0x0100) {
            *o2_device = 0x50; /* Both ANZ chains, bank 0: even segment line. */
            *o2_device = 0;
            line1 = digit_line(0, field, segment + 1, phase);
            line2 = digit_line(1, field, segment + 1, phase);
            digit_bit = 0x80;
        }
    }
    *o2_device = 0x50; /* Both ANZ chains, bank 1: odd segment line. */
    *o2_device = 0xa0; /* Rising ENMUX1/ENMUX2 edges latch both boards. */

    if (row == 0)
        cycle ^= 1;
    row = (row - 1) & 7;
    if (++elapsed == STEP_MS) {
        elapsed = 0;
        lamp_rows[first_lamp / LAMP_COLUMNS] &=
            ~((uint16_t)1 << (first_lamp % LAMP_COLUMNS));
        lamp_rows[next_lamp / LAMP_COLUMNS] |=
            (uint16_t)1 << (next_lamp % LAMP_COLUMNS);
        if (++first_lamp == LAMP_COUNT)
            first_lamp = 0;
        if (++next_lamp == LAMP_COUNT)
            next_lamp = 0;
        phase++;
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

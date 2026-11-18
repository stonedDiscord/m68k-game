#include "io.h"

uint8_t io1_in_raw[16];
uint16_t mux[8];

uint8_t inputs;

/* SP/ME I clocks eight parallel serial chains; ANZ1 is bit 4, MUX1/MUX2
 * are bits 5/7.
 * Each lamp chain is 16 bits: LS0..11, LZ0..2, then EnSDAp (kept low).
 * disc4000.bin: FUN_00001068 shifts 16 bytes to 0x800101, then raises
 * ENMUX at 0x8000c1. See also stellafr.cpp mux_w/mux2_w/lamps_w.
 * ANZ is latched after each group of eight writes; ENMUX after sixteen.
 * Coin and motor latches stay low. */
static uint8_t digit_line(uint8_t field, uint8_t segment, uint8_t phase)
{
    /* Board-order fonts from disc4000.bin, 0x18c16 and 0x18cf6. */
    static const uint8_t font[2][10] = {
        { 0x77, 0x41, 0x6e, 0x6b, 0x59, 0x3b, 0x3f, 0x61, 0x7f, 0x7b },
        { 0xef, 0x89, 0x7d, 0xbd, 0x9b, 0xb7, 0xf7, 0x8d, 0xff, 0xbf }
    };
    /* Anzeigestelle 0..7: field and shift-register position, from the
     * ROM's position tables at 0x1c724/0x1c725 (see stellafr.cpp).
     * Populate only digits: field 3, position 3 drives coin magnets/LEDs. */
    static const uint8_t fields[8] = { 2, 1, 3, 0, 2, 1, 3, 0 };
    static const uint8_t positions[8] = { 4, 2, 2, 2, 2, 4, 4, 4 };
    uint8_t line = 0;

    for (uint8_t digit = 0; digit < 8; digit++) {
        if (fields[digit] == field) {
            uint8_t value = phase + digit;
            if (value >= 10)
                value -= 10;
            if (font[field & 1][value] & ((uint8_t)1 << segment))
                line |= (uint8_t)0x80 >> positions[digit];
        }
    }
    return line;
}

void output_test_tick(void)
{
    enum { LAMP_COLUMNS = 12, LAMP_COUNT = 16 * LAMP_COLUMNS,
           SNAKE_LENGTH = 8, STEP_MS = 128 };
    static uint8_t row = 7;
    static uint8_t cycle;
    static uint8_t phase;
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

    uint8_t pos = 7 - row;
    uint8_t field = (cycle << 1) | (pos >> 2);
    uint8_t segment = (pos & 3) * 2;
    uint8_t line = digit_line(field, segment, phase);
    uint8_t digit_bit = 0x80;

    *o2_device = 0;
    for (uint16_t bit = 0x8000; bit != 0; bit >>= 1) {
        uint8_t data = 0;
        if (mux1 & bit)
            data |= 0x20;
        if (mux2 & bit)
            data |= 0x80;
        if (line & digit_bit)
            data |= 0x10;
        *io1_device = data;
        digit_bit >>= 1;
        if (bit == 0x0100) {
            *o2_device = 0x10; /* ANZ bank 0: even segment line. */
            *o2_device = 0;
            line = digit_line(field, segment + 1, phase);
            digit_bit = 0x80;
        }
    }
    *o2_device = 0x10; /* ANZ bank 1: odd segment line. */
    *o2_device = 0xa0; /* Rising ENMUX1/ENMUX2 edges latch both boards. */

    if (row == 0)
        cycle ^= 1;
    row = (row - 1) & 7;
    if (++elapsed == STEP_MS) {
        elapsed = 0;
        if (++first_lamp == LAMP_COUNT)
            first_lamp = 0;
        if (++phase == 10)
            phase = 0;
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

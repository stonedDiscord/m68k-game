#include "audio.h"
#include "board.h"
#include "pt3player.h"

void write_ym2149(uint8_t reg, uint8_t value) {
    *audio_address = reg;
    *audio_data = value;
}

uint8_t read_ioa(void) {
    *audio_address = YM_IO_A;
    return *audio_address;
}

uint8_t read_iob(void) {
    *audio_address = YM_IO_B;
    return *audio_address;
}

void audio_update_pt3(void) {
    uint8_t regs[14];
    func_getregs(regs, 0);
#if AUDIO_VOLUME < 15
    /* R8-R10 bit 4 selects the hardware envelope, which ignores the
     * fixed level bits. Use a bounded fixed level for those channels:
     * this changes envelope timbre, but prevents full-volume notes.
     * Leave the player's register state intact. */
    for (int i = YM_REG_AMP_A; i <= YM_REG_AMP_C; i++) {
        uint8_t level = (regs[i] & 0x10) ? 15 : (regs[i] & 0x0f);
        regs[i] = (level * AUDIO_VOLUME) / 15;
    }
#endif
    // Write registers 0-12
    for (int i = 0; i < 13; i++) {
        write_ym2149(i, regs[i]);
    }
    // Only write register 13 if it's not 255
    // Writing to Reg 13 resets the envelope phase, so we only do it when a new shape is set.
    if (regs[13] != 255) {
        write_ym2149(13, regs[13]);
    }
}

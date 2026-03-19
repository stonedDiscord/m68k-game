#include "audio.h"
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

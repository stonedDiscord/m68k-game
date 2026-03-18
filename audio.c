#include "audio.h"

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

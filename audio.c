#include "audio.h"

void write_ym2149(uint8_t reg, uint8_t value) {
    *audio_address = reg;
    *audio_data = value;
}

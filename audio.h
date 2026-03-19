#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

// YM2149

static volatile uint8_t * const audio_address = (volatile uint8_t *)0x800141;
static volatile uint8_t * const audio_data    = (volatile uint8_t *)0x800143;

enum {
    YM_REG_TONE_A_FINE = 0,
    YM_REG_TONE_A_COARSE = 1,
    YM_REG_TONE_B_FINE = 2,
    YM_REG_TONE_B_COARSE = 3,
    YM_REG_TONE_C_FINE = 4,
    YM_REG_TONE_C_COARSE = 5,
    YM_REG_NOISE_PERIOD = 6,
    YM_REG_MIXER = 7,
    YM_REG_AMP_A = 8,
    YM_REG_AMP_B = 9,
    YM_REG_AMP_C = 10,
    YM_REG_ENV_FINE = 11,
    YM_REG_ENV_COARSE = 12,
    YM_REG_ENV_SHAPE = 13,
    YM_IO_A = 14,
    YM_IO_B = 15
};

void write_ym2149(uint8_t reg, uint8_t value);

uint8_t read_ioa(void);
uint8_t read_iob(void);

void audio_update_pt3(void);

#endif /* AUDIO_H */

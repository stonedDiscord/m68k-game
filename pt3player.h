#ifndef PT3PLAYER_H
#define PT3PLAYER_H

#include <stdint.h>

// PT3 player functions for m68k bare-metal

// Instantly stops music
void func_mute(void);

// Runs 1 tick of music (call at 50Hz)
void func_play_tick(int ch);

// Setup music for playing. length is ignored if it's in ROM.
int func_setup_music(const uint8_t* music_ptr, int length, int ch, int first);

// Restarts music
int func_restart_music(int ch);

// Retrieves the 14 AY registers for the given channel
void func_getregs(uint8_t *dest, int ch);

extern int forced_notetable;

#endif

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

static volatile uint8_t * const io1_device = (volatile uint8_t *)0x800101;
static volatile uint8_t * const  o2_device = (volatile uint8_t *)0x8000C1;

/* Convenience index constants */
#define INPUT_UP     1
#define INPUT_RIGHT  4
#define INPUT_DOWN   7
#define INPUT_LEFT   10

void scan_inputs(void);
uint16_t read_input(uint8_t n);

#endif /* INPUT_H */

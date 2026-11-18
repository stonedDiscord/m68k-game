#ifndef BOARD_H
#define BOARD_H

/* SuS CPU oscillator, selected with make CPU_MHZ=8 or CPU_MHZ=12.
 * The DUART has its own oscillator; do not scale it with the CPU clock. */
#ifndef CPU_MHZ
#define CPU_MHZ 8
#endif
#if CPU_MHZ != 8 && CPU_MHZ != 12
#error CPU_MHZ must be 8 or 12
#endif

#ifndef DUART_CLOCK_HZ
#define DUART_CLOCK_HZ 3686400UL
#endif

/* Music level: 0 = mute, 15 = original output. */
#ifndef AUDIO_VOLUME
#define AUDIO_VOLUME 6
#endif
#if AUDIO_VOLUME < 0 || AUDIO_VOLUME > 15
#error AUDIO_VOLUME must be between 0 and 15
#endif

// 68c681 outputs
#define OP0_LS_ALARM  0x01
#define OP1_ENSFI     0x02
#define OP2_LS_KNACK  0x04
#define OP3_SOD       0x08
#define OP4_SCL       0x10
#define OP5_LED1      0x20
#define OP6_SDA       0x40
#define OP7_nSpZ      0x80

#endif

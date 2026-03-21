#include "duart.h"

/* =========================================================================
 * ISR – placed at the vector address by the linker script (platform.ld).
 *
 * The ISR reads ISR to find which source fired, then handles each one.
 * Reading STOP_CNTR clears the counter/timer interrupt (ISR[3]).
 * Reading RBRA/RBRB clears the receiver-ready interrupt for that channel.
 * ========================================================================= */
void __attribute__((interrupt))
duartInterrupt(void)
{
    volatile char status = ISR;   /* Snapshot – reading ISR has no side effect */

    /* ---- Channel A receiver ready ---- */
    if (status & ISR_RxRDYA) {
        volatile char c = RBRA;   /* Reading the buffer clears RxRDYA in ISR  */
        (void)c;                  /* Replace with your receive handler         */
    }

    /* ---- Channel B receiver ready ---- */
    if (status & ISR_RxRDYB) {
        volatile char c = RBRB;
        (void)c;
    }

    /* ---- Channel A transmitter ready ---- */
    if (status & ISR_TxRDYA) {
        /*
         * The transmit-ready interrupt stays active as long as TBRA is empty.
         * Disable the interrupt here, or load the next character into TBRA.
         * Loading a character clears TxRDYA until the shift register accepts
         * it and the holding register becomes empty again.
         */
    }

    /* ---- Counter / timer ready ---- */
    if (status & ISR_CTRDY) {
        /*
         * In timer mode ISR[3] is set every second countdown cycle.
         * Reading STOP_CNTR clears ISR[3] but does NOT stop the timer.
         * In counter mode, reading STOP_CNTR clears ISR[3] AND stops
         * the counter.
         */
        volatile char dummy = STOP_CNTR;   /* Clear the C/T interrupt */
        (void)dummy;

        /* Put your periodic tick handler here */
    }

    /* ---- Input port change ---- */
    if (status & ISR_IPCNG) {
        volatile char ipcr = IPCR;   /* Reading IPCR clears ISR[7] */
        (void)ipcr;
    }
}

/* =========================================================================
 * setup_duart
 *
 * Initialises both UART channels to 9600 8N1 and configures the
 * counter/timer for a 10 ms periodic interrupt.
 *
 * Interrupt sources enabled by default:
 *   - Channel A receiver ready  (IMR bit 1)
 *   - Counter/timer ready       (IMR bit 3)
 *
 * Uncomment / add IMR bits to taste.
 * ========================================================================= */
void setup_duart(void)
{
    /* -----------------------------------------------------------------
     * Output port – all general purpose, LEDs on (OP5/OP6 = 0 in OPR
     * means the pins are driven HIGH which typically means LED off;
     * OPR_SET writes ones to turn the corresponding OPR bits on, which
     * drives the pins LOW – active-low LEDs).
     * OPCR = 0 means OP7-OP2 are all OPR-driven general-purpose outputs.
     * ----------------------------------------------------------------- */
    OPCR    = 0x00;
    OPR_SET = 0x60;     /* Assert OP6 and OP5 (LEDs) */

    /* -----------------------------------------------------------------
     * Interrupt vector – will be placed on the bus during IACK cycles.
     * 0x40 maps to MC68000 autovector level / user vector; adjust to
     * match your platform's interrupt table.
     * ----------------------------------------------------------------- */
    IVR = 0x40;

    /* -----------------------------------------------------------------
     * Auxiliary Control Register
     *   ACR[7]   = 0  → BRG set 1 (standard baud rates with 3.6864 MHz)
     *   ACR[6:4] = 111 → Timer mode, clock = X1 / 16
     *   ACR[3:0] = 0  → No IP change-of-state interrupts
     *
     * Must be set BEFORE loading CTUR/CTLR so the timer knows its source.
     * ----------------------------------------------------------------- */
    ACR = ACR_BRG_SET1 | ACR_CT_TIMER_X1D16;

    /* -----------------------------------------------------------------
     * Counter / Timer preload for 10 ms tick.
     *
     *   X1 = 3.6864 MHz  →  X1/16 = 230400 Hz
     *   period = 2 * preload / 230400
     *   For 10 ms: preload = 0.010 * 230400 / 2 = 1152 = 0x0480
     *
     * The timer fires ISR[3] every SECOND countdown (i.e. every full
     * square-wave cycle), so the actual interrupt period is:
     *   2 * preload / clock = 10 ms  (as calculated above)
     * ----------------------------------------------------------------- */
    ct_set_timer(CT_PRELOAD_MS(10));    /* 10 ms periodic interrupt */

    /* Start the timer BEFORE enabling channel clocks (datasheet recommendation) */
    ct_start();

    /* -----------------------------------------------------------------
     * Channel A – reset then configure
     * ----------------------------------------------------------------- */
    CRA = CR_RESET_MRP;     /* Reset mode-register pointer to MR1A */
    CRA = CR_RESET_RX;      /* Reset receiver                       */
    CRA = CR_RESET_TX;      /* Reset transmitter                    */

    /*
     * MR1A:
     *   bit 7   = 0  RxRTR disabled
     *   bit 6   = 0  RxIRQ selects RxRDY (not FFULL)
     *   bit 5   = 0  error mode = character
     *   bit[4:3]= 10 no parity
     *   bit[1:0]= 11 8 bits per character
     *   → 0x13
     */
    MR1A = 0x13;

    /*
     * MR2A:
     *   bit[7:6]= 00 normal mode
     *   bit 5   = 0  TxRTS control disabled
     *   bit 4   = 0  CTS control disabled
     *   bit[3:2]= 01 1 stop bit  (00 and 01 both = 1 stop bit per datasheet)
     *   bit[1:0]= xx unused
     *   → 0x07  (bits[3:2]=01, bits[1:0]=11 – the unused bits don't matter)
     *
     * Note: second write to MR1A address automatically goes to MR2A
     * because the internal pointer advances after the first access.
     */
    MR2A = 0x07;

    /* 9600 baud on both Rx and Tx, BRG set 1 */
    CSRA = CSR_9600;

    /* Enable both Tx and Rx */
    CRA = CR_TX_ENABLE | CR_RX_ENABLE;

    /* -----------------------------------------------------------------
     * Channel B – identical setup
     * ----------------------------------------------------------------- */
    CRB = CR_RESET_MRP;
    CRB = CR_RESET_RX;
    CRB = CR_RESET_TX;

    MR1B = 0x13;
    MR2B = 0x07;
    CSRB = CSR_9600;

    CRB = CR_TX_ENABLE | CR_RX_ENABLE;

    /* -----------------------------------------------------------------
     * Interrupt Mask Register – enable only the sources we want.
     *
     *   ISR_RxRDYA (bit 1) – wake up when channel A receives a byte
     *   ISR_CTRDY  (bit 3) – wake up on each timer tick
     *
     * Add ISR_RxRDYB, ISR_TxRDYA etc. as needed.
     * ----------------------------------------------------------------- */
    IMR = ISR_RxRDYA | ISR_CTRDY;
}

/* =========================================================================
 * Counter / Timer helpers
 * ========================================================================= */

/*
 * ct_set_timer – load a 16-bit preload value and (re)configure the ACR
 * for timer mode with X1/16 clock source.
 *
 * Call this before ct_start().  The timer will not recognise a new
 * preload until the next start command (or the next terminal count if
 * already running).
 */
void ct_set_timer(uint16_t preload)
{
    CTUR = CT_UPPER(preload);
    CTLR = CT_LOWER(preload);
}

/*
 * ct_start – issue the start-counter command.
 *
 * In timer mode: resets the timer to the preload value and begins a
 * fresh countdown.  The timer output is set to 1 (high).
 *
 * In counter mode: initialises to the preload value and begins counting
 * down.
 *
 * The start command is triggered by a READ from STRT_CNTR address; the
 * data returned is irrelevant.
 */
void ct_start(void)
{
    volatile char dummy = STRT_CNTR;
    (void)dummy;
}

/*
 * ct_stop – issue the stop-counter command.
 *
 * In timer mode: clears ISR[3] but does NOT stop the timer.
 * In counter mode: stops the countdown and clears ISR[3].
 *
 * This is also used inside the ISR to acknowledge a counter/timer
 * interrupt without stopping (timer mode).
 */
void ct_stop(void)
{
    volatile char dummy = STOP_CNTR;
    (void)dummy;
}

/*
 * ct_read – read the current 16-bit count value.
 *
 * The datasheet warns: read CUR and CLR only while the counter is
 * stopped (counter mode), because a borrow from CLR to CUR could
 * occur between the two reads if the counter is running.
 */
uint16_t ct_read(void)
{
    uint8_t hi = (uint8_t)CUR;
    uint8_t lo = (uint8_t)CLR;
    return (uint16_t)((hi << 8) | lo);
}

/* =========================================================================
 * Serial I/O – Channel A
 * ========================================================================= */

/*
 * getchar_ – non-blocking read from channel A.
 * Returns 0 if no character is waiting.
 */
char getchar_(void)
{
    if ((SRA & RxRDY) == 0)
        return 0;
    return RBRA;
}

/*
 * putchar_ – blocking write to channel A.
 * Waits until the transmit holding register is empty before loading
 * the new character; without this, characters written back-to-back
 * will be dropped.
 */
void putchar_(char c)
{
    while ((SRA & TxRDY) == 0)
        ;
    TBRA = c;
}

/* =========================================================================
 * Serial I/O – Channel B
 * ========================================================================= */

char getcharB_(void)
{
    if ((SRB & RxRDY) == 0)
        return 0;
    return RBRB;
}

void putcharB_(char c)
{
    while ((SRB & TxRDY) == 0)
        ;
    TBRB = c;
}

#include "duart.h"

// ISR - see platform.ld
void __attribute__((interrupt))
duartInterrupt(void) {
	char c;
    if (ISR & iRxRDY) {
        c = RBRA;	
    }
}

// duart MC68681
void setup_duart(void) {
	OPCR = 0x00;

	IMR  = 0x00;		// No interrupts enabled
	ACR  = 0x00;

	IVR  = 0x40;
	CTUR = 0x07;
	CTLR = 0x33;
	CRA  = 0x1a;
	CRA  = 0x2a;
	CRA  = 0x3a;
	CRB  = 0x1a;
	CRB  = 0x2a;
	CRB  = 0x3a;
	OPR_SET = 0x60; // LEDs on

	MR1A = 0x13;
	MR2A = 0x07;
	CSRA = 0xbb;
	CRA  = 0x1a;

	MR1B = 0x13;
	MR2B = 0x07;
	CSRB = 0xbb;
	CRB  = 0x1a;
}

char getchar_(void) {
	if ( (SRA & RxRDY) == 0 )
		return 0;
	else
		return RBRA;
}

void putchar_(char c) {
	while ( (SRA & TxRDY) == 0 );
	TBRA = c;
}
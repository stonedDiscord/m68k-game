#include "duart.h"

// ISR - see platform.ld
void __attribute__((interrupt))
duartInterrupt(void)
{
	char c;
	if (ISR & iRxRDY)
	{
		c = RBRA;
	}
}

// duart MC68681
void setup_duart(void)
{
	OPCR = 0x00;
	IMR = 0x00;
	ACR = 0x00; // BRG set 1
	IVR = 0x40;

	// Channel A: reset pointer, receiver, transmitter
	CRA = 0x10; // Reset MR pointer
	CRA = 0x20; // Reset receiver
	CRA = 0x30; // Reset transmitter

	MR1A = 0x13; // No parity, 8 bits
	MR2A = 0x07; // Normal mode, 1 stop bit
	CSRA = 0xbb; // 9600 baud Tx and Rx (set 1)
	CRA = 0x05;	 // Enable Tx and Rx

	// Channel B: same
	CRB = 0x10;
	CRB = 0x20;
	CRB = 0x30;

	MR1B = 0x13;
	MR2B = 0x07;
	CSRB = 0xbb;
	CRB = 0x05;

	OPR_SET = 0x60; // LEDs on
}

char getchar_(void)
{
	if ((SRA & RxRDY) == 0)
		return 0;
	else
		return RBRA;
}

void putchar_(char c)
{
	while ((SRA & TxRDY) == 0)
		;
	TBRA = c;
}
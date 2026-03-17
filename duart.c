#include "duart.h"

// ISR - see platform.ld
void __attribute__((interrupt))
duartInterrupt(void) {
	char c;    
    //DOUT = ISR;
    if (ISR & iRxRDY) {
        c = RBRA;
        circ_bbuf_push(c);		
    }

    if (ISR & iCTRDY) {
        // Read from STOP_CNTR port to clear interrupt flag
        c = STOP_CNTR;  
        counter++;
        if (counter == SECONDS) {
            seconds++;
            counter = 0;
        }
    }
}

// circular buffer
int circ_bbuf_push(char data) {
    short int cbuffer_next;

    cbuffer_next = cbuffer_head + 1;  // next is where head will point to after this write.
    if (cbuffer_next >= CBUFFER_LEN)
        cbuffer_next = 0;

    if (cbuffer_next == cbuffer_tail)  // if the head + 1 == tail, circular buffer is full
        return -1;

    cbuffer_data[cbuffer_head] = data;  // Load data and then move
    cbuffer_head = cbuffer_next;             // head to next data offset.
    return 0;  // return success to indicate successful push.
}

int circ_bbuf_pop(char *data) {
    short int cbuffer_next;

    if (cbuffer_head == cbuffer_tail)  // if the head == tail, we don't have any data
        return -1;

    cbuffer_next = cbuffer_tail + 1;  // next is where tail will point to after this read.
    if(cbuffer_next >= CBUFFER_LEN)
        cbuffer_next = 0;

    *data = cbuffer_data[cbuffer_tail];  // Read data and then move
    cbuffer_tail = cbuffer_next;              // tail to next offset.
    return 0;  // return success to indicate successful push.
}


// duart MC68681
void setup_duart(void) {
	CRA = 0x30; /* Reset port A transmitter */
	CRA = 0x20; /* Reset port A receiver */
	CRA = 0x10; /* Reset port A mode register pointer */
	//ACR = 0x00; /* Select Baud rate Set 1, no counter nor timer */
	ACR = 0xF0; /* Select Baud rate Set 1, counter X1/16 */
	CSRA = 0xBB; /* Set both the Rx and Tx speeds to 9600 baud */
	MR1A = 0x93; /* Port A 8 bits, no parity, 1 stop bit enable RxRTS output */
	//MR2A = 0x07; /* Basic operating mode */
    MR2A = 0x37; /* Normal operating mode, enable TxRTS, TxCTS, 1 stop bit */
    
    // Set up the timer for 300Hz
    // Crystal 3.6864MHz / 16 prescaler = 230400. 230400 / 300 = 768 ($300) (384? $180)
    CTUR = 0x01; /* Set Timer range value */
    CTLR = 0x80;
    
    // Place counter/timer output on OP3
    OPCR = 0x04; 

    IMR = 0x0a; /* Enable Interrupts: RxRDYA interrupt (bit 1), Timer interrupt (bit 3) */
    //IMR = 0x02; /* Enable Interrupts: RxRDYA interrupt (bit 1) */
    IVR = 0x50; /* set vector number 0x50 (80d) that is at address 0x140 (320d) (see platform.ld) */
	CRA = 0x05; /* Enable port A transmitter and receiver */
}

char getchar_(void) {
	while ( (SRA & RxRDY) == 0 );
	return RBRA;
}

void putchar_(char c) {
	while ( (SRA & TxRDY) == 0 );
	TBRA = c;
}
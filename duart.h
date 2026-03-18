#ifndef DUART_H
#define DUART_H

#include <stdint.h>

// MC68681 config
#define DUART_BASE 0x800181 				/* Base of I/O port addresses */
#define MR1A  (* (volatile char *) (DUART_BASE+0) ) 	/* Mode register 1A 		 (R/W)  */
#define MR2A  (* (volatile char *) (DUART_BASE+0) )     /* Mode register 2A 		 (R/W)  */

#define SRA   (* (volatile char *) (DUART_BASE+2) )     /* Status Register A 		 (R)    */
#define CSRA  (* (volatile char *) (DUART_BASE+2) )     /* Clock Select register A 	 (W)    */

                                                        /* Do Not Access */
#define CRA   (* (volatile char *) (DUART_BASE+4) )     /* Command Register A 		 (W)    */

#define RBRA  (* (volatile char *) (DUART_BASE+6) )     /* Receiver Buffer  A  	     (R)    */
#define TBRA  (* (volatile char *) (DUART_BASE+6) )     /* Transmiter Buffer A		 (W)    */

#define IPCR  (* (volatile char *) (DUART_BASE+8) )     /* Input Port Change Register(R)    */
#define ACR   (* (volatile char *) (DUART_BASE+8) )     /* Auxiliary Control Register(W)    */

#define ISR   (* (volatile char *) (DUART_BASE+10) )    /* Interrupt Status Register (R)    */
#define IMR   (* (volatile char *) (DUART_BASE+10) ) 	/* Interrupt Mask Register   (W)    */

#define CUR   (* (volatile char *) (DUART_BASE+12) ) 	/* Counter MSB               (R)    */
#define CTUR  (* (volatile char *) (DUART_BASE+12) ) 	/* Counter/Timer Upper Reg   (W)    */

#define CLR   (* (volatile char *) (DUART_BASE+14) ) 	/* Counter LSB               (R)    */
#define CTLR  (* (volatile char *) (DUART_BASE+14) ) 	/* Counter/Timer Lower Reg   (W)    */

#define MR1B  (* (volatile char *) (DUART_BASE+16) )    /* Mode Register 1B          (R/W)  */
#define MR2B  (* (volatile char *) (DUART_BASE+16) )    /* Mode Register 2B          (R/W)  */

#define SRB   (* (volatile char *) (DUART_BASE+18) )    /* Status Register B         (R)    */
#define CSRB  (* (volatile char *) (DUART_BASE+18) ) 	/* Clock Select Register B   (W)    */

                                                        /* Do Not Access */
#define CRB   (* (volatile char *) (DUART_BASE+20) ) 	/* Commands Register B       (W)    */

#define RBRB  (* (volatile char *) (DUART_BASE+22) )    /* Reciever Buffer B         (R)    */
#define TBRB  (* (volatile char *) (DUART_BASE+22) ) 	/* Transmitter Buffer B      (W)    */

#define IVR   (* (volatile char *) (DUART_BASE+24) )    /* Interrupt Vector Register (R/W)  */

#define IPUL  (* (volatile char *) (DUART_BASE+26) )    /* Unlatched Input Port values (R)  */
#define OPCR  (* (volatile char *) (DUART_BASE+26) )    /* Output Port Configuration Register (W)   */

#define STRT_CNTR (* (volatile char *) (DUART_BASE+28) )  /* Start-Counter Command (R)  */
#define OPR_SET   (* (volatile char *) (DUART_BASE+28) )  /* Output Port Bit Set Command (W)  */

#define STOP_CNTR (* (volatile char *) (DUART_BASE+30) )  /* Stop-Counter Command (R) / Clear timer interrupt  */
#define OPR_CLR   (* (volatile char *) (DUART_BASE+30) )  /* Output Port Bit Clear Command (W)  */

#define RxRDY 0x01	/* Receiver ready bit mask */
#define TxRDY 0x04	/* Transmiter ready bit mask */

#define iRxRDY 0x02	/* Receiver ISR RxRDY bit mask */
#define iCTRDY 0x08	/* Counter/ timer ISR ready bit mask */

// functions
void setup_duart(void);
char getchar_(void);
void putchar_(volatile char c);

#endif /* DUART_H */

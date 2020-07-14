//***************************************************************************
// Title			"ese_381_lab9"
// Date				05/05/2020	
// Version			1.0
// Author			Ramez Kaupak && Miguel Rivas
// DESCRIPTION:		The program initializes the MCU for SPI communication with
//					the BME680 for 20 bit wide temperature readings 
// Target MCU:		saml21j18b
// Target Hardware	SPI interface with BME 680	
// Restrictions		Must use 3.3V as Vin for BME680	
// Revision History	Initial version
//						
//**************************************************************************
/*
;
;*********************************************************************
;
;   *** Port Interface Definitions for BME680:
;
;  Ports              PA16  PA17  PA19   PB07  
;  Port alt names	  MOSI  SCK	  MISO  /SS   
;  BME680 pins		  SDI	SCK	  SDO	CSB
;             /SS = active low SPI select signal
;
;*********************************************************************
*/

#include "saml21j18b.h"
#include <stdint.h>

unsigned char* ARRAY_PORT_PINCFG0 = (unsigned char*)&REG_PORT_PINCFG0;
unsigned char* ARRAY_PORT_PMUX0 = (unsigned char*)&REG_PORT_PMUX0;
unsigned char* ARRAY_PORT_PINCFG1 = (unsigned char*)&REG_PORT_PINCFG1;

void init_spi_bme680 (void);			//initialize SERCOM for SPI with BME680
void user_delay_ms(uint32_t);			//user defined delay
uint8_t spi_read_bme680(uint8_t);		//returns contents of register address in param
void spi_write_bme680(uint8_t, uint8_t);	//writes in register address the next byte of param
uint8_t spi_transfer(uint8_t);			//send byte

uint8_t status;
uint8_t id;
uint32_t temperature_raw;

int main (void) {
	
	init_spi_bme680();				/* initialize SERCOM for SPI with BME680 */
	spi_write_bme680(0x60,0xB6);	/* software reset */
	status=spi_read_bme680(0x73);	/* read status register */
	id=spi_read_bme680(0x50);		/* read ID register, reads 0x61 if reset successful */
	spi_write_bme680(0x73,0x10);	/* switch to page 1 */
	status=spi_read_bme680(0x73);	/* read status to verify page change */
	
	while (1) {
		spi_write_bme680(0x74,0x21);		/* temp. oversampling x1, forced mode */
		/* calculating temperature_raw from shifting in MSB,LSB,XLSB to correct places */
		/* MSB = 0x22,		LSC = 0x23,		XLSB = 0x24 */
		temperature_raw = (spi_read_bme680(0x22) << 12) | (spi_read_bme680(0x23) << 4) 
								| (spi_read_bme680(0x24)>>4);	
	}
}

/*
;********************************
;NAME:			init_spi_bme680
;ASSUMES:		nothing
;RETURNS:		nothing
;CALLED BY:		main
;DESCRIPTION:	initialization function, sets up saml21j18b for SPI in SERCOM1
;				also sets up pin for /CS in the BME680. Finally, switch to page 0
;				in the status register of the BME680
;NOTE: Can be used as is with MCU clock speeds of 4MHz or less.
;*****************************
*/
void init_spi_bme680(void){
	REG_GCLK_PCHCTRL19 = 0x00000040;	/* SERCOM1 core clock not enabled by default */
	ARRAY_PORT_PINCFG0[16] |= 1;		/* allow pmux to set PA16 pin configuration */
	ARRAY_PORT_PINCFG0[17] |= 1;		/* allow pmux to set PA17 pin configuration */
	//ARRAY_PORT_PINCFG0[18] |= 1;		/* allow pmux to set PA18 pin configuration !!SS for LCD!!*/
	ARRAY_PORT_PINCFG0[19] |= 1;		/* allow pmux to set PA19 pin configuration */
	
	ARRAY_PORT_PMUX0[8] |= 0x22;     /* PA16 = MOSI, PA17 = SCK */
	ARRAY_PORT_PMUX0[9] |= 0x20;     /* PA19 = MISO */
	
	REG_PORT_DIRSET0 |= 0x40000;	/*set PA18 as /SS for LCD*/
	REG_PORT_OUTSET1 |= 0x40000;	/*PA18 hi initially*/
	REG_PORT_DIRSET1 |= 0x80;		/* Set PB07 as /SS for BME */
	REG_PORT_OUTSET1 |= 0x80;		/* PB07 hi initially */
	
	REG_SERCOM1_SPI_CTRLA = 1;              /* reset SERCOM1 */
	while (REG_SERCOM1_SPI_CTRLA & 1) {}    /* wait for reset to complete */
	REG_SERCOM1_SPI_CTRLA = 0x3030000C;     /* MISO-3, MOSI-0, SCK-1, SS-2, */
	/*SW controlled SS so ctrlB.MSSEN=0 so LCD can be used in next lab */
	REG_SERCOM1_SPI_CTRLB = 0x20000;		/* RXEN=1, MSSEN=0 (SW controlled), 8-bit */
	REG_SERCOM1_SPI_BAUD = 1;               /* SPI clock is = 1MHz */
	REG_SERCOM1_SPI_CTRLA |= 2;             /* enable SERCOM1 */
	while(!(REG_SERCOM1_SPI_INTFLAG & 1)){} //wait until Tx ready
		
	spi_write_bme680(0x73,0x00);			/* change to page 0 since it's at 1 on reset */
}

/* 
;********************************
;NAME:			user_delay_ms
;ASSUMES:		period of ms as parameter
;RETURNS:		nothing
;CALLED BY:		main
;DESCRIPTION:	User delay function, defines amount of ms in 32 bit int parameter
;NOTE: Can be used as is with MCU clock speeds of 4MHz or less.
;********************************
*/
void user_delay_ms(uint32_t period) {
	for (int i = 0; i < 307; i++)				//~1ms
	__asm("nop");
}

/*
;********************************
;NAME:			spi_transfer
;ASSUMES:		8 bit uint as input
;RETURNS:		8 bit uint in SPI data register
;CALLED BY:		main, spi_read_bme680, spi_write_bme680
;DESCRIPTION:	transfer function, sends a byte and checks flags for 
;				completion of task (Tx / Rx)
;NOTE: Can be used as is with MCU clock speeds of 4MHz or less.
;*****************************
*/
uint8_t spi_transfer(uint8_t data) {
	while(!(REG_SERCOM1_SPI_INTFLAG & 1)){}		//wait until Tx ready
	REG_SERCOM1_SPI_DATA = data;
	if (data & 0x80) {
		while(!(REG_SERCOM1_SPI_INTFLAG & 4)){}	//if reading (R/W=1), wait until Rx complete (RxC flag)
		}
	while(!(REG_SERCOM1_SPI_INTFLAG & 2)){}		//wait until Tx complete (TxC flag)
	return REG_SERCOM1_SPI_DATA;
}

/*
;********************************
;NAME:			spi_read_bme680
;ASSUMES:		8 bit register address as input
;RETURNS:		8 bit contents of register address
;CALLED BY:		main
;DESCRIPTION:	read function, sends a byte of address and uses
;				spi_transfer to read the contents
;NOTE: Can be used as is with MCU clock speeds of 4MHz or less.
;*****************************
*/
uint8_t spi_read_bme680(uint8_t reg_addr) {
	REG_PORT_OUTCLR1 |= 0x80;				//Bring CS low
	spi_transfer(reg_addr | 0x80);			//making it read byte
	uint8_t data = spi_transfer(0x80);		//dummy byte for reading
	REG_PORT_OUTSET1 |= 0x80;				//Bring CS high
	return data;
}

/*
;********************************
;NAME:			spi_write_bme680
;ASSUMES:		8 bit register address and 8 bit data as input
;RETURNS:		nothing
;CALLED BY:		main, init_spi_bme680
;DESCRIPTION:	write function, sends a byte of address and uses
;				spi_transfer to access the contents to overwrite
;NOTE: Can be used as is with MCU clock speeds of 4MHz or less.
;*****************************
*/
void spi_write_bme680(uint8_t reg_addr, uint8_t reg_data) {
	REG_PORT_OUTCLR1 |= 0x80;		//Bring CS low
	spi_transfer(reg_addr);			//define address to write
	spi_transfer(reg_data);			//write in reg_addr
	REG_PORT_OUTSET1 |= 0x80;		//Bring CS high
}





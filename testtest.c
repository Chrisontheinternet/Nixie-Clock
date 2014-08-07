/*
 * nixiefirmware_rev1.c
 *
 * Created: 6/11/2014 2:29:00 AM
 *  Author: Christopher Ladd
 *	
 *	Firmware for nixie clock rev1.0. 
 *		
 */ 



#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "C:/users/Chris/Documents/Atmel Studio/6.2/testtest/testtest/i2cmaster.h" // i2c master library by Peter Fleury
#include "../../../../../../../Program Files (x86)/Atmel/Atmel Toolchain/AVR8 GCC/Native/3.4.1056/avr8-gnu-toolchain/avr/include/avr/iom8.h"

#define F_CPU 16000000	// define operating freq to be 16MHz

#define LEDPIN  0x8			// bit masks for used pins
#define RCLKPIN  0x4
#define SRCLKPIN  0x2
#define SERPIN  0x1


int hours_register;						// global timekeeping variabeles, to be passed to the maketime function for formatting
int minutes_register;
unsigned int displaytime = 0x0000;		// initialize display time. Variable shifted out to shift registers.

int interrupt_hours = 0;				// Because atmega 8 architecture does not allow for use of i2c bus during ISRs, global variables are used here
int interrupt_minutes = 0;





// Shift register functions defined here ***********************
// *************************************************************

void latchpulse(){
	PORTC |= (RCLKPIN);		// Latch pin on
	_delay_ms(1);
	PORTC &= ~(RCLKPIN);	// Latch pin off
}

void srpulse(){
	PORTC &= ~(SRCLKPIN);	// SR clock pin off
	_delay_ms(1);
	PORTC |= (SRCLKPIN);	// SR clock pin on
	_delay_ms(1);
	PORTC &= ~(SRCLKPIN);	// SR clock pin off
}

void highbit(){
	PORTC |= (SERPIN);		// sets data pin high
	_delay_ms(1);
}

void lowbit(){				// sets data pin low
	PORTC &= ~(SERPIN);
	_delay_ms(1);
}

void shiftout(unsigned int shiftdata){
	PORTC &= ~(RCLKPIN);	// make sure latch pin is off
	
	for(int i = 0; i < 16; i++){	//For each of the 16 bits,
		if(0x8000 & shiftdata)		// check MSB
			highbit();				// if it's 1, call function to set data pin high
		else
			lowbit();				// if it's 0, call function to set data pin low
			
	srpulse();						// pulse SRCLK pin to shift data in shift register
		
	shiftdata = (shiftdata << 1);	// shift variable to inspect next bit
	}
	
	latchpulse();				// pulse output latch
}

// End shift register functions *********************************
// **************************************************************



unsigned int maketime(unsigned int temp_hours, unsigned int temp_minutes){		// this function takes in values for hours and minutes and produces a concatenated 16 bit value to be supplied to the shift register
	unsigned int goodtime = 0x0000;
	
	temp_hours &= ~(0xE0);						// mask config bits in hours byte
	
	// ********** comment out the following if statement to enable leading zero display *************
	if( (temp_hours & ~(0x0F)) == 0x00){
		temp_hours |= (0xF0);
	}
	
	goodtime = goodtime + (temp_hours << 8);	// shift hours over to most significant bit
	goodtime = goodtime + (temp_minutes);		// tack on minutes
	
	return goodtime;							// return 16 bit BCD time
}

int main(void)
{
	DDRC = 0xFF;				// PC0, PC1, PC2 set as output pins
	PORTC = 0x00;				// All output initially 0
	

	// INTERRUPT CONFIG SET HERE
	DDRD = 0x00;				// all pins on port D set to input
	PORTD = 0xFF;				// D.0 and D.1 pull up resistors set
	
	GICR |= (0xC0);				// Enables external interrupts 0 and 1
	MCUCR &= ~(0x0F);			// sets 4 LSBs on MCU control register to 0 to set low level triggering for interrupts
	
	sei();						// turn global interrupts on
    
	unsigned int tenhrs;
	unsigned int hrs;
	unsigned int tenmins;
	unsigned int mins;
	
	
	
	
	i2c_init();
		
	// SET RTC CONFIG
	i2c_start_wait(0xD0);			// Send START, send slave address 0xD0
	i2c_write(0x0E);			// Send address for RTC config register
	i2c_write(0x00);			// set bits in config register. OSC off, interrupts for alarms off
	i2c_stop();
	
	i2c_start_wait(0xD0);			// Send START, send slave address 0xD0
	i2c_write(0x02);			// Send address for RTC hour register
	i2c_write(0x71);			// set bits in hour register. 12 hour format, 11:xx pm
	i2c_stop();
	
	i2c_start_wait(0xD0);			// Send START, send slave address 0xD0
	i2c_write(0x01);			// Send address for RTC minutes register
	i2c_write(0x44);			// set bits in minutes register. xx:44 pm
	i2c_stop();	
	// END RTC CONFIG
	
	

	
	while(1)
    {
	_delay_ms(1);

	// RTC READ HOURS
	i2c_start_wait(0xD0);			// Send START, send slave address 0xD0
	i2c_write(0x01);			// Send address for RTC hour register
	i2c_rep_start(0xD1);		// Send repeated start
	minutes_register = i2c_readAck();		// read one byte, acknowledge
	hours_register = i2c_readNak();	// read one byte from next register
	i2c_stop();					// end operation
	// END RTC READ HOURS
	
	
	
	
	// INTERRUPT SERVICE ROUTINES:
	if(interrupt_minutes){

		tenhrs = (hours_register & ~(0xEF));	// mask all bits besides 10 hours place (1 bit).
		hrs = (hours_register & ~(0xF0));		// mask all bits besides hours bits
	
		tenmins = (minutes_register & ~(0x0F));	// mask all bits besides ten minutes
		mins = (minutes_register & ~(0xF0));		// mask all bits besides minutes		
			
		
		tenmins = (tenmins >> 4);				// shift over bits for easy calculation below
		tenhrs = (tenhrs >> 4);
		
		mins++;
		
		if(mins > 9){
			mins = 0;
			tenmins++;
		}
		
		if(tenmins > 5){
			tenmins = 0;
			hrs++;
		}
		
		if((hrs > 2) && (tenhrs == 1)){
			tenhrs = 0;
			hrs = 1;
		}
		
		if((hrs > 9) && (tenhrs == 0)){
			hrs = 0;
			tenhrs = 1;
		}
		
		
		minutes_register = ((tenmins << 4) + mins);
		hours_register = (((tenhrs << 4) + 0x60) + hrs);
		
		interrupt_minutes = 0;		// reset flag
		
		i2c_start_wait(0xD0);		// Send START, send slave address 0xD0
		i2c_write(0x01);			// Send address for RTC minutes register
		i2c_write(minutes_register);// set bits in minutes register. xx:33 pm
		i2c_stop();
		
		i2c_start_wait(0xD0);		// Send START, send slave address 0xD0
		i2c_write(0x02);			// Send address for RTC hour register
		i2c_write(hours_register);	// set bits in hour register.
		i2c_stop();
		
	}	
	
	if(interrupt_hours){			// if interrupt flag set, increment hours, reset flag, write value to RTC
	
		tenhrs = (hours_register & ~(0xEF));	// mask all bits besides 10 hours place (1 bit).
		hrs = (hours_register & ~(0xF0));		// mask all bits besides hours bits
		
		tenmins = (minutes_register & ~(0x0F));	// mask all bits besides ten minutes
		mins = (minutes_register & ~(0xF0));		// mask all bits besides minutes
	
		tenmins = (tenmins >> 4);	// shift over bits for easy calculation below
		tenhrs = (tenhrs >> 4);
			
		hrs++;
		
		if((hrs > 2) && (tenhrs == 1)){
			tenhrs = 0;
			hrs = 1;
		}
		
		if((hrs > 9) && (tenhrs == 0)){
			hrs = 0;
			tenhrs = 1;
		}
		
			
		minutes_register = ((tenmins << 4) + mins);			// reassemble full 8 bit packet for minutes register
		hours_register = (((tenhrs << 4) + 0x60) + hrs);	// reassemble full 8 bit packet for hours register. 0x60 are config bits



		interrupt_hours = 0;
		
		i2c_start_wait(0xD0);		// Send START, send slave address 0xD0
		i2c_write(0x02);			// Send address for RTC hour register
		i2c_write(hours_register);	// set bits in hour register.
		i2c_stop();
	}

	
	
	
	displaytime = maketime(hours_register, minutes_register);
	shiftout(displaytime);
	
	_delay_ms(20);
	}
}


/*************************************************************************************
 ******* INTERRUPT SERVICE ROUTINES DEFINED HERE ************************************/

ISR(INT0_vect){				// ISR for external interrupt 0
	
	cli();					// disable global interrupts
	_delay_ms(120);			// wait (bootleg software button debounce)
	interrupt_minutes = 1;	// set variable to let main loop know interrupt has occurred
	sei();					// re-enable global interrupts
}

ISR(INT1_vect){				// ISR for external interrupt 1

	cli();					// disable global interrupts
	_delay_ms(120);			// wait (bootleg software button debounce)
	interrupt_hours = 1;	// set variable to let main loop know interrupt has occurred
	sei();					// re-enable global interrupts
}
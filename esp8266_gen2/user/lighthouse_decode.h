#ifndef _LIGHTHOUSE_DECODE_H
#define _LIGHTHOUSE_DECODE_H

#include <stdint.h>

//FI2S = 40,000,000 Hz

#define MAX_LHBUFF 512
#define MAX_EDGES 512   //Can't be any more than 512 otherwise here would be a freq_denominator overflow.


extern struct LHSM_type LHSM;

struct LightEvent
{
	uint8_t gotlock;				//Is nonzero when data is good (
	uint8_t freq_denominator;
	uint16_t freq_numerator;

	uint16_t full_length;
	uint16_t strength;				//in total ticks the signal was asserted.

	uint32_t average_numerator;		//Compressed average.  Divide by strength to get real average.
	uint32_t firsttransition;		//In ticks

	uint8_t  t1mark;
	uint8_t  t2mark;
	uint8_t  t3mark;
	uint8_t  t4mark;
};



//XXX Tricky: On Xtensa, by keeping everything in one global structure,
// it is actually much faster because its location need take only one
// register and you can offset from that register quickly.  Individual
// global registers are slow! because they require a LR.
//  P.S. Be sure to make this word-aligned.
struct LHSM_type
{
	const unsigned short * carrierlut;  //32-bit boundary

	uint8_t defaultstate;
	uint8_t last_was_interesting;
	uint16_t statemachinestate;         //32-bit boundary

	uint32_t timebase; 					 //Current tick time (in FI2S)

	uint32_t * edgetimesbase;
	uint32_t * edgetimeshead;

	uint32_t edgecount; 

	uint32_t  debugmonitoring;
	uint32_t  debugbufferflag;           //32-bit boundary
	uint32_t debugbufferlen;			//Only updated at end of packet.
	uint32_t * debugbufferbase;
	uint32_t * debugbufferhead;

	struct LightEvent dhle;  //debug lighthouse event.

	uint8_t configure_state;

};


//For decidng "optical data" from the lighthouses. 
// In particular, modulated 1.8 MHz data as it comes in off
// of the I2S bus.

void lighthouse_decode( uint32_t * data, int size_words );
void lighthouse_setup(); //Call this first.
int SendPacket( struct LightEvent * data );

#endif

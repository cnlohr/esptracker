#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <lighthouse_decode.h>

int main()
{
	int rs = 0;
	lighthouse_setup();

	for( ; ; rs++  )
	{
		int i;
		int srlen = (rand()%100)+2;
		uint32_t buffer[srlen];
		int offset = rand()%32;
		int freq = rand()%20;
		int first_off = rand()%32;

		for( i = 0; i < srlen; i++ )
		{
			buffer[i] = rand();
		}
		buffer[i-1] = 0;
		buffer[0] = 0;
		buffer[1] &= (1<<offset)-1;
		printf( "%08x\n", buffer[1] );
		//printf( "%d / %d\n", rs, srlen );
		lighthouse_decode( buffer, srlen );
	}
}

int SendPacket( struct LightEvent * le )
{
/*
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
*/
	printf( "%d %d %d / %d %d / %d %d   %08x\n",
		le->gotlock, le->freq_denominator, le->freq_numerator, le->full_length, le->strength, le->average_numerator, le->firsttransition, LHSM.timebase );
}



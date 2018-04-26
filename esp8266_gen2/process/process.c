/*
	Data processor for raw pulse data.

	Run this program by piping in esp8266-formatted UDP messages.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint32_t Read32()
{
	uint32_t ret = 0;
	ret = getchar();
	ret |= getchar()<<8;
	ret |= getchar()<<16;
	int gc = getchar();
	if( gc == EOF ) { fprintf( stderr, "Broken pipe.\n" ); exit( -1 ); }
	ret |= gc<<24;
	return ret;
}

uint16_t Read16()
{
	uint32_t ret = 0;
	ret = getchar();
	int gc = getchar();
	if( gc == EOF ) { fprintf( stderr, "Broken pipe.\n" ); exit( -1 ); }
	ret |= gc<<8;
	return ret;
}

uint8_t Read8()
{
	int gc = getchar();
	if( gc == EOF ) { fprintf( stderr, "Broken pipe.\n" ); exit( -1 ); }
	return gc;
}

int main()
{
	while(1)
	{
		uint8_t sensor = Read8();
		uint8_t freq_denom = Read8();
		uint16_t freq_num  = Read16();
		uint16_t full_length = Read16();
		uint16_t strength = Read16();
		uint32_t average_numerator = Read32();
		uint32_t firsttransition = Read32();
		uint8_t  trans1 = Read8();
		uint8_t  trans2 = Read8();
		uint8_t  trans3 = Read8();
		uint8_t  trans4 = Read8();
		printf( "%d\t%10u\t%5u\t%10.2f\t%10.2f\t%5d\t%d\t%d\t%d\t%d\n",sensor, firsttransition, full_length, (average_numerator)*1.0/strength, 2.0 * 40000000.0 * freq_denom / freq_num,strength,trans1,trans2,trans3,trans4 );
	}
}

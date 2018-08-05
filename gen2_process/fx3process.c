#include <stdio.h>
#include <stdint.h>

unsigned long histogram[100];
int currun = 0;

int main()
{
	FILE * f = fopen( "Data_20180805155528.8.dat", "rb" );
//	FILE * f = fopen( "/tmp/newdatafile.dat", "rb" );
	uint8_t lv = fgetc( f );
	currun = 1;
	while( !feof( f ) )
	{
		uint8_t v = fgetc( f );
		if( lv != v )
		{
			if( currun >= 100 ) currun = 99;
			histogram[currun]++;
			currun = 1;
		}
		else
			currun++;
		lv = v;
	}

	int i;
	for( i = 0; i < 100; i++ )
	{
		printf( "%d\t%lu\n", i, histogram[i] );
	}
}


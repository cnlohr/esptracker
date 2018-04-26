#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

//XXX TODO: Write a more fully featured version that handles more data from previous state to help deglitch byte-edge cases.


//Run this function against every byte coming in.
//Goal: Keep this at 512x16b (1kB entries)
uint16_t clockerfn( uint16_t data )
{
	//Input:
	//
	//  0   0   0   0   0   0   0   B7  D0  D1  D2  D3  D4  D5  D6  D7 
	//Output:
	//  E2  E1 RUN RUN RUN RUN  0   D7     [EMIT 2]        [EMIT 1]

	int msbrunning = 0;
	int laststate = data >> 8 & 0x01;
	int emits[9];
	int emitplace = 0;

	int transitionlens[10];
	int transitionweights[10];
	int transitions = 0;
	int lockfirst = 0; //Potentially unused
	int weight = 0;

	int bitpl = 0;
	for( bitpl = 7; bitpl >= 0; bitpl-- )
	{
		int bit = data >> bitpl & 1;

		if( bit == laststate && bitpl == 7 )
		{
			weight++;
		}
		if( bit == laststate )
		{
			msbrunning++;
			weight++;
		}
		else
		{
			//We have a transition!
			transitionlens[transitions] = msbrunning;
			transitionweights[transitions] = weight;
			transitions++;
			msbrunning = 1;
			weight = 1;
		}
		laststate = bit;
	}

	//Careful: If we have line glitches, we may have to do tricky stuff to get it down to 2 emits.
	//Example would be something like:
	//  0 | 1 0 1 1 1 0 0 0
	//  0 | 0 1 0 0 0 1 1 1

	transitionlens[transitions] = msbrunning;
	transitionweights[transitions] = weight;
	
	while( transitions > 2 )
	{
		int i;
		int lowestweight = 100;
		int lowestplace = 0;

		//XXX TODO: There are a number of de-noising strategies.  This is nonoptimal.
		//
		//IN: 011010110 results in the first 11 growing to be the whole word.
		//
		//IN: 011100001 -- This is a really bad situation.   the first 3 ones disappear.

		//printf( "[%d]",transitionweights[0] );
		for( i = 1; i < transitions; i++ )
		{
			//printf( "(%d)",transitionweights[i] );
			if( transitionweights[i] < lowestweight )
			{
				lowestweight = transitionweights[i];
				lowestplace = i;
			}
		}
		//printf( "Merging @ %d\n", lowestplace );

		transitionlens[lowestplace-1] += transitionlens[lowestplace] + transitionlens[lowestplace+1];
		transitionweights[lowestplace-1] += transitionweights[lowestplace+2] + transitionweights[lowestplace+1];
		for( i = lowestplace; i < transitions; i++ )
		{
			transitionlens[i] = transitionlens[i+2];
			transitionweights[i] = transitionweights[i+2];
		}

		transitions-=2;
	}

	msbrunning = transitionlens[transitions];

	if( transitions < 2 ) 
	{
		transitionlens[1] = 0;
	}
	if( transitions < 1 )
	{
		transitionlens[0] = 0;
	}

	uint16_t ret = 0;
	ret |= data << 8 & 0x100;
	ret |= msbrunning << 10;
	ret |= (transitions > 0)?0x4000:0;
	ret |= (transitions > 1)?0x8000:0;
	ret |= (transitions > 0)?transitionlens[0]:0;
	ret |= (transitions > 1)?(transitionlens[1]<<4):0;

	if( transitionlens[0] + transitionlens[1] + msbrunning != 8 )
	{
		fprintf( stderr, "Significant algorithm fault. %d %d %d Does not add up to 8.\n",transitionlens[0], transitionlens[1],msbrunning );
		exit(1);
	}
	return ret;

}

int main()
{
	FILE * f = fopen( "data.txt", "rb" );
	if( !f ) { fprintf( stderr, "Error: Can't open data.txt\n" ); exit(1); }

	if( 1 )
	{
		int i;
		FILE * fh = fopen( "lighthouse_i2s_lut.c", "w" );
		fprintf( fh, "const unsigned short lighthouse_i2s_lut[512] = { " );
		for( i = 0; i < 512; i++ )
		{
			uint16_t out = clockerfn( i );
			if( (i & 0x0f) == 0 ) fprintf(  fh,"\n\t" );
			fprintf( fh, "0x%04x, ", out );
		}
		fprintf( fh, "\n};\n" );
		fclose( fh );
	}


	if( 0 )
	{
		int i;
		for( i = 0; i < 512; i++ )
		{
			int j;
			printf( "IN: " );
			for( j = 8; j >= 0; j-- )
			{
				printf( "%d", (i&(1<<j))?1:0 );
			}
			uint16_t out = clockerfn( i );
			printf( " // OUT: %04x ", out );
			printf( "((%d:%2d))", (out & 0x4000)?1:0, out & 0x0f);
			printf( "((%d:%2d))", (out & 0x8000)?1:0, out >> 4 & 0x0f ); 
			printf( "Running: %d BIT: %d", out >> 10 & 0x0f, out >> 8 & 0x01 );
			//if( (out & 0x0f) + (out >> 4 & 0x0f) + (out >> 10 & 0x0f) != 8 ) printf( "!!!!!!!!!!!\n" );
			printf( "\n" );

		}
	}

	int line = 1;
	while( 1 && !feof(f) )
	{
		printf( "LINE: %d\n", line );
		char stream[10000];
		int c, stl = 0;
		int i;
		do
		{
			c = fgetc(f);
			if( c == '\n' ) break;
			if( c == EOF ) break;
			if( c == '0' ) { stream[stl++] = 0; }
			if( c == '1' ) { stream[stl++] = 1; }
		}
		while(1);


		uint32_t transitions[1000];
		uint32_t transitionct = 0;

		//NOTE: If it was a 1 going into this, then set the 0x100 bit in state.

		{
			uint16_t state = 0;
			int dat = 0;
			uint32_t timer = 0;
			for( i = 0; i <= stl; )
			{
				int j;
		//		printf( " %d ", state >> 8 & 0x01  );
				dat = 0;
				for( j = 0; j < 8; j++ )
				{
					char c = stream[i++];
		//			printf( "%d", c );
					dat |= c?(1<<(7-j)):0;
				}
				if( i > stl ) dat = 0;


				state &= 0x100; //Suck in last bit from last time.
				state |= dat;

			//	printf( " = %04x", state );

				state = clockerfn( state );

			//	printf( " => %04x", state );

				if( state & 0x4000 )
				{
					//Emit 1.
					timer += (state & 0x0f);
			//		printf( "TRANSITION: %d ", timer );
					transitions[transitionct++] = timer;				
				}
				if( state & 0x8000 )
				{
					//Emit 1.
					timer += (state >> 4 & 0x0f);
			//		printf( "TRANSITION: %d ", timer );
					transitions[transitionct++] = timer;
				}

				timer += state >> 10 & 0x0f;

			//	printf( "\n" );
			}
		}

		if( transitionct & 1 )
		{
			fprintf( stderr, "Error: Got an unterminated series on line %d\n", line );
			exit( -1 );
		}

		if( transitionct < 12 ) continue;
		printf( "Transitions: %d\n", transitionct );
		//For each transition:
		// [0] = time of transition on
		// [1] = time of transition off
		uint32_t first_transition = transitions[0];
		uint32_t average = 0; //Average
		uint32_t stg = 0;     //Strength

		uint32_t center = 0;  //Units: Fs*2
		uint32_t centerlast = 0;
		//Also, calculate frequency.  How can we do this?

		//Bins for caluclating 
		#define FREQBINS 24
		uint32_t binsets[FREQBINS];
		uint32_t binqty[FREQBINS];
		int bestbin = 0;
		memset( binsets, 0, sizeof( binsets ) );
		memset( binqty, 0, sizeof( binqty ) );

		for( i = 0; i < transitionct; i+= 2 )
		{
			uint32_t transitioni = transitions[i];
			uint32_t len = transitions[i+1] -= transitioni;
			uint32_t tt = transitions[i]   = transitioni - first_transition; //Transition (as starting from virtual 0)
			printf( "@%d_%d ", transitions[i], transitions[i+1] );
			average += len * tt;
			stg += len;
			center = tt*2 + len;
			if( i )
			{
				int placing = center - centerlast;
				int interval = placing>>2;

				if( interval < FREQBINS )
				{
					binsets[interval] ++;
					if( binsets[interval] > binsets[bestbin] ) bestbin = interval;
					binqty[interval] += placing;
				}
			}
			centerlast = center;			
		}

		printf( "\n" );
		for( i = 0; i < FREQBINS; i++ )
		{
			printf( "%d / %d\n", binsets[i], binqty[i] );
		}

		int frequency = -1;
		if( bestbin >= 2 && bestbin < FREQBINS-2 )
		{
			uint32_t * bbminus2 = &binsets[bestbin-2];
			uint32_t totalset = bbminus2[0] + bbminus2[1] + bbminus2[2] + bbminus2[3] + bbminus2[4];
			bbminus2 = &binqty[bestbin-2];
			uint32_t totalqty = bbminus2[0] + bbminus2[1] + bbminus2[2] + bbminus2[3] + bbminus2[4];
			if( totalset > 8 )
				printf( "MidFreq:\t%f\t%d\t%d\n", 1.0 * 40000000.0 * 2.0 * totalset / totalqty, transitionct, totalset );
		}
		

		printf( "\nCentroid: %f / Power: %d\n", (average*1.0)/stg, stg );
		line++;
	}
}


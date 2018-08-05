#include <stdio.h>
#include <stdint.h>

unsigned long histogram[100];
int currun = 0;

#define IMAGE_WIDTH   113214
#define IMAGE_HEIGHT  422


double fline[IMAGE_WIDTH];
double fout[IMAGE_WIDTH];
FILE * ppm, * ppm2;
double ftotal;

void Emit( int oneorzero )
{
	static int x;
	static int y;
	if( oneorzero < 0 ) {
		while( x < IMAGE_WIDTH )
		{
			fputc( 128, ppm );
			x++;
		}
		printf( "LINE %d %f\n", y, ftotal );

		int i;
		for( i = 1; i < x; i++ )
		{
			int j;
			double corr = 0;
			corr = 0;
			for( j = 0; j < x; j++ )
			{
				int a = j;
				int b = j % i;
				corr += fline[a] * fline[b];
			}
			printf( "%d\t%f\n", i, corr );
		}
/*
		fftw_plan p;
		p = fftw_create_plan(IMAGE_WIDTH, FFTW_FORWARD, FFTW_ESTIMATE);
		rfftw_one(p, fline, fout);
		fftw_destroy_plan(p);
*/
		for( i = 0; i < IMAGE_WIDTH; i++ )
		{
			fputc( fout[i], ppm2 );
		}

		x = 0;
		y++;

	};
	if( y < IMAGE_HEIGHT && x < IMAGE_WIDTH )
	{
		ftotal += fline[x] = oneorzero?1.0:-1.0;

		fputc( oneorzero?255:0, ppm );
	}
		if( x > IMAGE_WIDTH ) printf( "%d\n", x );

	x++;
//	printf( "%d", oneorzero );
}


//State-machine of sorts for incoming data.
void HandleTransition( int len_of_run )
{
	static int place_in_line = 0;
	static int was_short = 0;
	static int runlen = 0;
	static int was_long_wait = 0;
	if( len_of_run < 3 )
	{
		runlen += len_of_run;
		return;
	}
	else
		runlen = len_of_run;

	if (runlen>=5 && runlen <= 10)
	{
		place_in_line++;
		//Is Short
		if( was_short )
		{
			Emit( 1 );
			was_short = 0;
		}
		else
		{
			was_short = 1;
		}
	}
	else if( runlen >= 13 && runlen <= 19 )
	{
		place_in_line++;
		if( was_short )
		{
			if( !was_long_wait )
			{
				fprintf( stderr, "Error: Tracking problem, asymmetric shorts\n" );
			}
			was_short = 0;
		}
		else
		{
			Emit( 0 );
		}
		was_long_wait = 0;
	}
	else if( runlen > 98 )
	{
		was_long_wait = 1;
		was_short = 0;
		place_in_line = 0;
		Emit( -1 );
	}
 	else
	{
		if( !was_long_wait ) fprintf( stderr, "Bad or corrupt data event. %d %d %d\n", runlen, was_long_wait,  place_in_line );
		was_long_wait = 0;
	}
}


int main()
{
	FILE * f = fopen( "_CHAN_AFTER_BUTTON_Data_20180805170303.8.dat", "rb" );
	ppm = fopen( "_CHAN_AFTER_BUTTON_.ppm", "wb" );
	ppm2 = fopen( "FFTW.ppm", "wb" );
	fprintf( ppm, "P5\n#Is a comment required\n%d %d\n255\n", IMAGE_WIDTH, IMAGE_HEIGHT );
	fprintf( ppm2, "P5\n#Is a comment required\n%d %d\n255\n", IMAGE_WIDTH, IMAGE_HEIGHT );
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
			HandleTransition( currun );
			currun = 1;
		}
		else
			currun++;
		lv = v;
	}

}


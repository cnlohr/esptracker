#include "lighthouse_decode.h"
#include <esp82xxutil.h>

//From lighthouse_i2s_lut.c
extern const unsigned short lighthouse_i2s_lut[512];
uint32_t lighthouse_debugbuffer[MAX_LHBUFF]; 
uint32_t edgeinfo[MAX_EDGES];

struct LHSM_type LHSM;

void ResetStateMachine()
{
	LHSM.statemachinestate = LHSM.defaultstate;
	LHSM.debugbufferhead = LHSM.debugbufferbase;
	LHSM.edgetimeshead = LHSM.edgetimesbase;
}

void lighthouse_setup()
{
	LHSM.carrierlut = lighthouse_i2s_lut;
	LHSM.debugbufferbase = lighthouse_debugbuffer;
	LHSM.edgetimesbase = edgeinfo;
	LHSM.configure_state = 0;

	ResetStateMachine();
}


#define buffer_size (128*36)
static uint32_t buffer_data[buffer_size];
static int      buffer_head = 0;
static int      buffer_tail = 0;
static int      buffer_overflows = 0;
void lighthouse_decode( uint32_t *  data, int size_words )
{
	LHSM.input_events++;
	int remaining_size = buffer_size - buffer_head;
	if( size_words < remaining_size )
	{
		ets_memcpy( buffer_data + buffer_head, data, size_words * 4 );
		buffer_head += size_words;
	}
	else
	{
		ets_memcpy( buffer_data + buffer_head, data, remaining_size * 4 );
		size_words -= remaining_size;
		ets_memcpy( buffer_data, data, size_words * 4 );
		buffer_head = size_words;
	}
	if( buffer_head == buffer_tail ) buffer_overflows++;
}


//"Forced inline." :-p
//This function may look odd.  That's because it is.
//To see what's really going on in here, check lighthouse_clocking.c
#define ProcessMachineState( dat ) \
	{ \
		localstate = (localstate & 0x100) | dat; \
		localstate = LHSM.carrierlut[localstate]; \
		if( localstate & 0x4000 ) { \
			LHSM.timebase += localstate & 0x0f; \
			*(LHSM.edgetimeshead++) = LHSM.timebase; \
		} \
		if( localstate & 0x8000 ) { \
			LHSM.timebase += localstate >> 4 & 0x0f; \
			*(LHSM.edgetimeshead++) = LHSM.timebase; \
		} \
		LHSM.timebase += localstate >> 10 & 0x0f; \
	}

int ProcessLighthouse()
{
	uint32_t * data;
	int size_words;


	int remain = (buffer_head - buffer_tail + buffer_size) % buffer_size;
	int remain_to_end = buffer_size - buffer_head;
	//printf( "%d", remain/128 );

	if( remain > remain_to_end )
		size_words = remain_to_end;
	else
		size_words = remain;

	data = buffer_data + buffer_tail;

	if( LHSM.debugbufferflag == 2 )
	{
		buffer_tail = (buffer_tail + size_words) % buffer_size;
		return 0;
	}

	int i;

	//Check to make sure we won't overflow our buffers.  Doing this here speeds us up,
	//rather than having to do it every iteration.
	uint32_t used_buffer = LHSM.debugbufferhead - LHSM.debugbufferbase; //Because of magic C pointer math, this returns elements.
	if( used_buffer >= MAX_LHBUFF - size_words )
	{
		LHSM.debugbufferflag = 3; //3 = abort. (oversized frame)
	}

	for( i = 0; i < size_words; i++ )
	{
		uint32_t r = data[i];
		uint32_t is_interesting = r != 0x00000000 && r != 0xffffffff;
		if( is_interesting | LHSM.last_was_interesting )
		{
			LHSM.last_was_interesting = is_interesting;
			if( LHSM.debugbufferflag == 0 )
			{
				ResetStateMachine();
				LHSM.debugbufferflag = 1;
				LHSM.debugbufferlen = 1;
				if( !LHSM.configure_state ) LHSM.configure_state = 1;
			}

			if( LHSM.debugbufferflag == 1 )
			{
				*(LHSM.debugbufferhead++) = r;

				uint32_t used_edges = LHSM.edgetimeshead - LHSM.edgetimesbase;

				//Check here instead of in each PSM. (performance speedup)
				if( used_edges >= MAX_EDGES - 9 ) //-9 = each PSM can initiate up to 2 edges.
				{
					LHSM.debugbufferflag = 3;  //Abort: Too many transitions.
					LHSM.timebase += 32;
					continue;
				}

				uint16_t localstate = LHSM.statemachinestate;

				//Actually process the data.
				ProcessMachineState( (r>>24&0xff) );
				ProcessMachineState( (r>>16&0xff) );
				ProcessMachineState( (r>>8&0xff) );
				ProcessMachineState( (r>>0&0xff) );

				LHSM.statemachinestate = localstate;
			}
			else
			{
				LHSM.timebase += 32;
			}
		}
		else
		{	
			if( LHSM.debugbufferflag == 3 )
			{
				LHSM.debugbufferflag = 0;
			}

			if( LHSM.debugbufferflag == 1 )
			{
				//This may look odd, but what's going on 
				//is we find out what the default value is
				//for the line, i.e. bias low or high.
				//Then we can use that for the defualt state.
				LHSM.defaultstate = (r & 0x100)>>8;
				LHSM.debugbufferlen = LHSM.debugbufferhead - LHSM.debugbufferbase;

				if( LHSM.debugbufferlen < 5 )
				{
					LHSM.timebase += 32;
					LHSM.debugbufferflag = 0;
				}
				else
				{
					//Continue the state machine.
					uint16_t localstate = LHSM.statemachinestate;
					ProcessMachineState( r>>24 );	//We don't care about storing the output.  It just needs to finish.
					LHSM.timebase += 24;


					//All edges are now populated.  We could operate on them.
					int transitionct = LHSM.edgetimeshead - LHSM.edgetimesbase;
					uint32_t * edgept = LHSM.edgetimesbase;
					LHSM.edgecount = transitionct;

					struct LightEvent * le = &LHSM.dhle;
					le->gotlock = 0;

					

					//Once we're here, we have properly formatted edges and everything!
					{
						int i;
						uint32_t first_transition = edgept[0];
						uint32_t average = 0; //Average
						uint32_t stg = 0;     //Strength

						uint32_t center = 0;  //Units: Fs*2
						uint32_t centerlast = 0;
						uint16_t ending;
						//Also, calculate frequency.  How can we do this?

						//Bins for caluclating 
						#define FREQBINS 24
						uint32_t binsets[FREQBINS];
						uint32_t binqty[FREQBINS];
						int bestbin = 0;
						ets_memset( binsets, 0, sizeof( binsets ) );
						ets_memset( binqty, 0, sizeof( binqty ) );

						
						uint32_t transitioni = edgept[0];
						le->t1mark = edgept[1] - transitioni;
						le->t2mark = edgept[2] - transitioni;
						le->t3mark = edgept[3] - transitioni;
						le->t4mark = edgept[4] - transitioni;

						for( i = 0; i < transitionct; i+= 2 )
						{
							transitioni = edgept[0];
							uint32_t len = edgept[1] -= transitioni;
							uint32_t tt  = edgept[0]  = transitioni - first_transition; //Transition (as starting from virtual 0)
							ending = len + tt;
							edgept+=2;

							average += len * tt;
							stg += len;
							center = tt*2 + len;
							if( i )
							{
								int placing = center - centerlast;
								int interval = placing>>2;

								if( interval < FREQBINS && interval > 0 )
								{
									binsets[interval] ++;
									if( binsets[interval] > binsets[bestbin] ) bestbin = interval;
									binqty[interval] += placing;
								}
							}
							centerlast = center;
						}


						int frequency = -1;
						if( bestbin >= 2 && bestbin < FREQBINS-2 )
						{
							uint32_t * bbminus2 = &binsets[bestbin-2];
							uint32_t totalset = bbminus2[0] + bbminus2[1] + bbminus2[2] + bbminus2[3] + bbminus2[4];
							bbminus2 = &binqty[bestbin-2];
							uint32_t totalqty = bbminus2[0] + bbminus2[1] + bbminus2[2] + bbminus2[3] + bbminus2[4];
							if( totalset > 4 ) //Make sure we don't have a runt frame.
							{
								static int stset = 0;
								//WE HAVE A LOCK on a frequency and good data. Time to populate!
								le->gotlock = 1 + stset;
								if( ++stset == 16 ) stset = 0;
								le->firsttransition = first_transition;
								le->average_numerator = average;
								le->strength = stg; 
								le->full_length = ending;
								le->freq_numerator = totalqty;
								le->freq_denominator = totalset;
							}
						}
					}

					if( le->gotlock )
					{
						SendPacket( le );
					}

					if( !LHSM.debugmonitoring )
					{
						LHSM.debugbufferflag = 0;
					}
					else
					{
						LHSM.debugbufferflag = 2;
					}
				}
			}
		}
	}

	if( buffer_overflows )
	{
		printf( "*\n" );
		buffer_overflows = 0;
	}

	buffer_tail = (buffer_tail + size_words) % buffer_size;
	if( size_words )
		return 1;
	else
		return 0;
}


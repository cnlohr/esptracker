#include <stdio.h>
#include <stdint.h> 


// .CSV range in seconds
#define RANGE_LOWER 0.0F
#define RANGE_UPPER 0.02F



#define DATASIZE 116523008
#define DATA 0
#define FREQ 100000000UL
#define MODULATION_FREQ 6000000UL
#define SNR 5.0F

#define DATALENGTH DATASIZE
#define MODULATION_MIN (0.5F / (float)MODULATION_FREQ)
#define MODULATION_THRESHOLD (1.5F * MODULATION_MIN)


uint8_t buffer[DATALENGTH];

int32_t prevEdge;
uint8_t currValue;
float confidence;
int32_t prevValueTick;
uint8_t edge;


uint8_t Edge(uint8_t *buffer, int32_t index, int bitIndex, int8_t direction);
uint8_t Debounce(uint16_t *buffer, int32_t index, int bitIndex);
uint8_t NoiseFilter(float signal, float prevSignal, uint8_t prevValue, float snr);


int main()
{
	FILE *f = fopen("Data_20180805155528.8.dat", "rb");
	fread(buffer, DATALENGTH, 1, f);
	fclose(f);

	f = fopen("data3.csv", "w+");

	for(int32_t i = RANGE_LOWER * (float)FREQ + 1; i < RANGE_UPPER * (float)FREQ; i++){
		if(Edge(buffer, i, DATA, -1)){			// Edge detection (between the previous and current buffer readings)
			//if(!Debounce(buffer, i, DATA)){		// If this edge is not a bounce:

				float time = (float)i / (float)FREQ;

				// Replot all of the previous frame's data at the new time so we get nice visual square waves
				fprintf(f, "%.8f, %.1f, %.8f, %.2f, %.1f\n",
					time,											// X1 - Time
					(float)((buffer[i-1] & (1 << DATA)) >> DATA),	// Y1 - Raw data
					time - 1.0F / (float)MODULATION_FREQ,			// X2 - Plot the following two points a modulating tick behind (better visual lineup)
					3.0F + confidence,								// Y2 - Confidence score for demodulated data
					1.5F + (float)currValue );						// Y3 - Demodulated data based on confidence score and noise filter

				// Has it been "long enough" since the last value was captured, according to the modulation frequency?
				// ie. are we expecting this edge was a modulation tick?
				float prevValueTime = (float)prevValueTick / (float)FREQ;
				if(time - prevValueTime > MODULATION_THRESHOLD){

					uint8_t prevValue = currValue;
					float prevConfidence = confidence;

					confidence = ((float)(edge + 1) / (time - prevValueTime)) / MODULATION_FREQ - 1.0F;
					currValue = NoiseFilter(confidence, prevConfidence, currValue, SNR);

					prevValueTick = i;
					edge = 0;
				} else {
					edge++;
				}

				// Output current data
				fprintf(f, "%.8f, %.1f, %.8f, %.2f, %.1f\n",
					time,
					(float)((buffer[i] & (1 << DATA)) >> DATA),
					time - 1.0F / (float)MODULATION_FREQ,
					3.0F + confidence,
					1.5F + (float)currValue );

				prevEdge = i;
			//}
		}
	}

	fclose(f);
	return 0;
}


uint8_t Edge(uint8_t *buffer, int32_t index, int bitIndex, int8_t direction){
	uint8_t edge = 0;
	if((buffer[index] & (1 << bitIndex))
	!= (buffer[index + direction] & (1 << bitIndex))){
		edge = 1;
	}
	return edge;
}


uint8_t Debounce(uint16_t *buffer, int32_t index, int bitIndex){
	uint8_t debounce = 0;
	if((buffer[index - 1] & (1 << bitIndex))		// Identify bounces as a buffer reading with an edge on either side (ie. a spike)
	== (buffer[index + 1] & (1 << bitIndex))){
		buffer[index] ^= (1 << bitIndex);			// Filter the bounce out of the dataset
		debounce = 1;
	}
	return debounce;
}


float Abs(float value){
	return value < 0 ? -value : value;
}


uint8_t NoiseFilter(float signal, float prevSignal, uint8_t prevValue, float snr){
	if(signal < 1.0F / snr
	|| signal > 1.0F - 1.0F / snr){						// If the confidence score is outside the central noise range:
		return signal > 0.5F ? 1 : 0;					//   Converge the value to 1 or 0
	}else
	if(Abs(signal - prevSignal) < 1.0F / snr){			// If the difference between the previous and current signal is small enough:
		return prevValue;								//   Return the same value as before
	}else{												// Otherwise:
		return signal > prevSignal ? 1 : 0;				//   Extend the value in the direction it's pulling towards
	}
}

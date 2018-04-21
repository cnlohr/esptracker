#include "ts4231.h"
#include <uart.h>

void ICACHE_FLASH_ATTR ConfigureTS4231(  )
{
	printf( "Device configuration response: %d\n", TS4231configDevice( 0x392B ) );
}

//		PIN_DIR_OUTPUT = _BV(4) |  _BV(5);
		
/*
#define PIN_OUT       ( *((uint32_t*)0x60000300) )
#define PIN_OUT_SET   ( *((uint32_t*)0x60000304) )
#define PIN_OUT_CLEAR ( *((uint32_t*)0x60000308) )
#define PIN_DIR       ( *((uint32_t*)0x6000030C) )
#define PIN_DIR_OUTPUT ( *((uint32_t*)0x60000310) )
#define PIN_DIR_INPUT ( *((uint32_t*)0x60000314) )
#define PIN_IN        ( *((volatile uint32_t*)0x60000318) )
#define _BV(x) ((1)<<(x))
*/



/*
Below code is licensed 

MIT License

Copyright (c) 2018 TriadSemi

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#define BUS_DRV_DLY     1       //delay in microseconds between bus level changes
#define BUS_CHECK_DLY   500     //delay in microseconds for the checkBus() function
#define SLEEP_RECOVERY  100     //delay in microseconds for analog wake-up after exiting SLEEP mode
#define UNKNOWN_STATE   0x04    //checkBus() function state
#define S3_STATE        0x03    //checkBus() function state
#define WATCH_STATE     0x02    //checkBus() function state
#define SLEEP_STATE     0x01    //checkBus() function state
#define S0_STATE        0x00    //checkBus() function state
#define BUS_FAIL        0x01    //configDevice() function status return value
#define VERIFY_FAIL     0x02    //configDevice() function status return value
#define WATCH_FAIL      0x03    //configDevice() function status return value
#define CONFIG_PASS     0x04    //configDevice() function status return value


/*******************************************************************
    Copyright (C) 2017 Triad Semiconductor

    ts4231.h - Library for configuring the Triad Semiconductor TS4231 Light
               to Digital converter.
    Created by: John Seibel
*******************************************************************/

void ICACHE_FLASH_ATTR TS4231Setup();

//IMPORTANT NOTE: If porting the TS4231 library code to a non-Arduino architecture,
//be sure that the INPUT ports assigned to the E and D signals are configured as
//floating inputs with NO pull-up or pull-down function.  Using a pull-up or
//pull-down function on the inputs will cause the TS4231 to operate incorrectly.

#define D_pin _BV(4)
#define E_pin _BV(5)
#define HIGH 1
#define LOW  0
#define ts_delayUs(x) ets_delay_us( x )
#define INPUT 0
#define OUTPUT 1

static uint8_t configured;
static uint32_t us_ovflow = 0;

#define ts_pinMode( pin, mode ) \
	{ \
		if( mode == INPUT ) \
			PIN_DIR_INPUT = pin; \
		else \
			PIN_DIR_OUTPUT = pin; \
	}

#define ts_digitalRead( pin )	((PIN_IN & (pin))?1:0)
#define ts_digitalWrite( pin, write_val) { if( write_val ) { PIN_OUT_SET = pin; } else { PIN_OUT_CLEAR = pin; } }

//checkBus() performs a voting function where the bus is sampled 3 times
//to find 2 identical results.  This is necessary since light detection is
//asynchronous and can indicate a false state.
static uint8_t ICACHE_FLASH_ATTR checkBus(void) {
  uint8_t state;
  uint8_t E_state;
  uint8_t D_state;
  uint8_t S0_count = 0;
  uint8_t SLEEP_count = 0;
  uint8_t WATCH_count = 0;
  uint8_t S3_count = 0;
  uint8_t i;

  for (i=0; i<3; i++) {
    E_state = ts_digitalRead(E_pin);
    D_state = ts_digitalRead(D_pin);
    if (D_state == HIGH) {
      if (E_state == HIGH) S3_count++;
      else SLEEP_count++;
      }
    else {
      if (E_state == HIGH) WATCH_count++;
      else S0_count++;
      }
    ts_delayUs(BUS_CHECK_DLY);
    }
  if (SLEEP_count >= 2) state = SLEEP_STATE;
  else if (WATCH_count >= 2) state = WATCH_STATE;
  else if (S3_count >= 2) state = S3_STATE;
  else if (S0_count >= 2) state = S0_STATE;
  else state = UNKNOWN_STATE;
  
  return state;
  }



void ICACHE_FLASH_ATTR TS4231Setup()
{
	configured = false;
	ts_pinMode(E_pin, INPUT);
	ts_pinMode(D_pin, INPUT);
}

//Function waitForLight() should be executed after power-up and prior to
//configuring the device.  Upon power-up, D is a 0 and will output a 1
//when light is detected.  D will return to 0 at the end of light detection.
//This funciton looks for the falling edge of D to indicate that the end of
//light detection has occurred.
#if 0

//Different than ts_millis
#define ts_microsis() system_get_time()


bool ICACHE_FLASH_ATTR TS4231waitForLight(uint16_t light_timeout) {
  bool light = false;
  bool exit = false;
  unsigned long time0;

  light_timeout *= 1000;

  if (checkBus() == S0_STATE) {
    time0 = ts_microsis();
    while (exit == false) {
      if (ts_digitalRead(D_pin) > 0) {
        while (exit == false) {
          if (ts_digitalRead(D_pin) == 0) {
            exit = true;
            light = true;
            }
          else if (ts_microsis() > (time0 + light_timeout)) {
            exit = true;
            light = false;
            }
          else {
            exit = false;
            light = false;
            }
          }
        }
      else if (ts_microsis() > (time0 + light_timeout)) {
        exit = true;
        light = false;
        }
      else {
        exit = false;
        light = false;
        }
      }
    }
  else light = true;  //if not in state S0_state, light has already been detected
  return light;
  }


bool ICACHE_FLASH_ATTR TS4231goToSleep(void) {
  bool sleep_success;
  
  if (configured == false)  sleep_success = false;
  else {
    switch (checkBus()) {
      case S0_STATE:
        sleep_success = false;
        break;
      case SLEEP_STATE:
        sleep_success = true;
        break;
      case WATCH_STATE:
        ts_digitalWrite(E_pin, LOW);
        ts_pinMode(E_pin, OUTPUT);
        ts_delayUs(BUS_DRV_DLY);
        ts_pinMode(E_pin, INPUT);
        ts_delayUs(BUS_DRV_DLY);
        if (checkBus() == SLEEP_STATE) sleep_success = true;
        else sleep_success = false;
        break;
      case S3_STATE:
        sleep_success = false;
        break;
      default:
        sleep_success = false;
        break;
      }
    }
  return sleep_success;
  }
#endif


static void ICACHE_FLASH_ATTR writeConfig(uint16_t config_val) {
  uint8_t i;
  ts_digitalWrite(E_pin, HIGH);
  ts_digitalWrite(D_pin, HIGH);
  ts_pinMode(E_pin, OUTPUT);
  ts_pinMode(D_pin, OUTPUT);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(D_pin, LOW);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(E_pin, LOW);
  ts_delayUs(BUS_DRV_DLY);
  for (i = 0; i < 15; i++) {
    config_val = config_val << 1;
    if ((config_val & 0x8000) > 0) { ts_digitalWrite(D_pin, HIGH); }
    else { ts_digitalWrite(D_pin, LOW); }
    ts_delayUs(BUS_DRV_DLY);
    ts_digitalWrite(E_pin, HIGH);
    ts_delayUs(BUS_DRV_DLY);
    ts_digitalWrite(E_pin, LOW);
    ts_delayUs(BUS_DRV_DLY);
    }
  ts_digitalWrite(D_pin, LOW);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(E_pin, HIGH);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(D_pin, HIGH);
  ts_delayUs(BUS_DRV_DLY);
  ts_pinMode(E_pin, INPUT);
  ts_pinMode(D_pin, INPUT);
  }

static uint16_t ICACHE_FLASH_ATTR readConfig(void) {
  uint16_t readback;
  uint8_t i;

  readback = 0x0000;
  ts_digitalWrite(E_pin, HIGH);
  ts_digitalWrite(D_pin, HIGH);
  ts_pinMode(E_pin, OUTPUT);
  ts_pinMode(D_pin, OUTPUT);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(D_pin, LOW);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(E_pin, LOW);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(D_pin, HIGH);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(E_pin, HIGH);
  ts_delayUs(BUS_DRV_DLY);
  ts_pinMode(D_pin, INPUT);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(E_pin, LOW);
  ts_delayUs(BUS_DRV_DLY);
  for (i = 0; i < 14; i++) {
    ts_digitalWrite(E_pin, HIGH);
    ts_delayUs(BUS_DRV_DLY);
    readback = (readback << 1) | (ts_digitalRead(D_pin) & 0x0001);
    ts_digitalWrite(E_pin, LOW);
    ts_delayUs(BUS_DRV_DLY);
    }
  ts_digitalWrite(D_pin, LOW);
  ts_pinMode(D_pin, OUTPUT);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(E_pin, HIGH);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(D_pin, HIGH);
  ts_delayUs(BUS_DRV_DLY);
  ts_pinMode(E_pin, INPUT);
  ts_pinMode(D_pin, INPUT);
  return readback;
  }



static bool ICACHE_FLASH_ATTR goToWatch(void) {
  bool watch_success;
  
  if (configured == false)  watch_success = false;
  else {
    switch (checkBus()) {
      case S0_STATE:
        watch_success = false;
        break;
      case SLEEP_STATE:
        ts_digitalWrite(D_pin, HIGH);
        ts_pinMode(D_pin, OUTPUT);
        ts_digitalWrite(E_pin, LOW);
        ts_pinMode(E_pin, OUTPUT);
        ts_digitalWrite(D_pin, LOW);
        ts_pinMode(D_pin, INPUT);
        ts_digitalWrite(E_pin, HIGH);
        ts_pinMode(E_pin, INPUT);
        ts_delayUs(SLEEP_RECOVERY);
        if (checkBus() == WATCH_STATE) watch_success = true;
        else watch_success = false;
        break;
      case WATCH_STATE:
        watch_success = true;
        break;
      case S3_STATE:
        ts_digitalWrite(E_pin, HIGH);
        ts_pinMode(E_pin, OUTPUT);
        ts_digitalWrite(D_pin, HIGH);
        ts_pinMode(D_pin, OUTPUT);
        ts_digitalWrite(E_pin, LOW);
        ts_digitalWrite(D_pin, LOW);
        ts_pinMode(D_pin, INPUT);
        ts_digitalWrite(E_pin, HIGH);
        ts_pinMode(E_pin, INPUT);
        ts_delayUs(SLEEP_RECOVERY);
        if (checkBus() == WATCH_STATE) watch_success = true;
        else watch_success = false;
        break;
      default:
        watch_success = false;
        break;
      }
    }
  return watch_success;
  }





uint8_t ICACHE_FLASH_ATTR TS4231configDevice(uint16_t config_val) {
  uint8_t config_success = 0x00;
  uint16_t readback;
  
  configured = false;
  ts_pinMode(D_pin, INPUT);
  ts_pinMode(E_pin, INPUT);
  ts_digitalWrite(D_pin, LOW);
  ts_digitalWrite(E_pin, LOW);
  ts_pinMode(E_pin, OUTPUT);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(E_pin, HIGH);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(E_pin, LOW);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(E_pin, HIGH);
  ts_delayUs(BUS_DRV_DLY);
  ts_pinMode(D_pin, OUTPUT);
  ts_delayUs(BUS_DRV_DLY);
  ts_digitalWrite(D_pin, HIGH);
  ts_delayUs(BUS_DRV_DLY);
  ts_pinMode(E_pin, INPUT);
  ts_pinMode(D_pin, INPUT);
  if (checkBus() == S3_STATE) {
    writeConfig(config_val);
    readback = readConfig();
	printf( "READBACK: %04x\n", readback );
    if (readback == config_val) {
      configured = true;
      if (goToWatch()) config_success = CONFIG_PASS;
      else config_success = WATCH_FAIL;
      }
    else config_success = VERIFY_FAIL;
    }
  else config_success = BUS_FAIL;
  
  return config_success;
  }


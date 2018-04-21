#ifndef _TS4231_H
#define _TS4231_H

#include <c_types.h>
#include <esp82xxutil.h>

void ICACHE_FLASH_ATTR ConfigureTS4231( );

#define DEFAULT_CFG_WORD        0x392B  //configuration value

void ICACHE_FLASH_ATTR TS4231Setup();
bool ICACHE_FLASH_ATTR TS4231waitForLight(uint16_t light_timeout);
bool ICACHE_FLASH_ATTR TS4231goToWatch(void);
//uint16_t ICACHE_FLASH_ATTR TS4231readConfig(void);
//void ICACHE_FLASH_ATTR TS4231writeConfig(uint16_t config_val);
uint8_t ICACHE_FLASH_ATTR TS4231configDevice(uint16_t config_val);
//bool ICACHE_FLASH_ATTR TS4231goToSleep(void);

#endif


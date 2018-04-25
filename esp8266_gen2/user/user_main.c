//Copyright 2015 <>< Charles Lohr, see LICENSE file.

#include "mem.h"
#include "c_types.h"
#include "user_interface.h"
#include "ets_sys.h"
#include <uart.h>
#include "osapi.h"
#include "espconn.h"
#include "i2sduplex.h"
#include <commonservices.h>
#include "lighthouse_decode.h"
#include <esp82xxutil.h>
#include <mdns.h>
#include <lighthouse_decode.h>
#include "ts4231.h"

#define procTaskPrio        0
#define procTaskQueueLen    1


#define MAX_LH_CLIENTS 5
static volatile os_timer_t some_timer;
static struct espconn *pTcpListener;
static struct espconn *pTcpClients[MAX_LH_CLIENTS];
int                    pRREnds[MAX_LH_CLIENTS];
#define LEBUFFSIZE 256
volatile int pRRHead = 0;
struct LightEvent lebuffer[LEBUFFSIZE];


//int ICACHE_FLASH_ATTR StartMDNS();

void user_rf_pre_init(void)
{
	//nothing.
}


//Tasks that happen all the time.

os_event_t    procTaskQueue[procTaskQueueLen];

void ICACHE_FLASH_ATTR DumpAllConns()
{
	int i;
	for( i = 0; i < MAX_LH_CLIENTS; i++ )
	{
		espconn_disconnect(pTcpClients[i]);
		pTcpClients[i] = 0;
	}
}

static void ICACHE_FLASH_ATTR procTask(os_event_t *events)
{
	int ic;
	CSTick( 0 );

	//Check to see if we have any connected clients we can send data.
	int there_is_a_connection = 0;
	for( ic = 0; ic < MAX_LH_CLIENTS; ic++ )
	{		
		struct espconn * sock = pTcpClients[ic];
		if( sock )
		{
			there_is_a_connection = 1;
			int prrc = pRREnds[ic];
			if( TCPCanSend( sock, 1024 ) && prrc != pRRHead )
			{
				struct LightEvent les[64]; //Max 64 lighting events per packet.
				int i = 0;
				while( prrc != pRRHead && i < 64)
				{
					ets_memcpy( &les[i++], &lebuffer[prrc], sizeof( struct LightEvent ) );
					prrc = (prrc+1)%LEBUFFSIZE;
				}
				pRREnds[ic] = prrc;
				espconn_send( sock, (uint8_t*)&les[0], sizeof( struct LightEvent ) * i );
			}
		}
	}

	if( there_is_a_connection )
	{
		LHSM.debugmonitoring = 0;
		if( LHSM.debugbufferflag == 2 )
			LHSM.debugbufferflag = 0;
	}


	if( LHSM.configure_state == 1 )
	{
		printf( "Configure the TS4231\n" );
		ConfigureTS4231( );
		LHSM.configure_state = 2;
	}

	system_os_post(procTaskPrio, 0, 0 );
}

int SendPacket( struct LightEvent * data )
{
	ets_memcpy( &lebuffer[pRRHead], data, sizeof( struct LightEvent ) );
	pRRHead = (pRRHead+1)%LEBUFFSIZE;
	return 1;
}

//Timer event.
static void ICACHE_FLASH_ATTR myTimer(void *arg)
{
	printf( "%d %d\n", LHSM.debugbufferflag, LHSM.debugbufferlen );
	CSTick( 1 );
}



void ICACHE_FLASH_ATTR charrx( uint8_t c )
{
	//Called from UART.
}




LOCAL void ICACHE_FLASH_ATTR
ptcp_disconnetcb(void *arg) {
    struct espconn *pespconn = (struct espconn *) arg;
	int ic = ((int)pespconn->reverse);
	pTcpClients[ic] = 0;
	printf( "Disconnect %d\n", ic );
}

LOCAL void ICACHE_FLASH_ATTR ptcp_recvcb(void *arg, char *pusrdata, unsigned short length)
{
    struct espconn *pespconn = (struct espconn *) arg;
	int ic = (int)pespconn->reverse;
	printf( "Receive %d\n", ic );
	//TODO: Handle commands on tcp port.
}

void ICACHE_FLASH_ATTR ptcp_connect_listener(void *arg)
{

	int i;
    struct espconn *pespconn = (struct espconn *)arg;

	for( i = 0; i < MAX_LH_CLIENTS; i++ )
	{
		if( pTcpClients[i] == 0 )
		{
			pespconn->reverse = (void*)i;
			pTcpClients[i] = pespconn;
			pRREnds[i] = pRRHead;
			break;
		}
	}
	if( i == MAX_LH_CLIENTS )
	{
		espconn_disconnect( pespconn );
		return;
	}

	printf( "New conn %d\n", i );

	//http://bbs.espressif.com/viewtopic.php?f=21&t=320
	espconn_set_opt(pespconn, 0x04); // enable write buffer
	//Doing this allows us to queue writes in the socket, which
	//SIGNIFICANTLY speeds up transfer in Windows.

	//espconn_regist_write_finish( pespconn, http_sentcb );
    espconn_regist_recvcb( pespconn, ptcp_recvcb );
    espconn_regist_disconcb( pespconn, ptcp_disconnetcb );
}

void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
//	ets_delay_us(200000 );
	uart0_sendStr("\r\nesp8266 ws2812 driver\r\n");


//	int opm = wifi_get_opmode();
//	if( opm == 1 ) need_to_switch_opmode = 120;
//	wifi_set_opmode_current(2);
//Uncomment this to force a system restore.
//	system_restore();

	CSSettingsLoad( 0 );
	CSPreInit();

	//Configure buffers
	lighthouse_setup();

	pTcpListener = (struct espconn *)os_zalloc(sizeof(struct espconn));
	ets_memset( pTcpListener, 0, sizeof( struct espconn ) );
	espconn_create( pTcpListener );
	pTcpListener->type = ESPCONN_TCP;
    pTcpListener->state = ESPCONN_NONE;
	pTcpListener->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	pTcpListener->proto.tcp->local_port = 10900;
    espconn_regist_connectcb(pTcpListener, ptcp_connect_listener);
    espconn_accept(pTcpListener);
    espconn_regist_time(pTcpListener, 15, 0); //timeout

	CSInit();

	SetServiceName( "lighthouse" );
	AddMDNSName( "esp82xx" );
	AddMDNSName( "lighthouse" );
	AddMDNSService( "_http._tcp", "An ESP8266 Webserver", 80 );
	AddMDNSService( "_lighthouse._tcp", "Lighthouse data source", 10900 );
	AddMDNSService( "_cn8266._udp", "ESP8266 Backend", 7878 );

	//Add a process
	system_os_task(procTask, procTaskPrio, procTaskQueue, procTaskQueueLen);

	//Timer example
	os_timer_disarm(&some_timer);
	os_timer_setfn(&some_timer, (os_timer_func_t *)myTimer, NULL);
	os_timer_arm(&some_timer, 60, 1);

	//Configure 
	testi2s_init();

	system_update_cpu_freq( SYS_CPU_160MHZ );

	system_os_post(procTaskPrio, 0, 0 );
}


//There is no code in this project that will cause reboots if interrupts are disabled.
void EnterCritical()
{
}

void ExitCritical()
{
}




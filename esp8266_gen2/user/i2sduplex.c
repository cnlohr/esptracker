//ESP8266 I2S Input+Output

#include "slc_register.h"
#include <c_types.h>
#include "user_interface.h"
#include "pin_mux_register.h"
#include "dmastuff.h"
#include "i2sduplex.h"
#include <commonservices.h>
#include <esp82xxutil.h>
#include "lighthouse_decode.h"


//These contol the speed at which the bus comms.
#define WS_I2S_BCK 2  //Can't be less than 1.
#define WS_I2S_DIV 1

//I2S DMA buffer descriptors
static struct sdio_queue i2sBufDescRX[DMABUFFERDEPTH];
uint32_t i2sBDRX[I2SDMABUFLEN*DMABUFFERDEPTH];
int fxcycle;
int erx, etx;

LOCAL void slc_isr(void) {
	//portBASE_TYPE HPTaskAwoken=0;
	struct sdio_queue *finishedDesc;
	static struct sdio_queue * lastrxdesc;
	uint32 slc_intr_status;
	int x;
	fxcycle++;

	slc_intr_status = READ_PERI_REG(SLC_INT_STATUS);
	//clear all intr flags
	WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);//slc_intr_status);

	if ( (slc_intr_status & SLC_TX_EOF_INT_ST))
	{
		static int k;
		finishedDesc=(struct sdio_queue*)READ_PERI_REG(SLC_TX_EOF_DES_ADDR);

		struct sdio_queue * desc;

		int trycount = 0;

		desc = &i2sBufDescRX[k];

		//Warning if your page size is limited, you can get some unusual buffer underflows where there is a missing packet.
		//This usually only happens if your buffer size is relatively small, i.e. less than 256 words.

		while( desc != finishedDesc ) {
			desc = &i2sBufDescRX[k++];
			if( k == DMABUFFERDEPTH ) k = 0;

			uint32_t * data = (uint32_t*)desc->buf_ptr;
			lighthouse_decode( data, I2SDMABUFLEN );
			desc->owner=1;
			trycount++;
		}

		if( trycount > DMABUFFERDEPTH-2 ) printf( "XF%d*", trycount );
	}



}

//Initialize I2S subsystem for DMA circular buffer use
void ICACHE_FLASH_ATTR testi2s_init() {
	int x, y;
	//Bits are shifted out

	//Initialize DMA buffer descriptors in such a way that they will form a circular
	//buffer.

	for (x=0; x<DMABUFFERDEPTH; x++) {
		i2sBufDescRX[x].owner=1;
		i2sBufDescRX[x].eof=1;
		i2sBufDescRX[x].sub_sof=0;
		i2sBufDescRX[x].datalen=I2SDMABUFLEN*4;
		i2sBufDescRX[x].blocksize=I2SDMABUFLEN*4;
		i2sBufDescRX[x].buf_ptr=(uint32_t)&i2sBDRX[x*I2SDMABUFLEN];
		i2sBufDescRX[x].unused=0;
		i2sBufDescRX[x].next_link_ptr=(int)((x<(DMABUFFERDEPTH-1))?(&i2sBufDescRX[x+1]):(&i2sBufDescRX[0]));
		for( y = 0; y < I2SDMABUFLEN; y++ )
		{
			i2sBDRX[y+x*I2SDMABUFLEN] = 0xaaaaaaaa;
		}
	}

	//Reset DMA )
	//SLC_TX_LOOP_TEST = IF this isn't set, SO will occasionally get unrecoverable errors when you underflow.
	//Originally this little tidbit was found at https://github.com/pvvx/esp8266web/blob/master/info/libs/bios/sip_slc.c
	//
	//I have not tried without SLC_AHBM_RST | SLC_AHBM_FIFO_RST.  I just assume they are useful?
	SET_PERI_REG_MASK(SLC_CONF0, SLC_TX_LOOP_TEST |SLC_RXLINK_RST|SLC_TXLINK_RST|SLC_AHBM_RST | SLC_AHBM_FIFO_RST);
	CLEAR_PERI_REG_MASK(SLC_CONF0, SLC_RXLINK_RST|SLC_TXLINK_RST|SLC_AHBM_RST | SLC_AHBM_FIFO_RST);

	//Clear DMA int flags
	SET_PERI_REG_MASK(SLC_INT_CLR,  0xffffffff);
	CLEAR_PERI_REG_MASK(SLC_INT_CLR,  0xffffffff);

	//Enable and configure DMA
	CLEAR_PERI_REG_MASK(SLC_CONF0, (SLC_MODE<<SLC_MODE_S));
	SET_PERI_REG_MASK(SLC_CONF0,(1<<SLC_MODE_S));
	
	CLEAR_PERI_REG_MASK(SLC_TX_LINK,SLC_TXLINK_DESCADDR_MASK);
	SET_PERI_REG_MASK(SLC_TX_LINK, ((uint32)&i2sBufDescRX[0]) & SLC_TXLINK_DESCADDR_MASK); //any random desc is OK, we don't use TX but it needs something valid
	//CLEAR_PERI_REG_MASK(SLC_RX_LINK,SLC_RXLINK_DESCADDR_MASK);
	//SET_PERI_REG_MASK(SLC_RX_LINK, ((uint32)&i2sBufDescTX[0]) & SLC_RXLINK_DESCADDR_MASK);

	//Attach the DMA interrupt
	ets_isr_attach(ETS_SLC_INUM, slc_isr);
	WRITE_PERI_REG(SLC_INT_ENA,  SLC_INTEREST_EVENT);

	//clear any interrupt flags that are set
	WRITE_PERI_REG(SLC_INT_CLR, 0xffffffff);
	///enable DMA intr in cpu
	ets_isr_unmask(1<<ETS_SLC_INUM);

	//Start transmission
	SET_PERI_REG_MASK(SLC_TX_LINK, SLC_TXLINK_START);
	//SET_PERI_REG_MASK(SLC_RX_LINK, SLC_RXLINK_START);


	//Init pins to i2s functions
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_I2SI_DATA);
	//PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_I2SI_WS);  //Dunno why - this is needed.  If it's not enabled, nothing will be read.

	//PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_I2SO_DATA);
	//PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_I2SO_WS);
	//PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_I2SO_BCK);

	//Enable clock to i2s subsystem
	i2c_writeReg_Mask_def(i2c_bbpll, i2c_bbpll_en_audio_clock_out, 1);

	//Reset I2S subsystem
	CLEAR_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);
	SET_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);
	CLEAR_PERI_REG_MASK(I2SCONF,I2S_I2S_RESET_MASK);

	CLEAR_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN|(I2S_I2S_RX_FIFO_MOD<<I2S_I2S_RX_FIFO_MOD_S)|(I2S_I2S_TX_FIFO_MOD<<I2S_I2S_TX_FIFO_MOD_S));
	SET_PERI_REG_MASK(I2S_FIFO_CONF, I2S_I2S_DSCR_EN);
	WRITE_PERI_REG(I2SRXEOF_NUM, RX_NUM);

	CLEAR_PERI_REG_MASK(I2SCONF_CHAN, (I2S_TX_CHAN_MOD<<I2S_TX_CHAN_MOD_S)|(I2S_RX_CHAN_MOD<<I2S_RX_CHAN_MOD_S));

	//Clear int
	SET_PERI_REG_MASK(I2SINT_CLR,   I2S_I2S_TX_REMPTY_INT_CLR|I2S_I2S_TX_WFULL_INT_CLR|
			I2S_I2S_RX_WFULL_INT_CLR|I2S_I2S_PUT_DATA_INT_CLR|I2S_I2S_TAKE_DATA_INT_CLR);
	CLEAR_PERI_REG_MASK(I2SINT_CLR, I2S_I2S_TX_REMPTY_INT_CLR|I2S_I2S_TX_WFULL_INT_CLR|
			I2S_I2S_RX_WFULL_INT_CLR|I2S_I2S_PUT_DATA_INT_CLR|I2S_I2S_TAKE_DATA_INT_CLR);


	CLEAR_PERI_REG_MASK(I2SCONF, I2S_TRANS_SLAVE_MOD|I2S_RECE_SLAVE_MOD|
						(I2S_BITS_MOD<<I2S_BITS_MOD_S)|
						(I2S_BCK_DIV_NUM <<I2S_BCK_DIV_NUM_S)|
                                    	(I2S_CLKM_DIV_NUM<<I2S_CLKM_DIV_NUM_S));
	
	SET_PERI_REG_MASK(I2SCONF, I2S_RIGHT_FIRST|I2S_MSB_RIGHT|
						I2S_RECE_MSB_SHIFT|I2S_TRANS_MSB_SHIFT|
						((WS_I2S_BCK&I2S_BCK_DIV_NUM )<<I2S_BCK_DIV_NUM_S)|
						((WS_I2S_DIV&I2S_CLKM_DIV_NUM)<<I2S_CLKM_DIV_NUM_S) );


	//enable int
	SET_PERI_REG_MASK(I2SINT_ENA,
			/* I2S_I2S_TX_REMPTY_INT_ENA|I2S_I2S_TX_WFULL_INT_ENA| */
			I2S_I2S_RX_REMPTY_INT_ENA/*|I2S_I2S_TX_PUT_DATA_INT_ENA*/|I2S_I2S_RX_TAKE_DATA_INT_ENA);

	//Start transmission
	//SET_PERI_REG_MASK(I2SCONF,I2S_I2S_TX_START|I2S_I2S_RX_START);
	SET_PERI_REG_MASK(I2SCONF,I2S_I2S_RX_START); //Don't enable the TX Engine.
}



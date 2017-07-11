/*
 * edge_audio_acq.c
 *
 *  Created on: 04/06/2017
 *      Author: root
 */
#include "../usr_include/beeinformed.h"

#define AUDIO_TASK_STK_SIZE			256
#define AUDIO_TASK_PRIO				(configMAX_PRIORITIES - 2)
#define AUDIO_DMA_SIGNAL			DMAREQ_ADC0_SINGLE
#define AUDIO_DMA_CHANNEL			8


/** static variables */
static ppbuf_t 	audio_buffer;
static SemaphoreHandle_t audio_signal;
static TimerHandle_t     adc_timer;
static uint32_t 		 adc_cnt = 0;
extern DMA_DESCRIPTOR_TypeDef dmaControlBlock[];

/** internal functions */

/**
 * @brief start audio capture engine
 */
static void audio_start_capture(void)
{
	/*
	 * trigger dma capturing engine pointing to next free buffer
	 */
	DMA_ActivateBasic(AUDIO_DMA_CHANNEL,
						true,
						false,
						&audio_buffer.data[audio_buffer.active][0],
						&ADC0->SINGLEDATA,
						AUDIO_BUFFER_SIZE);
}


/**
 * @brief audio dma callback
 */
static void audio_dma_callback(uint32_t channel, bool primary, void *param)
{
	portBASE_TYPE ctw = 0;
	BCDS_UNUSED(primary);

	if(NULL==param) {
		goto error_not_dma;

	}

	/* evaluate if the channel is the correct */
	if(AUDIO_DMA_CHANNEL!=channel) {
		goto error_not_dma;

	}

	SemaphoreHandle_t *sem = param;
	audio_buffer.active ^= 0x01;
	xSemaphoreGiveFromISR(sem, &ctw);
	portYIELD_FROM_ISR(ctw);

error_not_dma:
	return;
}

/**
 * @brief analog input isr
 */
static void audio_adc_timer(TimerHandle_t *t)
{
	(void)t;
	uint16_t adc_data = (uint16_t)ADC_DataSingleGet(ADC0);
	audio_buffer.data[audio_buffer.active][adc_cnt] = adc_data;
	//printf("audio_adc_timer: sample value: 0x%X\n\r\n\r", adc_data);

	adc_cnt++;
	if(adc_cnt == AUDIO_BUFFER_SIZE) {
		adc_cnt = 0;
		xSemaphoreGive(audio_signal);
		xTimerStop(t, 0);
	}

}


/**
 * @brief init audio engine subsystem
 */
static void audio_hw_init(void)
{
	ADC_InitSingle_TypeDef seq = ADC_INITSINGLE_DEFAULT;
	ADC_Init_TypeDef cfg = ADC_INIT_DEFAULT;
	DMA_CfgChannel_TypeDef dma_channel;
	DMA_CfgDescr_TypeDef   dma_cfg;
	DMA_Init_TypeDef	   dma_init;

	/* turn on the AKU340 power supply */
	PTD_pinOutSet(PTD_PORT_AKU340_VDD,PTD_PIN_AKU340_VDD);

	seq.input = adcSingleInpCh4;
	seq.rep   = true;

	/* setup DMA internal structures, 
	 * we want to receive requests from ADC
	 * and fills a buffer on RAM
	 */
	dma_init.hprot = 0;
	dma_init.controlBlock = dmaControlBlock; 

	DMA_CB_TypeDef 	cbprop = {.cbFunc = audio_dma_callback,
								  .primary = false,
								  .userPtr = audio_signal};

	dma_channel.cb = &cbprop;
	dma_channel.enableInt=true;
	dma_channel.select = AUDIO_DMA_SIGNAL;
	
	dma_cfg.arbRate = dmaArbitrate1;
	dma_cfg.srcInc = dmaDataIncNone;
	dma_cfg.dstInc = dmaDataInc2;
	dma_cfg.hprot = 0;
	dma_cfg.size = dmaDataSize2;
	


	/* clocks and inits the ADC */
	CMU_ClockEnable(cmuClock_ADC0,true);
	ADC_Init(ADC0, &cfg);
	ADC_InitSingle(ADC0, &seq);
	ADC_Start(ADC0, adcStartSingle);

	/* clocks and inits the DMA module */
	CMU_ClockEnable(cmuClock_DMA,true);
	DMA_Init(&dma_cfg);
	DMA_CfgChannel(AUDIO_DMA_CHANNEL, &dma_channel);
	DMA_CfgDescr(AUDIO_DMA_CHANNEL, true, &dma_cfg);

}


/**
 * @brief audio acquisition application task
 */
static void audio_task(void *args)
{
	(void)args;
	audio_signal = xSemaphoreCreateBinary();
	adc_timer = xTimerCreate("audio_adc_timer", AUDIO_SAMPLE_RATE, pdTRUE,
							NULL, audio_adc_timer);

	assert(audio_signal != NULL);

	/* assign the available data to env */
	data_env.audio_data = &audio_buffer.data[0][0];
	audio_buffer.active = 0;

	/* init adc and DMA subsystem
	 * and starts to listen the acoustic sensor
	 */
	audio_hw_init();
	audio_start_capture();

	for(;;){
		uint8_t active;

		/* wait new data available from ISR */
		xSemaphoreTake(audio_signal, portMAX_DELAY);
		printf("%s: new audio data block arrived! \n\r", __func__);


		/*
		 * obtains the new captured buffer an give the next
		 * free buffer to fill
		 */
		active = audio_buffer.active;
		audio_buffer.active ^= 0x01;
		audio_start_capture();

		/* no data available during copy */
		data_env.audio_data = &audio_buffer.data[active][0];
	}
}

/** public functions */




void audio_app_init(void)
{
	portBASE_TYPE err;

	err = xTaskCreate(audio_task,"audio_app",AUDIO_TASK_STK_SIZE,NULL,AUDIO_TASK_PRIO,NULL);
	assert(err == pdPASS);
}

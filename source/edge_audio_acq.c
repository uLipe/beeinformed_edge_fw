/*
 * edge_audio_acq.c
 *
 *  Created on: 04/06/2017
 *      Author: root
 */
#include "../usr_include/beeinformed.h"

#define AUDIO_TASK_STK_SIZE			256
#define AUDIO_TASK_PRIO				(configMAX_PRIORITIES - 2)
//#define AUDIO_DMA_SIGNAL			DMAREQ_ADC0_SINGLE
//#define AUDIO_DMA_CHANNEL			8


/** static variables */
static volatile ppbuf_t audio_buffer;
static SemaphoreHandle_t audio_signal;
static uint16_t adc_cnt = 0;
static QueueHandle_t audio_request;
static bool audio_rdy = false;

//extern DMA_DESCRIPTOR_TypeDef dmaControlBlock[];

/** internal functions */


/**
 * @brief audio dma callback
 */
//static void audio_record_callback(uint32_t channel, bool primary, void *param)
//{
//	portBASE_TYPE ctw = 0;
//	BCDS_UNUSED(primary);
//	BCDS_UNUSED(param);
//
//	puts("*** audio ISR ***\n\r");
//	TIMER_Enable(TIMER0, false);
//	audio_buffer.active ^= 0x01;
//	xSemaphoreGiveFromISR(audio_signal, &ctw);
//	portYIELD_FROM_ISR(ctw);
//}

/**
 * @brief capture samples from ADC
 */
void ADC0_IRQHandler(void)
{
	/* collect the samples */
	audio_buffer.data[audio_buffer.active][adc_cnt] = ADC_DataSingleGet(ADC0);
	//printf("%s: Data mic captured: %u \n\r", __func__, audio_buffer.data[audio_buffer.active][adc_cnt]);
	adc_cnt++;
	ADC_IntClear(ADC0, ADC_IFC_SINGLE);

	if(adc_cnt >= AUDIO_BUFFER_SIZE) {
		/* samples collected notify the audio capture task */
		TIMER_Enable(TIMER0, false);
		portBASE_TYPE ctw = 0;
		xSemaphoreGiveFromISR(audio_signal, &ctw);
		portYIELD_FROM_ISR(ctw);
	}
}


/**
 * @brief start audio capture engine
 */
static uint8_t audio_start_capture(void)
{
	uint8_t ret = audio_buffer.active;
	/*
	 * trigger dma capturing engine pointing to next free buffer
	 */
/*
	DMA_ActivateBasic(AUDIO_DMA_CHANNEL,
						true,
						false,
						&audio_buffer.data[audio_buffer.active],
						(void *)&ADC0->SINGLEDATA,
						(uint32_t)(AUDIO_BUFFER_SIZE - 1));
 */
	/*
	 * Reset sample counter and flip to next buffer to capture
	 */
	adc_cnt = 0;
	audio_buffer.active ^= 0x01;
	TIMER_Enable(TIMER0, true);
	printf("%s: started to capture audio! \n\r", __func__);

	return(ret);

}


/**
 * @brief inits the DMA subsystem
 */

//static void audio_init_dma(void)
//{
//	DMA_CfgChannel_TypeDef dma_channel;
//	DMA_CfgDescr_TypeDef   dma_cfg;
//	DMA_Init_TypeDef	   dma_init;
//	CMU_ClockEnable(cmuClock_DMA,true);
//
//	/* setup DMA internal structures,
//	 * we want to receive requests from ADC
//	 * and fills a buffer on RAM
//	 */
//
//	dma_init.hprot = 0;
//	dma_init.controlBlock = dmaControlBlock;
//	DMA_Reset();
//	DMA_Init(&dma_init);
//
//
//	DMA_CB_TypeDef 	cbprop;
//	cbprop.cbFunc = audio_record_callback;
//	cbprop.userPtr = NULL;
//
//	dma_channel.cb = &cbprop;
//	dma_channel.enableInt=true;
//	dma_channel.highPri = false;
//	dma_channel.select = AUDIO_DMA_SIGNAL;
//	DMA_CfgChannel(AUDIO_DMA_CHANNEL, &dma_channel);
//
//
//	dma_cfg.srcInc = dmaDataIncNone;
//	dma_cfg.dstInc = dmaDataInc2;
//	dma_cfg.hprot = 0;
//	dma_cfg.size = dmaDataSize2;
//	dma_cfg.arbRate = dmaArbitrate1;
//	DMA_CfgDescr(AUDIO_DMA_CHANNEL, true, &dma_cfg);
//
//}

/**
 * @brieg inits the timer used to sample analog readings
 */
static void audio_init_timer(void)
{

	/* setup PRS to accept timer overflow signals and trigger adc conversions
	 */
	CMU_ClockEnable(cmuClock_PRS, true);
	PRS_SourceSignalSet(0, PRS_CH_CTRL_SOURCESEL_TIMER0, PRS_CH_CTRL_SIGSEL_TIMER0OF, prsEdgeOff);


	/* Select timer parameters */
	CMU_ClockEnable(cmuClock_TIMER0, true);

	const TIMER_Init_TypeDef timerInit =
	{
		.enable     = false,                         /* Start counting when init complete */
		.debugRun   = false,                        /* Counter not running on debug halt */
		.prescale   = timerPrescale1,            /* Prescaler of 1024 -> overflow after aprox 4.70 seconds */
		.clkSel     = timerClkSelHFPerClk,          /* TIMER0 clocked by the HFPERCLK */
		.fallAction = timerInputActionStop,         /* Stop counter on falling edge */
		.riseAction = timerInputActionReloadStart,  /* Reload and start on rising edge */
		.mode       = timerModeUp,                  /* Counting up */
		.dmaClrAct  = false,                        /* No DMA */
		.quadModeX4 = false,                        /* No quad decoding */
		.oneShot    = false,                        /* Counting up constinuously */
		.sync       = false,                        /* No start/stop/reload by other timers */
	};

  TIMER_Init(TIMER0, &timerInit);
  TIMER_TopSet(TIMER0, SystemCoreClock/AUDIO_SAMPLE_RATE);
}


/**
 * @brief inits the adc converter
 */
static void audio_init_adc(void)
{
	ADC_InitSingle_TypeDef seq = ADC_INITSINGLE_DEFAULT;
	ADC_Init_TypeDef cfg = ADC_INIT_DEFAULT;

	/* turn on the AKU340 power supply */
	PTD_pinOutSet(PTD_PORT_AKU340_VDD,PTD_PIN_AKU340_VDD);


	/* clocks and inits the ADC */
	CMU_ClockEnable(cmuClock_ADC0,true);
	cfg.timebase = ADC_TimebaseCalc(0);
	cfg.prescale = ADC_PrescaleCalc(7000000, 0);

	ADC_Init(ADC0, &cfg);

	seq.input = adcSingleInpCh4;
	seq.rep   = false;
	seq.prsEnable =true;
	seq.resolution = adcRes12Bit;
	seq.reference = adcRef1V25;
	seq.prsSel = adcPRSSELCh0;
	seq.acqTime = adcAcqTime4;

	ADC_IntDisable(ADC0, ADC_IEN_SINGLE);
	NVIC_DisableIRQ(ADC0_IRQn);
	NVIC_SetPriority(ADC0_IRQn, 254);

	ADC_InitSingle(ADC0, &seq);

	ADC_IntEnable(ADC0, ADC_IEN_SINGLE);
	NVIC_EnableIRQ(ADC0_IRQn);
}


/**
 * @brief inits in proper order the audio hw components
 */
static void audio_hw_init(void)
{
	//audio_init_dma();
	audio_init_adc();
	audio_init_timer();
	printf("%s: initialized audio hardware! \n\r", __func__);
}


/**
 * @brief audio acquisition application task
 */
static void audio_task(void *args)
{
	uint8_t active;
	(void)args;
	audio_signal = xSemaphoreCreateBinary();
	audio_request = xQueueCreate(AUDIO_MAX_RECORDS, sizeof(audio_request_t));
	assert(audio_signal != NULL);
	assert(audio_request != NULL);


	audio_request_t current_req = {0};
	uint16_t noof_blocks = 0;
	uint16_t *audio_ptr = NULL;

	/* assign the available data to env */
	audio_buffer.active = 0;

	/* init adc and DMA subsystem
	 * and starts to listen the acoustic sensor
	 */
	audio_hw_init();
	audio_rdy = true;

	for(;;){

		/* wait for request of app */
		xQueueReceive(audio_request, &current_req, portMAX_DELAY);

		/* samples file needs to be valid */
		if(current_req.samples == NULL) {
			printf("%s: invalid audio acquisition file! \n\r", __func__);
			if(current_req.cb != NULL) {
				current_req.cb(current_req.user_data, kaudio_sample_invalid);
			}
			continue;
		}

		/* file size in seconds needs to be valid also */
		if(!current_req.seconds) {
			printf("%s: invalid period of  acquisition! \n\r", __func__);
			if(current_req.cb != NULL) {
				current_req.cb(current_req.user_data, kaudio_sec_invalid);
			}
			continue;
		}


		/* calculate the noof blocks for required audio window size */
		noof_blocks = (current_req.seconds * (AUDIO_SAMPLE_RATE))/AUDIO_BUFFER_SIZE;
		printf("%s: audio acquisition block size, block: %u \n\r", __func__, noof_blocks);

		audio_ptr = current_req.samples;

		/*start acquisition */
		active = audio_start_capture();



		 do {
			/* wait new data available from ISR */
			xSemaphoreTake(audio_signal, portMAX_DELAY);

			/* data arrived, triggers the next capture stage, and
			 * perform copy of new arrived samples to the audio file
			 */
			active = audio_start_capture();
			memcpy(audio_ptr, &audio_buffer.data[active], sizeof(uint16_t) * AUDIO_BUFFER_SIZE);
			audio_ptr += AUDIO_BUFFER_SIZE;
			noof_blocks--;

			printf("%s: new audio data block arrived, block: %u \n\r", __func__, noof_blocks);
		}while(noof_blocks > 0);


		/* once audio is captured, notify to the asynch callback
		 * that the acquisition is complete and resides in
		 * previous passed file
		 */
		if(current_req.cb != NULL) {
			current_req.cb(current_req.user_data, kaudio_acquisition_ok);
		}

	}
}

/** public functions */
void audio_app_init(void)
{
	portBASE_TYPE err;

	err = xTaskCreate(audio_task,"audio_app",AUDIO_TASK_STK_SIZE,NULL,AUDIO_TASK_PRIO,NULL);
	assert(err == pdPASS);
}

bool audio_start_record(uint16_t *outfile, uint16_t seconds, audio_complete_callback_t cb, void *user_data)
{
	bool ret = false;

	/* ignore call if audio is initializing or not configured */
	if(!audio_rdy)
		goto cleanup;

	/* validate arguments */
	if(outfile == NULL)
		goto cleanup;

	if(!seconds)
		goto cleanup;

	if(!cb)
		goto cleanup;

	audio_request_t req;
	req.samples = outfile;
	req.seconds = seconds;
	req.cb = cb;
	req.user_data = user_data;

	if(xQueueSend(audio_request, &req, 0) != pdPASS) {
		if(cb) {
			cb(user_data, kaudio_unk_error);
		}
	} else {
		ret = true;
	}

cleanup:
	return(ret);
}

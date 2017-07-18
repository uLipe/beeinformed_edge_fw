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
static TaskHandle_t audio_tcb;
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
	ADC_IntClear(ADC0, ADC_IFC_SINGLE);
	/* collect the samples */
	audio_buffer.data[audio_buffer.active][adc_cnt] = ADC_DataSingleGet(ADC0);
	adc_cnt++;
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
	seq.reference = adcRefVDD;
	seq.prsSel = adcPRSSELCh0;
	seq.acqTime = adcAcqTime4;

	ADC_IntDisable(ADC0, ADC_IEN_SINGLE);
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
	assert(audio_signal != NULL);


	/* assign the available data to env */
	audio_buffer.active = 0;

	/* init adc and DMA subsystem
	 * and starts to listen the acoustic sensor
	 */
	audio_hw_init();
	active = audio_start_capture();

	for(;;){
		/* wait new data available from ISR */
		xSemaphoreTake(audio_signal, portMAX_DELAY);
		printf("%s: new audio data block arrived! \n\r", __func__);
		active = audio_start_capture();
	}
}

/** public functions */
void audio_app_init(void)
{
	portBASE_TYPE err;

	err = xTaskCreate(audio_task,"audio_app",AUDIO_TASK_STK_SIZE,NULL,AUDIO_TASK_PRIO,&audio_tcb);
	assert(err == pdPASS);
}

void audio_start_record(void)
{
	//vTaskResume(audio_tcb);
}

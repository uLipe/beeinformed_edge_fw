/*
 * edge_audio_acq.c
 *
 *  Created on: 04/06/2017
 *      Author: root
 */
#include "../usr_include/beeinformed.h"

#define AUDIO_TASK_STK_SIZE			256
#define AUDIO_TASK_PRIO				(15)
#define AUDIO_MAX_PERIOD_ACQ		50

/** static variables */
static volatile uint16_t audio_buffer[AUDIO_BUFFER_SIZE];
static SemaphoreHandle_t audio_signal;
static uint16_t adc_cnt = 0;
static QueueHandle_t audio_request;
static bool audio_rdy = false;
static uint32_t samples_to_capture = 0;

/** internal functions */

/**
 * @brief capture samples from ADC
 */
void ADC0_IRQHandler(void)
{
	/* collect the samples */
	audio_buffer[adc_cnt] = ADC_DataSingleGet(ADC0);
	//printf("%s: Data mic captured: %u \n\r", __func__, audio_buffer[adc_cnt]);
	adc_cnt++;
	ADC_IntClear(ADC0, ADC_IFC_SINGLE);

	if(adc_cnt >= samples_to_capture) {
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
static void audio_start_capture(void)
{
	/*
	 * Reset sample counter and flip to next buffer to capture
	 */
	adc_cnt = 0;
	TIMER_Enable(TIMER0, true);
	printf("%s: started to capture audio! \n\r", __func__);
}



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
	BSP_Mic_AKU340_Connect();
	BSP_Mic_AKU340_Enable();

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
	audio_init_adc();
	audio_init_timer();
	printf("%s: initialized audio hardware! \n\r", __func__);
}


/**
 * @brief audio acquisition application task
 */
static void audio_task(void *args)
{
	(void)args;
	audio_signal = xSemaphoreCreateBinary();
	audio_request = xQueueCreate(AUDIO_MAX_RECORDS, sizeof(audio_request_t));
	assert(audio_signal != NULL);
	assert(audio_request != NULL);


	audio_request_t current_req = {0};
	uint16_t *audio_ptr = NULL;

	/* init adc
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
		if(current_req.miliseconds > AUDIO_MAX_PERIOD_ACQ) {
			printf("%s: invalid period of  acquisition! \n\r", __func__);
			if(current_req.cb != NULL) {
				current_req.cb(current_req.user_data, kaudio_sec_invalid);
			}
			continue;
		}

		/* calculate the noof blocks for required audio window size */
		samples_to_capture = (current_req.miliseconds * (AUDIO_SAMPLE_RATE/1000));
		printf("%s: audio acquisition block size, block: %u \n\r", __func__, samples_to_capture);

		audio_ptr = current_req.samples;

		/*start acquisition */
		audio_start_capture();

		/* wait new data available from ADC subsys */
		xSemaphoreTake(audio_signal, portMAX_DELAY);
		memcpy(audio_ptr, &audio_buffer, sizeof(uint16_t) * samples_to_capture);
		printf("%s: new audio data block arrived. \n\r", __func__);

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

bool audio_start_record(uint16_t *outfile, uint16_t miliseconds, audio_complete_callback_t cb, void *user_data)
{
	bool ret = false;

	/* ignore call if audio is initializing or not configured */
	if(!audio_rdy)
		goto cleanup;

	/* validate arguments */
	if(outfile == NULL)
		goto cleanup;

	if(!miliseconds)
		goto cleanup;

	if(!cb)
		goto cleanup;

	audio_request_t req;
	req.samples = outfile;
	req.miliseconds = miliseconds;
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

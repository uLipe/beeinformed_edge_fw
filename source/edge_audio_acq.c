/*
 * edge_audio_acq.c
 *
 *  Created on: 04/06/2017
 *      Author: root
 */
#include "../usr_include/beeinformed.h"

#define AUDIO_TASK_STK_SIZE			256
#define AUDIO_TASK_PRIO				(configMAX_PRIORITIES - 2)


/** static variables */
static ppbuf_t 	audio_buffer;
static uint16_t	audio_ext_buffer[AUDIO_BUFFER_SIZE]={0};
static SemaphoreHandle_t audio_signal;
static TimerHandle_t     adc_timer;
static uint32_t 		 adc_cnt = 0;

/** internal functions */

/**
 * @brief start audio capture engine
 */
static void audio_start_capture(void)
{
	adc_cnt = 0;
	xTimerStart(adc_timer, 0);
}

/**
 * @brief init audio engine subsystem
 */
static void audio_hw_init(void)
{
	ADC_InitSingle_TypeDef seq = ADC_INITSINGLE_DEFAULT;
	ADC_Init_TypeDef cfg = ADC_INIT_DEFAULT;

	/* turn on the AKU340 power supply */
	PTD_pinOutSet(PTD_PORT_AKU340_VDD,PTD_PIN_AKU340_VDD);

	seq.input = adcSingleInpCh4;
	seq.rep   = true;

	CMU_ClockEnable(cmuClock_ADC0,true);
	ADC_Init(ADC0, &cfg);
	ADC_InitSingle(ADC0, &seq);
	ADC_Start(ADC0, adcStartSingle);
}

/**
 * @brief analog input isr
 */
static void audio_adc_timer(TimerHandle_t *t)
{
	(void)t;
	uint16_t adc_data = (uint16_t)ADC_DataSingleGet(ADC0);
	audio_buffer.data[audio_buffer.active][adc_cnt] = adc_data;
	printf("audio_adc_timer: sample value: 0x%X\n\r\n\r", adc_data);

	adc_cnt++;
	if(adc_cnt == AUDIO_BUFFER_SIZE) {
		adc_cnt = 0;
		xSemaphoreGive(audio_signal);
		xTimerStop(t, 0);
		PTD_pinOutToggle(PTD_PORT_LED_ORANGE,PTD_PIN_LED_ORANGE);
	}

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
	data_env.audio_data = &audio_ext_buffer[0];

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
		data_env.audio_data = NULL;
		memcpy(&audio_ext_buffer, &audio_buffer.data[active], sizeof(audio_ext_buffer));
		data_env.audio_data = &audio_ext_buffer[0];
	}
}

/** public functions */




void audio_app_init(void)
{
	portBASE_TYPE err;

	err = xTaskCreate(audio_task,"audio_app",AUDIO_TASK_STK_SIZE,NULL,AUDIO_TASK_PRIO,NULL);
	assert(err == pdPASS);
}

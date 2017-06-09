/*
 * edge_temp_acq.c
 *
 *  Created on: 04/06/2017
 *      Author: root
 */

#include "../usr_include/beeinformed.h"

/* application task definitions */
#define TEMP_TASK_STK_SIZE			256
#define TEMP_TASK_PRIO				(configMAX_PRIORITIES - 3)


/** static variables */
static bool alarm_enabled = false;



/** static functions */


/**
 * @brief periodic temperature acquisition task
 */
static void temp_task(void *args)
{
	(void)args;
	printf("%s: started temperature applicaton task!! \n\r", __func__);

	for(;;) {

		int32_t temperature = 0;
		Retcode_T err = Environmental_readTemperature(xdkEnvironmental_BME280_Handle, &temperature);
		if(err != RETCODE_SUCCESS) {
			printf("%s: failed to get the temperature!! \n\r", __func__);
		} else {
			printf("%s: succes to get the temperature!! \n\r", __func__);
		}
		printf("%s: current temperature is: %d (mDeg)!! \n\r", __func__, temperature);

		/* update global structure */
		data_env.temperature = temperature;
		vTaskDelay(ACQUISITION_PERIOD);
	}

}


/** public functions */
void temp_app_init(void)
{
	portBASE_TYPE err;

	err = xTaskCreate(temp_task,"temp_app",TEMP_TASK_STK_SIZE,NULL,TEMP_TASK_PRIO,NULL);
	assert(err == pdPASS);
}

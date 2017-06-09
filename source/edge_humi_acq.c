/*
 * edge_humi_acq.c
 *
 *  Created on: 04/06/2017
 *      Author: root
 */

#include "../usr_include/beeinformed.h"

/* application task definitions */
#define HUMI_TASK_STK_SIZE			256
#define HUMI_TASK_PRIO				(configMAX_PRIORITIES - 3)


/** static variables */
static bool alarm_enabled = false;



/** static functions */


/**
 * @brief periodic humierature acquisition task
 */
static void humi_task(void *args)
{
	(void)args;
	printf("%s: started humierature applicaton task!! \n\r", __func__);

	for(;;) {

		uint32_t humidity = 0;
		Retcode_T err = Environmental_readHumidity(xdkEnvironmental_BME280_Handle, &humidity);
		if(err != RETCODE_SUCCESS) {
			printf("%s: failed to get the humidity!! \n\r", __func__);
		} else {
			printf("%s: succes to get the humidity!! \n\r", __func__);
		}
		printf("%s: current humidity is: %d (Percent)!! \n\r", __func__, humidity);

		/* update global structure */
		data_env.humidity = humidity;
		vTaskDelay(ACQUISITION_PERIOD);
	}

}


/** public functions */
void humi_app_start(void)
{
	portBASE_TYPE err;

	err = xTaskCreate(humi_task,"humi_app",HUMI_TASK_STK_SIZE,NULL,HUMI_TASK_PRIO,NULL);
	assert(err == pdPASS);
}

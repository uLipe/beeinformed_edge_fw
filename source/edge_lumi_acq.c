/*
 * edge_lumi_acq.c
 *
 *  Created on: 04/06/2017
 *      Author: root
 */
#include "../usr_include/beeinformed.h"

/* application task definitions */
#define LUMI_TASK_STK_SIZE			256
#define LUMI_TASK_PRIO				(configMAX_PRIORITIES - 3)


/** static variables */
static bool alarm_enabled = false;



/** static functions */


/**
 * @brief periodic pressure acquisition task
 */
static void lumi_task(void *args)
{
	(void)args;
	printf("%s: started luminosity applicaton task!! \n\r", __func__);

	for(;;) {

		uint32_t lumi = 0;
		Retcode_T err = LightSensor_readLuxData(xdkLightSensor_MAX44009_Handle, &lumi);
		if(err != RETCODE_SUCCESS) {
			printf("%s: failed to get the luminosity!! \n\r", __func__);
		} else {
			//printf("%s: succes to get the luminosity!! \n\r", __func__);
		}
		//printf("%s: current luminosity is: %d (mLux)!! \n\r", __func__, lumi);

		/* update global structure */
		data_env.luminosity = lumi;
		vTaskDelay(ACQUISITION_PERIOD);
	}

}


/** public functions */
void lumi_app_start(void)
{
	portBASE_TYPE err;

	err = xTaskCreate(lumi_task,"lumi_app",LUMI_TASK_STK_SIZE,NULL,LUMI_TASK_PRIO,NULL);
	assert(err == pdPASS);
}

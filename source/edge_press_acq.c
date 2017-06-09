/*
 * edge_press_acq.c
 *
 *  Created on: 04/06/2017
 *      Author: root
 */

#include "../usr_include/beeinformed.h"

/* application task definitions */
#define PRESS_TASK_STK_SIZE			256
#define PRESS_TASK_PRIO				(configMAX_PRIORITIES - 3)


/** static variables */
static bool alarm_enabled = false;



/** static functions */


/**
 * @brief periodic pressure acquisition task
 */
static void press_task(void *args)
{
	(void)args;
	printf("%s: started pressure applicaton task!! \n\r", __func__);

	for(;;) {

		uint32_t pressure = 0;
		Retcode_T err = Environmental_readPressure(xdkEnvironmental_BME280_Handle, &pressure);
		if(err != RETCODE_SUCCESS) {
			printf("%s: failed to get the pressure!! \n\r", __func__);
		} else {
			printf("%s: succes to get the pressure!! \n\r", __func__);
		}
		printf("%s: current pressure is: %d (Pa)!! \n\r", __func__, pressure);

		/* update global structure */
		data_env.pressure = pressure;
		vTaskDelay(ACQUISITION_PERIOD);
	}

}


/** public functions */
void pressure_app_start(void)
{
	portBASE_TYPE err;

	err = xTaskCreate(press_task,"press_app",PRESS_TASK_STK_SIZE,NULL,PRESS_TASK_PRIO,NULL);
	assert(err == pdPASS);
}

/*
 * edge_temp_acq.c
 *
 *  Created on: 04/06/2017
 *      Author: root
 */

#include "../usr_include/beeinformed.h"

/* application task definitions */
#define SENSOR_TASK_STK_SIZE			256
#define SENSOR_TASK_PRIO				(configMAX_PRIORITIES - 3)


/** private variables */
static QueueHandle_t sensor_request;
static bool sensors_rdy = false;


/** static functions */

/**
 * sensors_hw_init()
 * @brief inits sensor hw located on XDK board
 * @param
 * @reuturn
 */
static void sensors_hw_init(void)
{
    /* init env sensor hw */
    Retcode_T hw_err = Environmental_init(xdkEnvironmental_BME280_Handle);
    assert(hw_err == RETCODE_SUCCESS);
    hw_err = Environmental_setPowerMode(xdkEnvironmental_BME280_Handle,
    			ENVIRONMENTAL_BME280_POWERMODE_NORMAL);
    assert(hw_err == RETCODE_SUCCESS);

    /* inits the luminosity sensor infra structure */
    hw_err = LightSensor_init(xdkLightSensor_MAX44009_Handle);
    assert(hw_err == RETCODE_SUCCESS);
    hw_err = LightSensor_setContinuousMode(xdkLightSensor_MAX44009_Handle,
    			LIGHTSENSOR_MAX44009_ENABLE_MODE);
    assert(hw_err == RETCODE_SUCCESS);
}


/**
 * sensors_task()
 * @brief sensor main thread, responses requests for acquisition
 * @param
 * @reuturn
 */
static void sensors_task(void *args)
{
	(void)args;
	sensor_request = xQueueCreate(SENSORS_MAX_REQUESTS, sizeof(sensor_req_t));
	assert(sensor_request != NULL);
	sensors_hw_init();
	sensors_rdy = true;

	printf("%s: started sensor applicaton task!! \n\r", __func__);

	sensor_req_t req = {0};

	for(;;) {
		bool hw_error = false;

		/* wait for a request */
		xQueueReceive(sensor_request,&req, portMAX_DELAY);

		/* samples file needs to be valid */
		if(req.dat == NULL) {
			printf("%s: invalid data acquisition file! \n\r", __func__);
			if(req.cb != NULL) {
				req.cb(req.dat, ksensor_invalid_data, req.user_data);
			}
			continue;
		}


		/* perform hardware sensor reading
		 * stops and notifies requester at first error
		 */
		do{
			Retcode_T err = Environmental_readTemperature(xdkEnvironmental_BME280_Handle, &req.dat->temperature);
			if(err != RETCODE_SUCCESS) {
				printf("%s: failed to get the temperature!! \n\r", __func__);
				hw_error = true;
				break;
			} else {
				printf("%s: current temperature is: %lu (mDeg)!! \n\r", __func__, req.dat->temperature);
			}

			err = Environmental_readHumidity(xdkEnvironmental_BME280_Handle, &req.dat->humidity);
			if(err != RETCODE_SUCCESS) {
				printf("%s: failed to get the humidity!! \n\r", __func__);
				hw_error = true;
				break;

			} else {
				printf("%s: current humidity is: %lu (Percent)!! \n\r", __func__, req.dat->humidity);
			}


			err = LightSensor_readLuxData(xdkLightSensor_MAX44009_Handle, &req.dat->luminosity);
			if(err != RETCODE_SUCCESS) {
				printf("%s: failed to get the luminosity!! \n\r", __func__);
				hw_error = true;
				break;
			} else {
				printf("%s: current luminosity is: %lu (mLux)!! \n\r", __func__, req.dat->luminosity);
			}

			err = Environmental_readPressure(xdkEnvironmental_BME280_Handle, &req.dat->pressure);
			if(err != RETCODE_SUCCESS) {
				printf("%s: failed to get the pressure!! \n\r", __func__);
				hw_error = true;
				break;
			} else {
				printf("%s: current pressure is: %lu (Pa)!! \n\r", __func__, req.dat->pressure);
			}
		} while(0);


		/* check if readings was completed without error */
		if(hw_error) {
			if(req.cb != NULL) {
				req.cb(req.dat, ksensor_hw_error_retry, req.user_data);
			}
		} else {
			if(req.cb != NULL) {
				req.cb(req.dat, ksensor_ok, req.user_data);
			}

		}


	}

}


/** public functions */
void sensor_app_start(void)
{
	portBASE_TYPE err;
	err = xTaskCreate(sensors_task,"sensor_app",SENSOR_TASK_STK_SIZE,NULL,SENSOR_TASK_PRIO,NULL);
	assert(err == pdPASS);
}

bool sensor_trigger_reading(sensor_data_t *d, sensor_callback_t cb, void *user_data)
{
	bool ret = false;

	/* ignore if sensors is being initialized */
	if(!sensors_rdy)
		goto cleanup;

	if(d == NULL)
		goto cleanup;

	if(cb == NULL)
		goto cleanup;

	sensor_req_t req;
	req.dat = d;
	req.cb  = cb;
	req.user_data = user_data;

	if(xQueueSend(sensor_request, &req, 0) != pdPASS) {
		if(cb) {
			cb(d , ksensor_unk_error, user_data);
		}
	} else {
		ret = true;
	}

cleanup:
	return(ret);
}

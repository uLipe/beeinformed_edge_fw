/*
 * edge_humi_acq.h
 *
 *  Created on: 04/06/2017
 *      Author: root
 */
#ifndef __EDGE_SENSORS_H
#define __EDGE_SENSORS_H

/* beeinformed sensors range */
#define MAX_HUMI 	100
#define MIN_HUMI	0
#define MAX_LUMI	10000
#define MIN_LUMI	1
#define MAX_TEMP 	48000
#define MIN_TEMP  	3000

/* maximum sequencial sensor acquisitions */
#define SENSORS_MAX_REQUESTS 32


/* sensor status types */
typedef enum {
	ksensor_ok = 1,
	ksensor_invalid_data,
	ksensor_hw_error_retry,
	ksensor_unk_error,
}sensor_status_t;

/* sensor data acquisition structure */
typedef struct {
	int32_t  temperature;
	uint32_t pressure;
	uint32_t luminosity;
	uint32_t humidity;
}sensor_data_t;


/* sensor acquired callback */
typedef void (*sensor_callback_t) (sensor_data_t *dat, sensor_status_t s,void *user_data);


/* sensor data acquisition structure */
typedef struct {
	/* data payload */
	sensor_data_t *dat;

	/* acquisition complete callback */
	sensor_callback_t cb;

	/* rtc timestamp for future use */
	uint32_t rtc_val;

	/* custom user data */
	void *user_data;
}sensor_req_t;

/**
 * sensor_app_start()
 * @brief starst sensor acquisition engine
 * @param
 * @reuturn
 */
void sensor_app_start(void);


/**
 * sensor_trigger_reading()
 * @brief triggers the sensor readings appending a request
 * @param
 * @reuturn
 */
bool sensor_trigger_reading(sensor_data_t *d, sensor_callback_t cb, void *user_data);

#endif /* SOURCE_EDGE_HUMI_ACQ_H_ */

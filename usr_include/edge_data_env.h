/*
 * edge_data_env.h
 *
 *  Created on: 04/06/2017
 *      Author: root
 */

#ifndef USR_INCLUDE_EDGE_DATA_ENV_H_
#define USR_INCLUDE_EDGE_DATA_ENV_H_


/* acquisition time period */
#define ACQUISITION_SECOND	(1000)
#define ACQUISITION_PERIOD	(ACQUISITION_SECOND * 60)

/* event masks */
#define EVENT_TEMP_LOW			0x01
#define EVENT_TEMP_HIGH			0x02
#define EVENT_PRESS_LOW			0x04
#define EVENT_PRESS_HIGH		0x08
#define EVENT_LUMI_LOW			0x10
#define EVENT_LUMI_HIGH			0x20
#define EVENT_HUMI_LOW			0x40
#define EVENT_HUMI_HIGH			0x80




/**
 * Defines the data conversion environment
 */
typedef struct {
	uint32_t acquisition_period;
	int32_t temperature;
	uint32_t pressure;
	uint32_t luminosity;
	uint32_t humidity;
	uint32_t event_mask;
	uint16_t *audio_data;
}data_env_t;


extern data_env_t data_env;


#endif /* USR_INCLUDE_EDGE_DATA_ENV_H_ */

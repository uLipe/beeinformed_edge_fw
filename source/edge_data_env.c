/*
 * edge_data_env.c
 *
 *  Created on: 04/06/2017
 *      Author: root
 */
#include "../usr_include/beeinformed.h"


/* set a instance of global data holder */
data_env_t data_env = {
		.acquisition_period = ACQUISITION_PERIOD,
		.temperature = 2500,
		.pressure	 = 76,
		.humidity	 = 500,
		.audio_data  = NULL,
		.luminosity  = 0,
};



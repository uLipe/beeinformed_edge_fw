/*
 * edge_audio_acq.h
 *
 *  Created on: 04/06/2017
 *      Author: root
 */

#ifndef SOURCE_EDGE_AUDIO_ACQ_H_
#define SOURCE_EDGE_AUDIO_ACQ_H_

/* audio buffer size in samples */
#define AUDIO_BUFFER_SIZE  500

/* audio buffer sample rate ticks */
#define AUDIO_SAMPLE_RATE  2

/* audio ping-pongbuffer structure */
typedef struct {
	uint16_t data[2][AUDIO_BUFFER_SIZE];
	uint8_t  active;
}ppbuf_t;


/**
 * @brief init audio application
 */
void audio_app_init(void);


#endif /* SOURCE_EDGE_AUDIO_ACQ_H_ */

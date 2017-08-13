/*
 * edge_audio_acq.h
 *
 *  Created on: 04/06/2017
 *      Author: root
 */

#ifndef SOURCE_EDGE_AUDIO_ACQ_H_
#define SOURCE_EDGE_AUDIO_ACQ_H_

/* audio buffer size in samples */
#define AUDIO_BUFFER_SIZE  512

/* audio buffer sample in Hz */
#define AUDIO_SAMPLE_RATE  8000

/* maximum sequencial records */
#define AUDIO_MAX_RECORDS  32

/* audio ping-pongbuffer structure */
typedef struct {
	uint16_t data[2][AUDIO_BUFFER_SIZE];
	uint8_t  active;
}ppbuf_t;

typedef enum {
	kaudio_acquisition_ok = 1,
	kaudio_sample_invalid,
	kaudio_sec_invalid,
	kaudio_unk_error
}audio_status_t;

/* audio acquisition complete callback */
typedef void (*audio_complete_callback_t) (void *user_data, audio_status_t);


/* audio 1-second request */
typedef struct {
	/* file where audio is deposited */
	uint16_t *samples;

	/* block size */
	uint16_t noof_blocks;

	/* how much seconds */
	uint16_t seconds;

	/* custom callback */
	audio_complete_callback_t cb;

	/* user_custom data */
	void *user_data;

}audio_request_t;


/**
 * @brief init audio application
 */
void audio_app_init(void);

/**
 * @brief trigger audio recording
 */
bool audio_start_record(uint16_t *outfile, uint16_t seconds, audio_complete_callback_t cb, void *user_data);

#endif /* SOURCE_EDGE_AUDIO_ACQ_H_ */

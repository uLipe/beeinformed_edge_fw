/*
 *  @file edge_audio_processing.h
 *  @brief module responsible to perform some DSP on the acquired audio
 */

#ifndef __EDGE_AUDIO_PROCESSING_H
#define __EDGE_AUDIO_PROCESSING_H

/* number of points computed by the FFT */
#define DSP_FFT_POINTS	256

/* Bee audio RAW spectra */
typedef struct audio_spectra{
	/* raw audio acquisition */
	uint32_t spectral_sample_rate;
	uint32_t spectral_points;
	int16_t raw[DSP_FFT_POINTS];

	/* bee specifics levels */
	int16_t hissing_level;
	int16_t swarm_level;
	int16_t working_level;
}audio_spectra_t;


/**
 * 	@fn audio_dsp_process_samples()
 *  @brief process the audio acquired and return its spectra
 *
 *  @param
 *  @return
 */
int audio_dsp_process_samples(uint16_t *raw, uint32_t raw_size, uint32_t sample_rate , audio_spectra_t *spec);


#endif

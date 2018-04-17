#ifndef Haas_h
#define Haas_h

#define _GNU_SOURCE

#include <alsa/asoundlib.h>
#include "../VSEffect.h"

#include "DelayS.h"

typedef struct
{
	snd_pcm_format_t format; // SND_PCM_FORMAT_S16
	unsigned int rate; // sampling rate
	unsigned int channels; // channels

	int enabled;
	float millisec;
	int initialized;
	int physicalwidth, insamples;
	sounddelay haasdly;
}soundhaas;

void soundhaas_reinit(int enabled, float millisec, soundhaas *h);
void soundhaas_init(int enabled, float millisec, snd_pcm_format_t format, unsigned int rate, unsigned int channels, soundhaas *h);
void soundhaas_add(char* inbuffer, int inbuffersize, soundhaas *h);
void soundhaas_close(soundhaas *h);

void aef_init(audioeffect *ae);
void aef_setparameter(audioeffect *ae, int i, float value);
float aef_getparameter(audioeffect *ae, int i);
void aef_process(audioeffect *ae, uint8_t* inbuffer, int inbuffersize);
void aef_reinit(audioeffect *ae);
void aef_close(audioeffect *ae);

#endif

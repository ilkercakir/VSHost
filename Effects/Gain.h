#ifndef Gain_h
#define Gain_h

#define _GNU_SOURCE

#include "../VSEffectShared.h"

typedef struct
{
	snd_pcm_format_t format; // SND_PCM_FORMAT_S16
	unsigned int rate; // sampling rate
	unsigned int channels; // channels
	float mingain, maxgain, gainvalue;

	int initialized;
	int physicalwidth, insamples;
}soundgain;

typedef struct
{
	snd_pcm_format_t format;
	unsigned int rate;
	unsigned int channels;
	soundgain g;
}gain;

void soundgain_reinit(float mingain, float maxgain, float gainvalue, soundgain *g);
void soundgain_init(float mingain, float maxgain, float gainvalue, snd_pcm_format_t format, unsigned int rate, unsigned int channels, soundgain *g);
void soundgain_add(char* inbuffer, int inbuffersize, soundgain *g);
void soundgain_close(soundgain *s);

void aef_init(audioeffect *ae);
void aef_setparameter(audioeffect *ae, int i, float value);
float aef_getparameter(audioeffect *ae, int i);
void aef_process(audioeffect *ae, uint8_t* inbuffer, int inbuffersize);
void aef_reinit(audioeffect *ae);
void aef_close(audioeffect *ae);

#endif

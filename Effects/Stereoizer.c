/*
 * Stereoizer.c
 * 
 * Copyright 2018  <pi@raspberrypi>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#include "Stereoizer.h"

void aef_init(audioeffect *ae)
{
/* User defined parameter initialization code begin */

	soundmod *m = ae->data = malloc(sizeof(soundmod));

	strcpy(ae->name, "Stereo");
	audioeffect_allocateparameters(ae, 7);
	audioeffect_initparameter(ae,  0, "Enable", 0.0, 1.0, 0.0, 1.0, 1, pt_switch);
	audioeffect_initparameter(ae,  1, "Delay", 1.0, 30.0, 15.0, 1.0, 1, pt_scale);
	audioeffect_initparameter(ae,  2, "Modulation", 0.0, 1.0, 0.0, 1.0, 1, pt_switch);
	audioeffect_initparameter(ae,  3, "L Rate Hz", 0.1, 4.0, 0.3, 0.1, 1, pt_scale);
	audioeffect_initparameter(ae,  4, "L Depth %", 0.1, 10.0, 0.4, 0.1, 1, pt_scale);
	audioeffect_initparameter(ae,  5, "R Rate Hz", 0.1, 4.0, 0.3, 0.1, 1, pt_scale);
	audioeffect_initparameter(ae,  6, "R Depth %", 0.1, 10.0, 0.4, 0.1, 1, pt_scale);

	soundmod_init(ae->parameter[1].value, ae->parameter[2].value, (int)ae->parameter[0].value, ae->format, ae->rate, ae->channels, m);
//printf("%f, %f, %f, %f\n", aef_getparameter(ae, 0), aef_getparameter(ae, 1), aef_getparameter(ae, 2), aef_getparameter(ae, 3));

/* User defined parameter initialization code end */
}

void aef_setparameter(audioeffect *ae, int i, float value)
{
	ae->parameter[i].value = value;

/* User defined parameter setter code begin */

	soundmod *m = (soundmod *)ae->data;

	switch(i)
	{
		case 0: // Enable
			m->enabled = (int)ae->parameter[i].value;
			break;
		case 1:
			m->modfreq = ae->parameter[i].value;
			break;
		case 2:
			m->moddepth = ae->parameter[i].value;
			break;
	}
//printf("aef_setparameter %d = %2.2f\n", i, ae->parameter[i].value);

/* User defined parameter setter code end */

	if (ae->parameter[i].resetrequired)
		aef_reinit(ae);
}

float aef_getparameter(audioeffect *ae, int i)
{
	return ae->parameter[i].value;
}

void aef_process(audioeffect *ae, uint8_t* inbuffer, int inbuffersize)
{
/* User defined processing code begin */
 
	soundmod *m = (soundmod *)ae->data;

	soundmod_add((char*)inbuffer, inbuffersize, m);

/* User defined processing code end */
}

void aef_reinit(audioeffect *ae)
{
/* User defined reinitialization code begin */

	soundmod *m = (soundmod *)ae->data;

	soundmod_reinit(ae->parameter[1].value, ae->parameter[2].value, (int)ae->parameter[0].value, m);
//printf("%f, %f, %f, %f\n", aef_getparameter(ae, 0), aef_getparameter(ae, 1), aef_getparameter(ae, 2), aef_getparameter(ae, 3));

/* User defined reinitialization code end */
}

void aef_close(audioeffect *ae)
{
	audioeffect_deallocateparameters(ae);

/* User defined cleanup code begin  */

	soundmod *m = (soundmod *)ae->data;

	soundmod_close(m);

	free(ae->data);

/* User defined cleanup code end */
}

// Variable Frequency Oscillator Processor in VFO.c

// Modulator

void soundmod_reinit(float modfreq, float moddepth, int enabled, soundmod *m)
{
	soundvfo_close(&(m->v));
	m->modfreq = modfreq;
	m->moddepth = moddepth;
	m->enabled = enabled;
	m->v.enabled = 1;

	soundvfo_init(m->modfreq, m->moddepth/100.0, m->format, m->rate, m->channels, &(m->v));
}

void soundmod_init(float modfreq, float moddepth, int enabled, snd_pcm_format_t format, unsigned int rate, unsigned int channels, soundmod *m)
{
	m->format = format;
	m->rate = rate;
	m->channels = channels;
	m->modfreq = modfreq;
	m->moddepth = moddepth;
	m->enabled = enabled;
	m->v.enabled = 1;

	soundvfo_init(m->modfreq, m->moddepth/100.0, m->format, m->rate, m->channels, &(m->v));
}

void soundmod_add(char* inbuffer, int inbuffersize, soundmod *m)
{
	int i, j;
	signed short *inshort, *vfshort;

	if (m->enabled)
	{
		soundvfo_add(inbuffer, inbuffersize, &(m->v));
		inshort = (signed short *)inbuffer;
		vfshort = (signed short *)m->v.vfobuf;
		for(i=0;i<m->v.inbufferframes;i++)
		{
			for(j=0;j<m->channels;j++)
				inshort[i*m->channels+j] = vfshort[m->v.front++];
			m->v.front %= m->v.vfobufsamples;
		}
	}
}

void soundmod_close(soundmod *m)
{
	soundvfo_close(&(m->v));
}

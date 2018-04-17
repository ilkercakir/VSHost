/*
 * Haas.c
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

#include "Haas.h"

void aef_init(audioeffect *ae)
{
/* User defined parameter initialization code begin */

	strcpy(ae->name, "Haas");
	audioeffect_allocateparameters(ae, 2);
	audioeffect_initparameter(ae, 0, "Enable", 0.0, 1.0, 0.0, 1.0, 1, pt_switch);
	audioeffect_initparameter(ae, 1, "Delay ms", 1.0, 30.0, 15.0, 0.1, 1, pt_scale);

	soundhaas *h = ae->data = malloc(sizeof(soundhaas));

	soundhaas_init(ae->parameter[0].value, ae->parameter[1].value, ae->format, ae->rate, ae->channels, h);
//printf("%f, %f, %f, %f\n", aef_getparameter(ae, 0), aef_getparameter(ae, 1), aef_getparameter(ae, 2), aef_getparameter(ae, 3));

/* User defined parameter initialization code end */
}

void aef_setparameter(audioeffect *ae, int i, float value)
{
	ae->parameter[i].value = value;

/* User defined parameter setter code begin */

	soundhaas *h = (soundhaas *)ae->data;

	switch(i)
	{
		case 0: // Enable
			h->enabled = ae->parameter[i].value;
			break;
		case 1: // Delay
			h->millisec = ae->parameter[i].value;
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
 
	soundhaas *h = (soundhaas *)ae->data;

	soundhaas_add((char*)inbuffer, inbuffersize, h);

/* User defined processing code end */
}

void aef_reinit(audioeffect *ae)
{
/* User defined reinitialization code begin */

	soundhaas *h = (soundhaas *)ae->data;

	soundhaas_reinit(ae->parameter[0].value, ae->parameter[1].value, h);
//printf("%f, %f, %f, %f\n", aef_getparameter(ae, 0), aef_getparameter(ae, 1), aef_getparameter(ae, 2), aef_getparameter(ae, 3));

/* User defined reinitialization code end */
}

void aef_close(audioeffect *ae)
{
	audioeffect_deallocateparameters(ae);

/* User defined cleanup code begin  */

	soundhaas *h = (soundhaas *)ae->data;

	soundhaas_close(h);

	free(ae->data);

/* User defined cleanup code end */
}

// Haas Effect Processor, Delay Processor in DelayS.c

void soundhaas_reinit(int enabled, float millisec, soundhaas *h)
{
	h->millisec = millisec;
	h->initialized = 0;
	h->enabled = enabled;
	sounddelay_reinit(1, DLY_LATE, h->millisec, 1.0, &(h->haasdly));
}

void soundhaas_init(int enabled, float millisec, snd_pcm_format_t format, unsigned int rate, unsigned int channels, soundhaas *h)
{
	h->format = format;
	h->rate = rate;
	h->channels = channels;

	h->millisec = millisec;
	h->initialized = 0;
	h->enabled = enabled;
	sounddelay_init(1, DLY_LATE, h->millisec, 1.0, h->format, h->rate, h->channels, &(h->haasdly));
}

void soundhaas_add(char* inbuffer, int inbuffersize, soundhaas *h)
{
	if (h->enabled)
	{
		if (!h->initialized)
		{
			h->physicalwidth = snd_pcm_format_width(h->format); // bits per sample
			h->insamples = inbuffersize / h->physicalwidth * 8;
			h->initialized = 1;
		}
		sounddelay_add(inbuffer, inbuffersize, &(h->haasdly));
		signed short *inshort = (signed short *)inbuffer;
		signed short *fbuffer = h->haasdly.fshort;
		int i;
		for(i=0;i<h->insamples;)
		{
			inshort[i++] *= 0.7; h->haasdly.readfront++; // rescale left channel
			inshort[i++] = fbuffer[h->haasdly.readfront++]; // Haas effect on right channel
			h->haasdly.readfront%=h->haasdly.fbuffersamples;
		}
	}
}

void soundhaas_close(soundhaas *h)
{
	sounddelay_close(&(h->haasdly));
}

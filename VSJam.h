#ifndef VSJam_h
#define VSJam_h

#define _GNU_SOURCE

#include <stdint.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <sqlite3.h>

#include "VSEffect.h"
#include "AudioDev.h"
#include "AudioSpk.h"
#include "AudioMixer.h"

typedef struct
{
	int id;
	char name[40];
	snd_pcm_format_t format; // SND_PCM_FORMAT_S16
	unsigned int rate; // sampling rate
	unsigned int channels; // channels
	unsigned int frames;

	char dbpath[256];
	int maxchains;
	int maxeffects;
	GtkWidget *container;
	GtkWidget *frame;
	GtkWidget *hbox;

	GtkWidget *toolbarhbox;
	GtkWidget *toolbar;
	GtkWidget *icon_widget;
	GtkToolItem *addchainbutton;
	GtkWidget *chainnameentry;
	GtkToolItem *savechainsbutton;

	audioeffectchain *aec;
	audiomixer *mx;

	int aeci;
}audiojam;

typedef struct auout audioout;

typedef struct
{
	audioout *ao;
	char device[32];
	unsigned int frames;

	speaker spk;

	cpu_set_t cpu[4];
	pthread_t tid;
	int retval;
}mxthreadparameters;

struct auout
{
	char name[40];
	snd_pcm_format_t format; // SND_PCM_FORMAT_S16
	unsigned int rate; // sampling rate
	unsigned int channels; // channels
	unsigned int frames;

	GtkWidget *container;
	GtkWidget *outputframe;
	GtkWidget *outputhbox;
	GtkWidget *outputdevices;
	GtkWidget *led;
	GtkWidget *frameslabel;
	GtkWidget *spinbutton;

	int mixerChannels;
	audiomixer mx;
	audiojam *aj;
	mxthreadparameters tp;

	pthread_mutex_t mxmutex;
	pthread_cond_t mxinitcond;
	int mxready;
};

void audioout_init(audioout *ao, snd_pcm_format_t format, unsigned int rate, unsigned int channels, unsigned int frames, int mixerChannels, audiojam *aj, GtkWidget *container);
void audioout_close(audioout *ao);

void audiojam_init(audiojam *aj, int maxchains, int maxeffects, snd_pcm_format_t format, unsigned int rate, unsigned int channels, unsigned int frames, GtkWidget *container, char *dbpath, audiomixer *mx);
void audiojam_addchain(audiojam *aj, char *name);
void audiojam_close(audiojam *aj);
void audiojam_stopthreads(audiojam *aj);
void audiojam_startthreads(audiojam *aj);

#endif

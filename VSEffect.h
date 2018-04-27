#ifndef VSEffect_h
#define VSEffect_h

#define _GNU_SOURCE

#include <ctype.h>
#include <stdint.h>
#include <pthread.h>
#include <dlfcn.h>
#include <alsa/asoundlib.h>
#include <gtk/gtk.h>
#include <sqlite3.h>

#include "AudioMic.h"
#include "AudioMixer.h"
#include "VSTMediaPlayer.h"

/*
enum
{
  COL_ID = 0,
  COL_FILEPATH,
  NUM_COLS
};
*/

typedef enum
{
	pt_none,
	pt_switch,
	pt_scale,
	pt_check,
	pt_spin,
	pt_combo,
}parametertype;

typedef struct
{
	void *parent;
	int index;
	char name[10];
	float minval, maxval, value, step;
	int resetrequired;
	parametertype ptype;
	GtkWidget *vbox;
	GtkWidget *pwidget;
	GtkWidget *label;
	GtkAdjustment *adj;
	char confpath[256];
}audioeffectparameter;

typedef struct audioeffchain audioeffectchain;

typedef struct audioeff audioeffect;

struct audioeff
{
	int id;
	char sopath[256];
	void *handle;

	char name[40];
	snd_pcm_format_t format; // SND_PCM_FORMAT_S16
	unsigned int rate; // sampling rate
	unsigned int channels; // channels

	audioeffectchain *parent;

	int parameters;
	audioeffectparameter *parameter;
	pthread_mutex_t effectmutex;
	GtkWidget *container;
	GtkWidget *frame;
	GtkWidget *hbox;

	GtkWidget *vtoolbox;
	GtkWidget *toolbar;
	GtkWidget *icon_widget;
	GtkToolItem *removeeffectbutton;

	void (*aef_init)(audioeffect*); // init
	void (*aef_setparameter)(audioeffect*, int, float); // setparameter
	float (*aef_getparameter)(audioeffect*, int); // getparameter
	void (*aef_process)(audioeffect*, uint8_t*, int); // process
	void (*aef_reinit)(audioeffect*); // reinit
	void (*aef_close)(audioeffect*); // close

	void *data;
};

typedef struct
{
	audioeffectchain *aec;
	char device[32];
	unsigned int frames;

	audiojack jack;
	int channelbuffers;
	
	microphone mic;
	vpwidgets vpw;
	playlistparams plparams;

	cpu_set_t cpu;
	pthread_t tid;
	int retval;
}threadparameters;

typedef struct
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *toolbar;
	GtkWidget *icon_widget;
	GtkToolItem *addbutton;
	GtkToolItem *deletebutton;
	GtkWidget *scrolled_window;
	GtkWidget *view;
	GtkListStore *store;
	GtkTreeIter iter;
}audioeffectchainutil;

struct audioeffchain
{
	int id;
	char name[40];
	snd_pcm_format_t format; // SND_PCM_FORMAT_S16
	unsigned int rate; // sampling rate
	unsigned int channels; // channels
	unsigned int frames;

	pthread_mutex_t rackmutex;
	int effects;
	GtkWidget *container;
	GtkWidget *inputframe;
	GtkWidget *inputdevices;
	GtkWidget *inputhbox;
	GtkWidget *led;

	GtkWidget *toolbar;
	GtkWidget *icon_widget;
	GtkToolItem *addeffectbutton;
	GtkToolItem *saveeffectsbutton;
	GtkToolItem *deletechainbutton;

	GtkWidget *frame;
	GtkWidget *vbox;

	audioeffect *ae;
	threadparameters tp;
	audiomixer *mx;

	char dbpath[256];
	audioeffectchainutil util;
	int effid;

	int channelbuffers;
};

typedef struct
{
	audioeffect *ae;
	int i;
	float value;
}audioeffectidle;

typedef enum
{
	none,
	hardwaredevice,
	mediafiledevice
}devicetype;

void audioeffect_init(audioeffect *ae, int id);
void audioeffect_allocateparameters(audioeffect *ae, int count);
void audioeffect_initparameter(audioeffect *ae, int i, char* name, float minval, float maxval, float value, float step, int resetrequired, parametertype ptype);
void audioeffect_setparameter(audioeffect *ae, int i, float value);
float audioeffect_getparameter(audioeffect *ae, int i);
void audioeffect_process(audioeffect *ae, uint8_t* inbuffer, int inbuffersize);
void audioeffect_deallocateparameters(audioeffect *ae);
void audioeffect_close(audioeffect *ae);
void audioeffect_setdependentparameter(audioeffect *ae, int i, float value);

void audioeffectchain_create_thread(audioeffectchain *aec, char *device, unsigned int frames, int channelbuffers, audiomixer *mx);
void audioeffectchain_terminate_thread(audioeffectchain *aec);
void audioeffectchain_terminate_thread_ffmpeg(audioeffectchain *aec);
void audioeffectchain_save(audioeffectchain *aec);
void audioeffectchain_init(audioeffectchain *aec, char *name, int id, audiomixer *mx, GtkWidget *container, char *dbpath);
int audioeffectchain_loadeffect(audioeffectchain *aec, int id, char *path);
void audioeffectchain_process(audioeffectchain *aec, char *inbuffer, int inbuffersize);
void audioeffectchain_unloadeffect(audioeffectchain *aec, int effect);
void audioeffectchain_close(audioeffectchain *aec);

devicetype get_devicetype(char *device);
#endif

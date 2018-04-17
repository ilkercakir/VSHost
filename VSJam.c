/*
 * VSJam.c
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

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>

#include "VSJam.h"

// Audio Out

// thread
gboolean audioout_led(gpointer data)
{
	audioout *ao = (audioout *)data;

	if (ao->mx.mixerstatus == MX_RUNNING)
		gtk_image_set_from_file(GTK_IMAGE(ao->led), "./images/green.png");
	else
		gtk_image_set_from_file(GTK_IMAGE(ao->led), "./images/red.png");

	return FALSE;
}

static gpointer audioout_thread0(gpointer args)
{
	int ctype = PTHREAD_CANCEL_ASYNCHRONOUS;
	int ctype_old;
	pthread_setcanceltype(ctype, &ctype_old);

	audioout *ao = (audioout *)args;
	mxthreadparameters *tp = (mxthreadparameters *)&(ao->tp);
	speaker *spk = &(tp->spk);

	init_audiomixer(ao->mixerChannels, MX_BLOCKING, ao->format, ao->rate, ao->frames, ao->channels, &(ao->mx));

	pthread_mutex_lock(&(ao->mxmutex));
	ao->mxready = 1;
	pthread_cond_signal(&(ao->mxinitcond));
	pthread_mutex_unlock(&(ao->mxmutex));

	init_spk(spk, tp->device, ao->format, ao->rate, ao->channels);

	int err;
	if ((err=init_audio_hw_spk(spk)))
	{
		printf("Init spk error %d\n", err);
	}
	else
	{
		gdk_threads_add_idle(audioout_led, ao);
		while (readfrommixer(&(ao->mx)) != MX_STOPPED)
		{
			// process mixed stereo frames here

			write_spk(spk, ao->mx.outbuffer, ao->mx.outbufferframes);
		}
		close_audio_hw_spk(spk);
	}
	close_spk(spk);
	close_audiomixer(&(ao->mx));

//printf("exiting %s\n", ao->name);
	ao->tp.retval = 0;
	pthread_exit(&(ao->tp.retval));
}

void audioout_create_thread(audioout *ao, char *device, unsigned int frames)
{
	int err, ret;

	ao->tp.ao = ao;
	strcpy(ao->tp.device, device);
	ao->tp.frames = frames;

	if ((ret=pthread_mutex_init(&(ao->mxmutex), NULL))!=0)
		printf("audio out mxmutex init failed, %d\n", ret);

	if ((ret=pthread_cond_init(&(ao->mxinitcond), NULL))!=0 )
		printf("audio aout mxinitcond init failed, %d\n", ret);

	ao->mxready = 0;

	err = pthread_create(&(ao->tp.tid), NULL, &audioout_thread0, (void*)ao);
	if (err)
	{}
//printf("thread %s\n", ao->name);
	CPU_ZERO(&(ao->tp.cpu[1]));
	CPU_SET(1, &(ao->tp.cpu[1]));
	if ((err=pthread_setaffinity_np(ao->tp.tid, sizeof(cpu_set_t), &(ao->tp.cpu[1]))))
	{
		//printf("pthread_setaffinity_np error %d\n", err);
	}

	pthread_mutex_lock(&(ao->mxmutex));
	while (!ao->mxready)
	{
		pthread_cond_wait(&(ao->mxinitcond), &(ao->mxmutex));
	}
	pthread_mutex_unlock(&(ao->mxmutex));

	pthread_cond_destroy(&(ao->mxinitcond));
	pthread_mutex_destroy(&(ao->mxmutex));
}

void audioout_terminate_thread(audioout *ao)
{
	int i;

	signalstop_audiomixer(&(ao->mx));

	gdk_threads_add_idle(audioout_led, ao);

	if (ao->tp.tid)
	{
		if ((i=pthread_join(ao->tp.tid, NULL)))
			printf("pthread_join error, %s, %d\n", ao->name, i);
		ao->tp.tid = 0;
	}
}

static void outputdevicescombo_changed(GtkWidget *combo, gpointer data)
{
	audioout *ao = (audioout *)data;
	audiojam *aj = ao->aj;
	audioeffectchain *aec;
	int i;

	for(i=0;i<aj->maxchains;i++)
	{
		aec = &(aj->aec[i]);
		if (aec->id)
			audioeffectchain_terminate_thread(aec);
	}

	audioout_terminate_thread(ao);

	gchar *device;
	g_object_get((gpointer)ao->outputdevices, "active-id", &device, NULL);
	audioout_create_thread(ao, device, ao->frames);
	g_free(device);

	for(i=0;i<aj->maxchains;i++)
	{
		aec = &(aj->aec[i]);
		if (aec->id)
		{
			gchar *device;
			g_object_get((gpointer)aec->inputdevices, "active-id", &device, NULL);
			audioeffectchain_create_thread(aec, device, aec->frames, aec->channelbuffers, aec->mx);
			g_free(device);
		}
	}

}

static void frames_changed(GtkWidget *widget, gpointer data)
{
	int frames = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget));
	audioout *ao = (audioout *)data;
	audiojam *aj = ao->aj;

	audiojam_close(aj);
	gtk_widget_destroy(aj->toolbarhbox);
	gtk_widget_destroy(aj->frame);
	audioout_terminate_thread(ao);

	ao->frames = frames;

	audioout_create_thread(ao, ao->tp.device, ao->frames);
	audiojam_init(aj, aj->maxchains, aj->maxeffects, aj->format, aj->rate, aj->channels, ao->frames, aj->container, aj->dbpath, &(ao->mx));
	gtk_widget_show_all(aj->toolbarhbox);
	gtk_widget_show_all(aj->frame);

}

void audioout_init(audioout *ao, snd_pcm_format_t format, unsigned int rate, unsigned int channels, unsigned int frames, int mixerChannels, audiojam *aj, GtkWidget *container)
{
	strcpy(ao->name, "Audio Mixer");
	ao->format = format;
	ao->rate = rate;
	ao->channels = channels;
	ao->frames = frames;
	ao->mixerChannels = mixerChannels;
	ao->aj = aj;
	ao->tp.tid = 0;

	ao->container = container;

// frame
	ao->outputframe = gtk_frame_new("Audio Mixer");
	gtk_container_add(GTK_CONTAINER(ao->container), ao->outputframe);
	//gtk_box_pack_start(GTK_BOX(ao->container), ao->outputframe, TRUE, TRUE, 0);

// horizontal box
	ao->outputhbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add(GTK_CONTAINER(ao->outputframe), ao->outputhbox);
	//gtk_box_pack_start(GTK_BOX(ae->outputframe), ao->outputhbox, TRUE, TRUE, 0);

// add output devices combo
	ao->outputdevices = gtk_combo_box_text_new();
	populate_output_devices_list(ao->outputdevices);
	g_signal_connect(GTK_COMBO_BOX(ao->outputdevices), "changed", G_CALLBACK(outputdevicescombo_changed), ao);
	gtk_container_add(GTK_CONTAINER(ao->outputhbox), ao->outputdevices);
	//gtk_box_pack_start(GTK_BOX(ae->outputhbox), ao->outputdevices, TRUE, TRUE, 0);

	ao->led = gtk_image_new();
	gtk_image_set_from_file(GTK_IMAGE(ao->led), "./images/red.png");
	gtk_container_add(GTK_CONTAINER(ao->outputhbox), ao->led);

// frames
	ao->frameslabel = gtk_label_new("Frames");
	gtk_container_add(GTK_CONTAINER(ao->outputhbox), ao->frameslabel);
	ao->spinbutton = gtk_spin_button_new_with_range(32.0, 1024.0 , 1.0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ao->spinbutton), (float)ao->frames);
	g_signal_connect(GTK_SPIN_BUTTON(ao->spinbutton), "value-changed", G_CALLBACK(frames_changed), ao);
	gtk_container_add(GTK_CONTAINER(ao->outputhbox), ao->spinbutton);

	gchar *device;
	g_object_get((gpointer)ao->outputdevices, "active-id", &device, NULL);
	audioout_create_thread(ao, device, ao->frames);
	g_free(device);
}

void audioout_close(audioout *ao)
{
	audioout_terminate_thread(ao);
}

// Audio Effect Chains

void addchainbutton_clicked(GtkToolButton *toolbutton, gpointer data)
{
	audiojam *aj = (audiojam*)data;

	const gchar *text = gtk_entry_get_text(GTK_ENTRY(aj->chainnameentry));
	audiojam_addchain(aj, (char *)text);
//    g_print("Button clicked\n");
}

void savechainsbutton_clicked(GtkToolButton *toolbutton, gpointer data)
{
	audiojam *aj = (audiojam*)data;

	for(aj->aeci=0;aj->aeci<aj->maxchains;aj->aeci++)
	{
		if (!aj->aec[aj->aeci].id)
			continue;
		audioeffectchain_save(&(aj->aec[aj->aeci]));
	}

//    g_print("Button clicked\n");
}

int audiojam_selectchains_callback(void *data, int argc, char **argv, char **azColName) 
{
	audiojam *aj = (audiojam*)data;

	audioeffectchain_init(&(aj->aec[aj->aeci]), argv[1], atoi((const char *)argv[0]), aj->mx, aj->hbox, aj->dbpath);
	aj->aeci++;

	return 0;
}

int audiojam_selectinputs_callback(void *data, int argc, char **argv, char **azColName) 
{
	audiojam *aj = (audiojam*)data;
	int i, id = atoi((const char *)argv[0]);

	for(i=0;i<aj->maxchains;i++)
	{
		if (aj->aec[i].id == id)
		{
			g_object_set((gpointer)aj->aec[i].inputdevices, "active-id", argv[1], NULL);
//printf("set %d to %s\n", i, argv[1]);
			break;
		}
	}

	return 0;
}

int audiojam_select_audioeffects_callback(void *data, int argc, char **argv, char **azColName) 
{
	int id, effid;
	audiojam *aj = (audiojam*)data;

	id = atoi((const char *)argv[0]);
	if ((effid = audioeffectchain_loadeffect(&(aj->aec[aj->aeci]), id, argv[1]))>=0)
	{}
	else
		printf("audioeffectchain_loadeffect error %d\n", effid);

	return 0;
}

void audiojam_loadfromdb(audiojam *aj)
{
	int i;
	sqlite3 *db;
	char *err_msg = NULL;
	char sql[200];
	int rc;

	if ((rc = sqlite3_open(aj->dbpath, &db)))
	{
		printf("Can't open database: %s\n", sqlite3_errmsg(db));
	}
	else
	{
//printf("Opened database successfully\n");
		aj->aeci = 0;

		strcpy(sql, "select * from audiochains;");
		if ((rc = sqlite3_exec(db, sql, audiojam_selectchains_callback, (void*)aj, &err_msg)) != SQLITE_OK)
		{
			printf("Failed to select data, %s\n", err_msg);
			sqlite3_free(err_msg);
		}
		else
		{
// success
		}

		strcpy(sql, "select * from chaininputs;");
		if ((rc = sqlite3_exec(db, sql, audiojam_selectinputs_callback, (void*)aj, &err_msg)) != SQLITE_OK)
		{
			printf("Failed to select data, %s\n", err_msg);
			sqlite3_free(err_msg);
		}
		else
		{
// success
		}

/*
	}
	sqlite3_close(db);


	if ((rc = sqlite3_open(aj->dbpath, &db)))
	{
		printf("Can't open database: %s\n", sqlite3_errmsg(db));
	}
	else
	{
//printf("Opened database successfully\n");
*/
		for(i=0;i<aj->maxchains;i++)
		{
			if (aj->aec[i].id)
			{
				sprintf(sql, "select audioeffects.id, audioeffects.path from chaineffects inner join audioeffects on audioeffects.id = chaineffects.effect where chaineffects.chain = %d order by id;", aj->aec[i].id);
				//printf("%s\n", sql);
				aj->aeci = i;
				if ((rc = sqlite3_exec(db, sql, audiojam_select_audioeffects_callback, (void*)aj, &err_msg)) != SQLITE_OK)
				{
					printf("Failed to select data, %s\n", err_msg);
					sqlite3_free(err_msg);
				}
				else
				{
// success
				}
			}
		}
	}
	sqlite3_close(db);
}

void audiojam_init(audiojam *aj, int maxchains, int maxeffects, snd_pcm_format_t format, unsigned int rate, unsigned int channels, unsigned int frames, GtkWidget *container, char *dbpath, audiomixer *mx)
{
	int i, j, ret;

	aj->format = format;
	aj->rate = rate;
	aj->channels = channels;
	aj->frames = frames;
	aj->maxchains = maxchains;
	aj->maxeffects = maxeffects;
	strcpy(aj->dbpath, dbpath);
	aj->mx = mx;

	aj->container = container;

// horizontal box
	aj->toolbarhbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add(GTK_CONTAINER(aj->container), aj->toolbarhbox);

// toolbar
	aj->toolbar = gtk_toolbar_new();
	gtk_container_add(GTK_CONTAINER(aj->toolbarhbox), aj->toolbar);
	//gtk_box_pack_start(GTK_BOX(aj->toolbarhbox), aj->toolbar, TRUE, TRUE, 0);

	aj->icon_widget = gtk_image_new_from_icon_name("document-save", GTK_ICON_SIZE_BUTTON);
	aj->savechainsbutton = gtk_tool_button_new(aj->icon_widget, "Save Chains");
	gtk_tool_item_set_tooltip_text(aj->savechainsbutton, "Save Chains");
	g_signal_connect(aj->savechainsbutton, "clicked", G_CALLBACK(savechainsbutton_clicked), aj);
	gtk_toolbar_insert(GTK_TOOLBAR(aj->toolbar), aj->savechainsbutton, -1);

	aj->icon_widget = gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_BUTTON);
	aj->addchainbutton = gtk_tool_button_new(aj->icon_widget, "Add Chain");
	gtk_tool_item_set_tooltip_text(aj->addchainbutton, "Add Chain");
	g_signal_connect(aj->addchainbutton, "clicked", G_CALLBACK(addchainbutton_clicked), aj);
	gtk_toolbar_insert(GTK_TOOLBAR(aj->toolbar), aj->addchainbutton, -1);

	aj->chainnameentry = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(aj->toolbarhbox), GTK_WIDGET(aj->chainnameentry));

// frame
	aj->frame = gtk_frame_new("Effect Chains");
	//gtk_container_add(GTK_CONTAINER(aj->container), aj->frame);
	gtk_box_pack_start(GTK_BOX(aj->container), aj->frame, TRUE, TRUE, 0);

// horizontal box
	aj->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add(GTK_CONTAINER(aj->frame), aj->hbox);

	aj->aec = malloc(sizeof(audioeffectchain) * maxchains);
	for(i=0;i<aj->maxchains;i++)
	{
		aj->aec[i].id = 0;
		aj->aec[i].format = aj->format;
		aj->aec[i].rate = aj->rate;
		aj->aec[i].channels = aj->channels;
		aj->aec[i].frames = aj->frames;
		if ((ret=pthread_mutex_init(&(aj->aec[i].rackmutex), NULL))!=0)
			printf("rack mutex init failed, %d\n", ret);
		strcpy(aj->aec[i].dbpath, aj->dbpath);
		aj->aec[i].tp.tid = 0;
		aj->aec[i].channelbuffers = 2;

		aj->aec[i].effects = maxeffects;
		aj->aec[i].ae = malloc(sizeof(audioeffect) * aj->aec[i].effects);
		for(j=0;j<aj->aec[i].effects;j++)
		{
			aj->aec[i].ae[j].parent = &(aj->aec[i]);
			aj->aec[i].ae[j].handle = NULL;
			aj->aec[i].ae[j].format = aj->aec[i].format;
			aj->aec[i].ae[j].rate = aj->aec[i].rate;
			aj->aec[i].ae[j].channels = aj->aec[i].channels;
			if ((ret=pthread_mutex_init(&(aj->aec[i].ae[j].effectmutex), NULL))!=0)
				printf("effect mutex init failed, %d\n", ret);
		}
	}

	audiojam_loadfromdb(aj);
}

void audiojam_addchain(audiojam *aj, char *name)
{
	int i;

	for(i=0;i<aj->maxchains;i++)
		if (!aj->aec[i].id) break;

	if (i<aj->maxchains)
	{
		audioeffectchain_init(&(aj->aec[i]), name, i+1, aj->mx, aj->hbox, aj->dbpath);
		gtk_widget_show_all(aj->toolbarhbox);
		gtk_widget_show_all(aj->frame);
	}
	else
		printf("audio chains full, max %d\n", aj->maxchains);
}

void audiojam_close(audiojam *aj)
{
	int i, c;

	for(c=0;c<aj->maxchains;c++)
	{
		audioeffectchain_close(&(aj->aec[c]));
		for(i=0;i<aj->aec[c].effects;i++)
		{
			audioeffectchain_unloadeffect(&(aj->aec[c]), i);
			pthread_mutex_destroy(&(aj->aec[c].ae[i].effectmutex));
		}
		free(aj->aec[c].ae);
		pthread_mutex_destroy(&(aj->aec[c].rackmutex));
	}
	free(aj->aec);
}
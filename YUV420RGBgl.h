#ifndef YUV420RGBgl
#define YUV420RGBgl

#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <GL/glext.h>

typedef enum
{
	RGBA,
	YUV420,
	YUV422
}YUVformats;

typedef struct
{
	GLuint program;
	GLuint va_buffer;
	GLuint vb_buffer;
	GLuint ea_buffer;
	GLint positionLoc;
	GLint samplerLoc;
	GLint yuvsamplerLoc[3];
	GLint sizeLoc;
	GLint cmatrixLoc;
	GLuint tex;
	GLuint texyuv[3];
	GLint texture_coord_attribute;
	GLfloat vVertices[8];
	GLfloat tVertices[8];
	GLuint indices[6];

	GdkVisual *visual;
	GdkScreen *screen;
	XVisualInfo *xvisual;
	Colormap xcolormap;
	Display *display;
	Window root;
	int xscreen;
	int attributes[14];
	GLXContext context;
	GdkWindow *w;
	GdkDisplay *d;
	Display *did;
	Window wid;

	YUVformats fmt;
	int width, height; // texture size
	int linewidth, codecheight; // cropped video
}oglstate;

typedef struct
{
	YUVformats fmt;
	int width, height, linewidth, codecheight;
	char *buf;
	oglstate *ogl;
	GtkWidget *widget;
}oglparameters;

typedef struct
{
	pthread_mutex_t *A;
	pthread_cond_t *ready;
	pthread_cond_t *done;
	pthread_cond_t *busy;
	int commandready, commanddone, commandbusy;
	void (*func)(oglparameters *data);
	oglparameters data;
	oglstate ogl;
	pthread_t tid;
	int retval;
	int stoprequested;
	GtkWidget *widget;
}oglidle;

void realize_da_event(GtkWidget *widget, gpointer data);
gboolean draw_da_event(GtkWidget *widget, cairo_t *cr, gpointer data);
gboolean size_allocate_da_event(GtkWidget *widget, GdkRectangle *allocation, gpointer user_data);
void destroy_da_event(GtkWidget *widget, gpointer data);
void reinit_ogl(oglidle *i, YUVformats fmt, int width, int height, int linewidth, int codecheight);
void draw_texture(oglidle *i, char *buf);
#endif

/*
 * YUV420RGBgl.c
 * 
 * Copyright 2017  <pi@raspberrypi>
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

#include "YUV420RGBgl.h"

// Idle Queue

void init_oglidle(oglidle *i)
{
	i->stoprequested = 0;
	i->commandbusy = 1;

	i->A = malloc(sizeof(pthread_mutex_t));
	i->ready = malloc(sizeof(pthread_cond_t));
	i->done = malloc(sizeof(pthread_cond_t));
	i->busy = malloc(sizeof(pthread_cond_t));

	int ret;
	if((ret=pthread_mutex_init(i->A, NULL))!=0 )
		printf("A init failed, %d\n", ret);
		
	if((ret=pthread_cond_init(i->ready, NULL))!=0 )
		printf("ready init failed, %d\n", ret);

	if((ret=pthread_cond_init(i->done, NULL))!=0 )
		printf("done init failed, %d\n", ret);

	if((ret=pthread_cond_init(i->busy, NULL))!=0 )
		printf("busy init failed, %d\n", ret);
}

void close_oglidle(oglidle *i)
{
	pthread_mutex_destroy(i->A);
	free(i->A);
	pthread_cond_destroy(i->ready);
	free(i->ready);
	pthread_cond_destroy(i->done);
	free(i->done);
	pthread_cond_destroy(i->busy);
	free(i->busy);
}

void* oglidle_thread(void *args)
{
	int ctype = PTHREAD_CANCEL_ASYNCHRONOUS;
	int ctype_old;
	pthread_setcanceltype(ctype, &ctype_old);

	oglidle *i = (oglidle *)args;

	pthread_mutex_lock(i->A);
	i->commandready = 0;
	i->commandbusy = 0;
	pthread_cond_signal(i->busy);

	while (!i->stoprequested)
	{
		while (!(i->commandready))
		{
			pthread_cond_wait(i->ready, i->A);
		}
		if (i->stoprequested) break;

		(i->func)(&(i->data));
		i->commanddone = 1;
		pthread_cond_signal(i->done); // Should wake up *one* thread
		i->commandready = 0;
	}
	pthread_mutex_unlock(i->A);

//printf("exiting idle thread\n");
	i->retval = 0;
	pthread_exit(&(i->retval));
}

void oglidle_add(oglidle *i, void (*f)(oglparameters *data), oglparameters *data)
{
	pthread_mutex_lock(i->A);
	while (i->commandbusy)
	{
		pthread_cond_wait(i->busy, i->A);
	}
	i->func = f;
	i->data = *data;

	i->commandbusy = 1;
	i->commanddone = 0;

	i->commandready = 1;
	pthread_cond_signal(i->ready);
	
	while (!i->commanddone)
	{
		pthread_cond_wait(i->done, i->A);
	}
	i->commandbusy = 0;
	pthread_cond_signal(i->busy);
	pthread_mutex_unlock(i->A);
}

void oglidle_start(oglidle *i)
{
	init_oglidle(i);

	int err = pthread_create(&(i->tid), NULL, &oglidle_thread, (void *)i);
	if (err)
	{}
}

void oglidle_stop(oglidle *i)
{
	int ret;

	pthread_mutex_lock(i->A);
	i->stoprequested = 1;
	i->commandready = 1;
	pthread_cond_signal(i->ready);
	pthread_mutex_unlock(i->A);

	if ((ret=pthread_join(i->tid, NULL)))
		printf("pthread_join error, i->tid, %d\n", ret);

	close_oglidle(i);
}

// GL

void init_glxattributes(oglstate *ogl)
{
	int i;

	int glxa[14] =	{	GLX_RGBA,
						GLX_RED_SIZE, 8, 
						GLX_GREEN_SIZE, 8,
						GLX_BLUE_SIZE, 8,
						GLX_ALPHA_SIZE, 8, 
						GLX_DOUBLEBUFFER, TRUE, 
						GLX_DEPTH_SIZE, 12, 
						None 
					};
//printf("init_glxattributes %d\n", sizeof(glxa)/sizeof(int));
	for(i=0;i<sizeof(glxa)/sizeof(int);i++)
	{
		ogl->attributes[i] = glxa[i];
	}
}

void init_vertices_indices(oglstate *ogl)
{
	int i;

	GLfloat xvVertices[8] = { -1.0f,  1.0f, // Position 0
                              -1.0f, -1.0f, // Position 1
                               1.0f, -1.0f, // Position 2
                               1.0f,  1.0f  // Position 3
                            };

	GLfloat xtVertices[8] = {  0.0f,  0.0f,        // TexCoord 0 
                               0.0f,  1.0f,        // TexCoord 1
                               1.0f,  1.0f,        // TexCoord 2
                               1.0f,  0.0f         // TexCoord 3
                            };

	GLuint xIndices[6] = { 0, 1, 2, 0, 2, 3 };

	for(i=0;i<sizeof(xvVertices)/sizeof(GLfloat);i++)
	{
		ogl->vVertices[i] = xvVertices[i];
	}

	for(i=0;i<sizeof(xtVertices)/sizeof(GLfloat);i++)
	{
		ogl->tVertices[i] = xtVertices[i];
	}

// rescale width
	if (ogl->linewidth>ogl->width)
	{
		ogl->tVertices[6] = ogl->tVertices[4] = (float)ogl->width / (float)ogl->linewidth;
	}
//rescale height
	if (ogl->height>ogl->codecheight)
	{
		ogl->tVertices[5] = ogl->tVertices[3] = (float)ogl->codecheight / (float)ogl->height;
	}

	for(i=0;i<sizeof(xIndices)/sizeof(GLuint);i++)
	{
		ogl->indices[i] = xIndices[i];
	}
}

/* Initialize the GL buffers */
void init_buffers(oglstate *o)
{
	GLuint vao, vbo, eab;
//glGetError();
	// We only use one VAO, so we always keep it bound
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
//printf("glBindVertexArray %d\n", glGetError());
	glGenBuffers(1, &vbo);
	// Allocate space for vertex positions and texture coordinates
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(o->vVertices) + sizeof(o->tVertices), NULL, GL_STATIC_DRAW);
//printf("glBufferData %d\n", glGetError());
	// Transfer the vertex positions
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(o->vVertices), o->vVertices);
//printf("glBufferSubData vertex %d\n", glGetError());
	// Transfer the texture coordinates
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(o->vVertices), sizeof(o->tVertices), o->tVertices);
//printf("glBufferSubData tex %d\n", glGetError());

	// Create an Element Array Buffer that will store the indices array:
	glGenBuffers(1, &eab);
	// Transfer the data from indices to eab
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eab);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(o->indices), o->indices, GL_STATIC_DRAW);
//printf("glBufferData eab %d\n", glGetError());

	o->va_buffer = vao;
	o->vb_buffer = vbo;
	o->ea_buffer = eab;
}

/* Create and compile a shader */
GLuint create_shader(int type, const GLchar *src)
{
	GLuint shader;
	int status;

	shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		int log_len;
		char *buffer;

		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
		buffer = malloc(log_len + 1);
		glGetShaderInfoLog(shader, log_len, NULL, buffer);
		printf("Compile failure in %s shader:\n%s\n", type == GL_VERTEX_SHADER ? "vertex" : "fragment", buffer);
		free(buffer);
		glDeleteShader(shader);
		shader = 0;
	}
	return shader;
}

/*
void init_shaders(GLuint *program_out)
{
	GLuint vertex, fragment;
	GLuint program = 0;
	int status;

	GLbyte vShaderStr[] =  
		"attribute vec4 a_position;   \n"
		"attribute vec2 a_texCoord;   \n"
		"varying vec2 v_texCoord;     \n"
		"void main()                  \n"
		"{                            \n"
		"   gl_Position = a_position; \n"
		"   v_texCoord = a_texCoord;  \n"
		"}                            \n";

	GLbyte fShaderStr[] =  
		"varying vec2 v_texCoord;                            \n"
		"uniform sampler2D s_texture;                        \n"
		"void main()                                         \n"
		"{                                                   \n"
		"  gl_FragColor = texture2D(s_texture, v_texCoord);  \n"
		//"  gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);\n"
		"}                                                   \n";

	vertex = create_shader(GL_VERTEX_SHADER, (const GLchar*)vShaderStr);
	if (!vertex)
	{
		*program_out = 0;
		return;
	}

	fragment = create_shader(GL_FRAGMENT_SHADER, (const GLchar*)fShaderStr);
	if (!fragment)
	{
		glDeleteShader(vertex);
		*program_out = 0;
		return;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		int log_len;
		char *buffer;

		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
		buffer = malloc(log_len + 1);
		glGetProgramInfoLog(program, log_len, NULL, buffer);
		printf("Linking failure:\n%s\n", buffer);
		free(buffer);
		glDeleteProgram(program);
		program = 0;
    }
	else
	{
		glDetachShader(program, vertex);
		glDetachShader(program, fragment);
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	*program_out = program;
}
*/

void init_shaders_rgba(GLuint *program_out)
{
	GLuint vertex, fragment;
	GLuint program = 0;
	int status;

	GLbyte vShaderStr[] =  
		"attribute vec4 a_position;   \n"
		"attribute vec2 a_texCoord;   \n"
		"varying vec2 v_texCoord;     \n"
		"void main()                  \n"
		"{                            \n"
		"   gl_Position = a_position; \n"
		"   v_texCoord = a_texCoord;  \n"
		"}                            \n";

	GLbyte fShaderStr[] =  
		"varying vec2 v_texCoord;                            \n"
		"uniform sampler2D s_texture;                        \n"
		"void main()                                         \n"
		"{                                                   \n"
		"  gl_FragColor = texture2D(s_texture, v_texCoord);  \n"
		//"  gl_FragColor = vec4(1.0, 0.0, 1.0, 1.0);\n"
		"}                                                   \n";

	vertex = create_shader(GL_VERTEX_SHADER, (const GLchar*)vShaderStr);
	if (!vertex)
	{
		*program_out = 0;
		return;
	}

	fragment = create_shader(GL_FRAGMENT_SHADER, (const GLchar*)fShaderStr);
	if (!fragment)
	{
		glDeleteShader(vertex);
		*program_out = 0;
		return;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		int log_len;
		char *buffer;

		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
		buffer = malloc(log_len + 1);
		glGetProgramInfoLog(program, log_len, NULL, buffer);
		printf("Linking failure:\n%s\n", buffer);
		free(buffer);
		glDeleteProgram(program);
		program = 0;
    }
	else
	{
		glDetachShader(program, vertex);
		glDetachShader(program, fragment);
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	*program_out = program;
}
/*
void init_shaders_yuv420(GLuint *program_out)
{
	GLuint vertex, fragment;
	GLuint program = 0;
	int status;

	GLbyte vShaderStr[] = 
		"attribute vec4 a_position;   \n"
		"attribute vec2 a_texCoord;   \n"
		"varying vec2 v_texCoord;     \n"
		"void main()                  \n"
		"{                            \n"
		"   gl_Position = a_position; \n"
		"   v_texCoord = a_texCoord;  \n"
		"}                            \n";

	GLbyte fShaderStr[] =
		"varying vec2 v_texCoord;                              \n"
		"uniform vec2 texSize;                                 \n"
		"uniform mat3 yuv2rgb;                                 \n"
		"uniform sampler2D s_texture;                          \n"
		"void main()                                           \n"
		"{                                                     \n"
		"  float y, u, v, r, g, b;                             \n"
		"  float yx, yy, ux, uy, vx, vy;                       \n"

		"  vec2 texCoord=vec2(v_texCoord.x*texSize.x, v_texCoord.y*texSize.y);\n"
		"  float oe=floor(mod(texCoord.y/2.0, 2.0));           \n"

		"  yx=v_texCoord.x;                                    \n"
		"  yy=v_texCoord.y*2.0/3.0;                            \n"
		"  ux=oe*0.5+v_texCoord.x/2.0;                         \n"
		"  uy=4.0/6.0+v_texCoord.y/6.0;                        \n"
		"  vx=ux;                                              \n"
		"  vy=5.0/6.0+v_texCoord.y/6.0;                        \n"

		"  int x=int(mod(texCoord.x, 8.0));                    \n"

		"  vec4 y4 = vec4(float((x==0)||(x==4)), float((x==1)||(x==5)), float((x==2)||(x==6)), float((x==3)||(x==7))); \n"
		"  vec4 uv4 = vec4(float((x==0)||(x==1)), float((x==2)||(x==3)), float((x==4)||(x==5)), float((x==6)||(x==7))); \n"
		"  y=dot(y4,  texture2D(s_texture, vec2(yx, yy)));     \n"
		"  u=dot(uv4, texture2D(s_texture, vec2(ux, uy)));     \n"
		"  v=dot(uv4, texture2D(s_texture, vec2(vx, vy)));     \n"

		"  vec3 yuv=vec3(1.1643*(y-0.0625), u-0.5, v-0.5);     \n"
		"  vec3 rgb=yuv*yuv2rgb;                               \n"

		"  gl_FragColor=vec4(rgb, 1.0);                        \n"
		"}                                                     \n";

	vertex = create_shader(GL_VERTEX_SHADER, (const GLchar*)vShaderStr);
	if (!vertex)
	{
		*program_out = 0;
		return;
	}

	fragment = create_shader(GL_FRAGMENT_SHADER, (const GLchar*)fShaderStr);
	if (!fragment)
	{
		glDeleteShader(vertex);
		*program_out = 0;
		return;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		int log_len;
		char *buffer;

		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
		buffer = malloc(log_len + 1);
		glGetProgramInfoLog(program, log_len, NULL, buffer);
		printf("Linking failure:\n%s\n", buffer);
		free(buffer);
		glDeleteProgram(program);
		program = 0;
    }
	else
	{
		glDetachShader(program, vertex);
		glDetachShader(program, fragment);
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	*program_out = program;
}
*/

void init_shaders_yuv420(GLuint *program_out)
{
	GLuint vertex, fragment;
	GLuint program = 0;
	int status;

	GLbyte vShaderStr[] = 
		"attribute vec4 a_position;   \n"
		"attribute vec2 a_texCoord;   \n"
		"varying vec2 v_texCoord;     \n"
		"void main()                  \n"
		"{                            \n"
		"   gl_Position = a_position; \n"
		"   v_texCoord = a_texCoord;  \n"
		"}                            \n";

	GLbyte fShaderStr[] =
		"varying vec2 v_texCoord;                              \n"
		"uniform mat3 yuv2rgb;                                 \n"
		"uniform sampler2D y_texture;                          \n"
		"uniform sampler2D u_texture;                          \n"
		"uniform sampler2D v_texture;                          \n"
		"void main()                                           \n"
		"{                                                     \n"
		"  float y, u, v;                                      \n"

		"  y=texture2D(y_texture, v_texCoord).s;               \n"
		"  u=texture2D(u_texture, v_texCoord).s;               \n"
		"  v=texture2D(v_texture, v_texCoord).s;               \n"

		"  vec3 yuv=vec3(1.1643*(y-0.0625), u-0.5, v-0.5);     \n"
		"  vec3 rgb=yuv*yuv2rgb;                               \n"

		"  gl_FragColor=vec4(rgb, 1.0);                        \n"
		"}                                                     \n";

	vertex = create_shader(GL_VERTEX_SHADER, (const GLchar*)vShaderStr);
	if (!vertex)
	{
		*program_out = 0;
		return;
	}

	fragment = create_shader(GL_FRAGMENT_SHADER, (const GLchar*)fShaderStr);
	if (!fragment)
	{
		glDeleteShader(vertex);
		*program_out = 0;
		return;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		int log_len;
		char *buffer;

		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
		buffer = malloc(log_len + 1);
		glGetProgramInfoLog(program, log_len, NULL, buffer);
		printf("Linking failure:\n%s\n", buffer);
		free(buffer);
		glDeleteProgram(program);
		program = 0;
    }
	else
	{
		glDetachShader(program, vertex);
		glDetachShader(program, fragment);
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	*program_out = program;
}

/*
void init_shaders_yuv422(GLuint *program_out)
{
	GLuint vertex, fragment;
	GLuint program = 0;
	int status;

	GLbyte vShaderStr[] = 
		"attribute vec4 a_position;   \n"
		"attribute vec2 a_texCoord;   \n"
		"varying vec2 v_texCoord;     \n"
		"void main()                  \n"
		"{                            \n"
		"   gl_Position = a_position; \n"
		"   v_texCoord = a_texCoord;  \n"
		"}                            \n";

	GLbyte fShaderStr[] =
      "varying vec2 v_texCoord;                              \n"
      "uniform vec2 texSize;                                 \n"
      "uniform mat3 yuv2rgb;                                 \n"
      "uniform sampler2D s_texture;                          \n"
      "void main()                                           \n"
      "{                                                     \n"
      "  float y, u, v, r, g, b;                             \n"
      "  float yx, yy, ux, uy, vx, vy;                       \n"

      "  vec2 texCoord=vec2(v_texCoord.x*texSize.x, v_texCoord.y*texSize.y);\n"
      "  float oe=floor(mod(texCoord.y, 2.0));               \n"

      "  yx=v_texCoord.x;                                    \n"
      "  yy=v_texCoord.y/2.0;                                \n"
      "  ux=oe*0.5+v_texCoord.x/2.0;                         \n"
      "  uy=0.5+v_texCoord.y/4.0;                            \n"
      "  vx=ux;                                              \n"
      "  vy=0.75+v_texCoord.y/4.0;                           \n"

      "  int x=int(mod(texCoord.x, 8.0));                    \n"

      "  vec4 y4 = vec4(float((x==0)||(x==4)), float((x==1)||(x==5)), float((x==2)||(x==6)), float((x==3)||(x==7))); \n"
      "  vec4 uv4 = vec4(float((x==0)||(x==1)), float((x==2)||(x==3)), float((x==4)||(x==5)), float((x==6)||(x==7))); \n"
      "  y=dot(y4,  texture2D(s_texture, vec2(yx, yy)));     \n"
      "  u=dot(uv4, texture2D(s_texture, vec2(ux, uy)));     \n"
      "  v=dot(uv4, texture2D(s_texture, vec2(vx, vy)));     \n"

      "  vec3 yuv=vec3(y, u-0.5, v-0.5);                     \n"
      "  vec3 rgb=yuv*yuv2rgb;                               \n"

      "  gl_FragColor=vec4(rgb, 1.0);                        \n"
      "}                                                     \n";

	vertex = create_shader(GL_VERTEX_SHADER, (const GLchar*)vShaderStr);
	if (!vertex)
	{
		*program_out = 0;
		return;
	}

	fragment = create_shader(GL_FRAGMENT_SHADER, (const GLchar*)fShaderStr);
	if (!fragment)
	{
		glDeleteShader(vertex);
		*program_out = 0;
		return;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		int log_len;
		char *buffer;

		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
		buffer = malloc(log_len + 1);
		glGetProgramInfoLog(program, log_len, NULL, buffer);
		printf("Linking failure:\n%s\n", buffer);
		free(buffer);
		glDeleteProgram(program);
		program = 0;
    }
	else
	{
		glDetachShader(program, vertex);
		glDetachShader(program, fragment);
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	*program_out = program;
}
*/

void init_shaders_yuv422(GLuint *program_out)
{
	GLuint vertex, fragment;
	GLuint program = 0;
	int status;

	GLbyte vShaderStr[] = 
		"attribute vec4 a_position;   \n"
		"attribute vec2 a_texCoord;   \n"
		"varying vec2 v_texCoord;     \n"
		"void main()                  \n"
		"{                            \n"
		"   gl_Position = a_position; \n"
		"   v_texCoord = a_texCoord;  \n"
		"}                            \n";

	GLbyte fShaderStr[] =
		"varying vec2 v_texCoord;                              \n"
		"uniform mat3 yuv2rgb;                                 \n"
		"uniform sampler2D y_texture;                          \n"
		"uniform sampler2D u_texture;                          \n"
		"uniform sampler2D v_texture;                          \n"
		"void main()                                           \n"
		"{                                                     \n"
		"  float y, u, v;                                      \n"

		"  y=texture2D(y_texture, v_texCoord).s;               \n"
		"  u=texture2D(u_texture, v_texCoord).s;               \n"
		"  v=texture2D(v_texture, v_texCoord).s;               \n"

		"  vec3 yuv=vec3(y, u-0.5, v-0.5);                     \n"
		"  vec3 rgb=yuv*yuv2rgb;                               \n"

		"  gl_FragColor=vec4(rgb, 1.0);                        \n"
		"}                                                     \n";

	vertex = create_shader(GL_VERTEX_SHADER, (const GLchar*)vShaderStr);
	if (!vertex)
	{
		*program_out = 0;
		return;
	}

	fragment = create_shader(GL_FRAGMENT_SHADER, (const GLchar*)fShaderStr);
	if (!fragment)
	{
		glDeleteShader(vertex);
		*program_out = 0;
		return;
	}

	program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE)
	{
		int log_len;
		char *buffer;

		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);
		buffer = malloc(log_len + 1);
		glGetProgramInfoLog(program, log_len, NULL, buffer);
		printf("Linking failure:\n%s\n", buffer);
		free(buffer);
		glDeleteProgram(program);
		program = 0;
    }
	else
	{
		glDetachShader(program, vertex);
		glDetachShader(program, fragment);
	}

	glDeleteShader(vertex);
	glDeleteShader(fragment);

	*program_out = program;
}

void init_ogl_rgba(oglparameters *op)
{
	oglstate *ogl = op->ogl;

	init_vertices_indices(ogl);
	init_buffers(ogl);
	init_shaders_rgba(&(ogl->program));

	glUseProgram(ogl->program);

	glGenTextures(1, &(ogl->tex));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ogl->tex);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, op->linewidth, op->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
//printf("init_ogl_rgba %d %d\n", ogl->width, ogl->height);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// Get the location of the attributes that enters in the vertex shader
	ogl->positionLoc = glGetAttribLocation(ogl->program, "a_position");
	// Specify how the data for position can be accessed
	glVertexAttribPointer(ogl->positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
	// Enable the attribute
	glEnableVertexAttribArray(ogl->positionLoc);

	// Texture coord attribute
	ogl->texture_coord_attribute = glGetAttribLocation(ogl->program, "a_texCoord");
	glVertexAttribPointer(ogl->texture_coord_attribute, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid *)sizeof(ogl->vVertices));
	glEnableVertexAttribArray(ogl->texture_coord_attribute);

	ogl->samplerLoc = glGetUniformLocation(ogl->program, "s_texture" );
	glUniform1i(ogl->samplerLoc, 0);
}

/*
void init_ogl_yuv420(oglstate *ogl)
{
	glGetError();
	init_vertices_indices(ogl);
	init_buffers(ogl);
	init_shaders_yuv420(&(ogl->program));

	// Use our shaders
	glUseProgram(ogl->program);

	glGenTextures(1, &(ogl->tex));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ogl->tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ogl->width/4, ogl->height*3/2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
//glGetError();
	// Get the location of the attributes that enters in the vertex shader
	ogl->positionLoc = glGetAttribLocation(ogl->program, "a_position");
//printf("positionloc %d\n", glGetError());
	// Specify how the data for position can be accessed
	glVertexAttribPointer(ogl->positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
//printf("glVertexAttribPointer %d\n", glGetError());
	// Enable the attribute
	glEnableVertexAttribArray(ogl->positionLoc);
//printf("glEnableVertexAttribArray %d\n", glGetError());

	// Texture coord attribute
	ogl->texture_coord_attribute = glGetAttribLocation(ogl->program, "a_texCoord");
	glVertexAttribPointer(ogl->texture_coord_attribute, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid *)sizeof(ogl->vVertices));
	glEnableVertexAttribArray(ogl->texture_coord_attribute);

	ogl->samplerLoc = glGetUniformLocation(ogl->program, "s_texture");
	glUniform1i(ogl->samplerLoc, 0);

	ogl->sizeLoc = glGetUniformLocation(ogl->program, "texSize");
	GLfloat picSize420[2] = { (GLfloat)ogl->width, (GLfloat)ogl->height*3/2 };
	glUniform2fv(ogl->sizeLoc, 1, picSize420);

	ogl->cmatrixLoc = glGetUniformLocation(ogl->program, "yuv2rgb");
	GLfloat yuv2rgbmatrix420[9] = { 1.0, 0.0, 1.5958, 1.0, -0.3917, -0.8129, 1.0, 2.017, 0.0 }; // YUV420
	glUniformMatrix3fv(ogl->cmatrixLoc, 1, GL_FALSE, yuv2rgbmatrix420);
}
*/

void init_ogl_yuv420(oglparameters *op)
{
	oglstate *ogl = op->ogl;

	glGetError();
	init_vertices_indices(ogl);
	init_buffers(ogl);
	init_shaders_yuv420(&(ogl->program));

	// Use our shaders
	glUseProgram(ogl->program);

	//glGenTextures(1, &(ogl->tex));
	glGenTextures(3, &(ogl->texyuv[0]));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ogl->texyuv[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, op->linewidth, op->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ogl->texyuv[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, op->linewidth/2, op->height/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, ogl->texyuv[2]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, op->linewidth/2, op->height/2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// Get the location of the attributes that enters in the vertex shader
	ogl->positionLoc = glGetAttribLocation(ogl->program, "a_position");
	// Specify how the data for position can be accessed
	glVertexAttribPointer(ogl->positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
	// Enable the attribute
	glEnableVertexAttribArray(ogl->positionLoc);

	// Texture coord attribute
	ogl->texture_coord_attribute = glGetAttribLocation(ogl->program, "a_texCoord");
	glVertexAttribPointer(ogl->texture_coord_attribute, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid *)sizeof(ogl->vVertices));
	glEnableVertexAttribArray(ogl->texture_coord_attribute);

	ogl->yuvsamplerLoc[0] = glGetUniformLocation(ogl->program, "y_texture");
	glUniform1i(ogl->yuvsamplerLoc[0], 0);

	ogl->yuvsamplerLoc[1] = glGetUniformLocation(ogl->program, "u_texture");
	glUniform1i(ogl->yuvsamplerLoc[1], 1);

	ogl->yuvsamplerLoc[2] = glGetUniformLocation(ogl->program, "v_texture");
	glUniform1i(ogl->yuvsamplerLoc[2], 2);

	ogl->cmatrixLoc = glGetUniformLocation(ogl->program, "yuv2rgb");
	GLfloat yuv2rgbmatrix420[9] = { 1.0, 0.0, 1.5958, 1.0, -0.3917, -0.8129, 1.0, 2.017, 0.0 }; // YUV420
	glUniformMatrix3fv(ogl->cmatrixLoc, 1, GL_FALSE, yuv2rgbmatrix420);
}

/*
void init_ogl_yuv422(oglparameters *op)
{
	oglstate *ogl = op->ogl;

	glGetError();
	init_vertices_indices(ogl);
	init_buffers(ogl);
	init_shaders_yuv422(&(ogl->program));

	// Use our shaders
	glUseProgram(ogl->program);

	glGenTextures(1, &(ogl->tex));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ogl->tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, op->linewidth/4, op->height*2, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

//glGetError();
	// Get the location of the attributes that enters in the vertex shader
	ogl->positionLoc = glGetAttribLocation(ogl->program, "a_position");
//printf("positionloc %d\n", glGetError());
	// Specify how the data for position can be accessed
	glVertexAttribPointer(ogl->positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
//printf("glVertexAttribPointer %d\n", glGetError());
	// Enable the attribute
	glEnableVertexAttribArray(ogl->positionLoc);
//printf("glEnableVertexAttribArray %d\n", glGetError());

	// Texture coord attribute
	ogl->texture_coord_attribute = glGetAttribLocation(ogl->program, "a_texCoord");
	glVertexAttribPointer(ogl->texture_coord_attribute, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid *)sizeof(ogl->vVertices));
	glEnableVertexAttribArray(ogl->texture_coord_attribute);

	ogl->samplerLoc = glGetUniformLocation(ogl->program, "s_texture");
	glUniform1i(ogl->samplerLoc, 0);

	ogl->sizeLoc = glGetUniformLocation ( ogl->program, "texSize" );
	GLfloat picSize[2] = { (GLfloat)op->linewidth, (GLfloat)op->height };
	glUniform2fv(ogl->sizeLoc, 1, picSize);
   
	ogl->cmatrixLoc = glGetUniformLocation(ogl->program, "yuv2rgb");
	GLfloat yuv2rgbmatrix422[9] = { 1.0, 0.0, 1.402, 1.0, -0.344, -0.714, 1.0, 1.772, 0.0 }; // YUV422// YUV420
	glUniformMatrix3fv(ogl->cmatrixLoc, 1, GL_FALSE, yuv2rgbmatrix422);
}
*/

void init_ogl_yuv422(oglparameters *op)
{
	oglstate *ogl = op->ogl;

	glGetError();
	init_vertices_indices(ogl);
	init_buffers(ogl);
	init_shaders_yuv422(&(ogl->program));

	// Use our shaders
	glUseProgram(ogl->program);

	glGenTextures(3, &(ogl->texyuv[0]));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ogl->texyuv[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, op->linewidth, op->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ogl->texyuv[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, op->linewidth/2, op->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, ogl->texyuv[2]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, op->linewidth/2, op->height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	// Get the location of the attributes that enters in the vertex shader
	ogl->positionLoc = glGetAttribLocation(ogl->program, "a_position");
	// Specify how the data for position can be accessed
	glVertexAttribPointer(ogl->positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
	// Enable the attribute
	glEnableVertexAttribArray(ogl->positionLoc);

	// Texture coord attribute
	ogl->texture_coord_attribute = glGetAttribLocation(ogl->program, "a_texCoord");
	glVertexAttribPointer(ogl->texture_coord_attribute, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid *)sizeof(ogl->vVertices));
	glEnableVertexAttribArray(ogl->texture_coord_attribute);

	ogl->yuvsamplerLoc[0] = glGetUniformLocation(ogl->program, "y_texture");
	glUniform1i(ogl->yuvsamplerLoc[0], 0);

	ogl->yuvsamplerLoc[1] = glGetUniformLocation(ogl->program, "u_texture");
	glUniform1i(ogl->yuvsamplerLoc[1], 1);

	ogl->yuvsamplerLoc[2] = glGetUniformLocation(ogl->program, "v_texture");
	glUniform1i(ogl->yuvsamplerLoc[2], 2);
   
	ogl->cmatrixLoc = glGetUniformLocation(ogl->program, "yuv2rgb");
	GLfloat yuv2rgbmatrix422[9] = { 1.0, 0.0, 1.402, 1.0, -0.344, -0.714, 1.0, 1.772, 0.0 }; // YUV422// YUV420
	glUniformMatrix3fv(ogl->cmatrixLoc, 1, GL_FALSE, yuv2rgbmatrix422);
}

void init_ogl(oglparameters *op)
{
	switch(op->fmt)
	{
		case YUV420:
			init_ogl_yuv420(op);
			break;
		case YUV422:
			init_ogl_yuv422(op);
			break;
		case RGBA:
		default:
			init_ogl_rgba(op);
			break;
	}
}

void tex2D_rgba(oglstate *ogl, char *rgba)
{
	glBindTexture(GL_TEXTURE_2D, ogl->tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ogl->linewidth, ogl->height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
}

/*
void tex2D_yuv420(oglstate *ogl, char *yuv)
{
	glBindTexture(GL_TEXTURE_2D, ogl->tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ogl->width/4, ogl->height*3/2, GL_RGBA, GL_UNSIGNED_BYTE, yuv);
}
*/

void tex2D_yuv420(oglstate *ogl, char *yuv)
{
	glActiveTexture(GL_TEXTURE0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ogl->linewidth, ogl->height, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv);

	glActiveTexture(GL_TEXTURE1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ogl->linewidth/2, ogl->height/2, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv+(ogl->linewidth*ogl->height));

	glActiveTexture(GL_TEXTURE2);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ogl->linewidth/2, ogl->height/2, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv+(ogl->linewidth*ogl->height*5/4));
}

/*
void tex2D_yuv422(oglstate *ogl, char *yuv)
{
	glBindTexture(GL_TEXTURE_2D, ogl->tex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ogl->linewidth/4, ogl->height*2, GL_RGBA, GL_UNSIGNED_BYTE, yuv);
}
*/

void tex2D_yuv422(oglstate *ogl, char *yuv)
{
	glActiveTexture(GL_TEXTURE0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ogl->linewidth, ogl->height, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv);

	glActiveTexture(GL_TEXTURE1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ogl->linewidth/2, ogl->height, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv+(ogl->linewidth*ogl->height));

	glActiveTexture(GL_TEXTURE2);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, ogl->linewidth/2, ogl->height, GL_LUMINANCE, GL_UNSIGNED_BYTE, yuv+(ogl->linewidth*ogl->height*3/2));
}

void tex2D_ogl(oglparameters *op)
{
	oglstate *ogl = op->ogl;
	char *buf = op->buf;

	switch(op->fmt)
	{
		case YUV420:
			tex2D_yuv420(ogl, buf);
			break;
		case YUV422:
			tex2D_yuv422(ogl, buf);
			break;
		case RGBA:
		default:
			tex2D_rgba(ogl, buf);
			break;
	}
}

void close_ogl_rgba(oglstate *ogl)
{
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	glDeleteTextures(1, &(ogl->tex));
	glDeleteBuffers(1, &(ogl->vb_buffer));
	glDeleteBuffers(1, &(ogl->ea_buffer));
	glDeleteVertexArrays(1, &(ogl->va_buffer));
	glDeleteProgram(ogl->program);
//printf("close_ogl_rgba\n");
}

void close_ogl_yuv420(oglstate *ogl)
{
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	glDeleteTextures(3, &(ogl->texyuv[0]));
	glDeleteBuffers(1, &(ogl->vb_buffer));
	glDeleteBuffers(1, &(ogl->ea_buffer));
	glDeleteVertexArrays(1, &(ogl->va_buffer));
	glDeleteProgram(ogl->program);
}

void close_ogl_yuv422(oglstate *ogl)
{
	glDisableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glUseProgram(0);

	glDeleteTextures(3, &(ogl->texyuv[0]));
	glDeleteBuffers(1, &(ogl->vb_buffer));
	glDeleteBuffers(1, &(ogl->ea_buffer));
	glDeleteVertexArrays(1, &(ogl->va_buffer));
	glDeleteProgram(ogl->program);
}

void close_ogl(oglparameters *op)
{
	oglstate *ogl = op->ogl;

	switch(ogl->fmt)
	{
		case RGBA:
			close_ogl_rgba(ogl);
			break;
		case YUV422:
			close_ogl_yuv422(ogl);
			break;
		case YUV420:
		default:
			close_ogl_yuv420(ogl);
			break;
	}
}

void makecurrent_ogl(oglparameters *op)
{
	oglstate *ogl = op->ogl;

// initialize
	init_glxattributes(ogl);

	ogl->display = gdk_x11_get_default_xdisplay();
	ogl->xscreen = DefaultScreen(ogl->display);
	ogl->xvisual = glXChooseVisual(ogl->display, ogl->xscreen, ogl->attributes);
	ogl->root = RootWindow(ogl->display, ogl->xscreen);
	ogl->xcolormap = XCreateColormap(ogl->display, ogl->root, ogl->xvisual->visual, AllocNone);
	ogl->context = glXCreateContext(ogl->display, ogl->xvisual, NULL, TRUE);

	ogl->screen = gdk_screen_get_default();
	ogl->visual = gdk_x11_screen_lookup_visual(ogl->screen, ogl->xvisual->visualid);

	gtk_widget_set_visual(op->widget, ogl->visual);
	ogl->w = gtk_widget_get_window(op->widget);
	ogl->d = gtk_widget_get_display(op->widget);
	ogl->did = gdk_x11_display_get_xdisplay(ogl->d);
	ogl->wid = gdk_x11_window_get_xid(ogl->w);
	glXMakeCurrent(ogl->did, ogl->wid, ogl->context);
//printf("glXMakeCurrent %ld %ld\n", (unsigned long int)ogl->did, ogl->wid);
}

/*
void DrawAQuad()
{
 glClearColor(1.0, 1.0, 1.0, 1.0);
 glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

 glMatrixMode(GL_PROJECTION);
 glLoadIdentity();
 glOrtho(-1., 1., -1., 1., 1., 20.);

 glMatrixMode(GL_MODELVIEW);
 glLoadIdentity();
 gluLookAt(0., 0., 10., 0., 0., 0., 0., 1., 0.);

 glBegin(GL_QUADS);
  glColor3f(1., 0., 0.); glVertex3f(-.75, -.75, 0.);
  glColor3f(0., 1., 0.); glVertex3f( .75, -.75, 0.);
  glColor3f(0., 0., 1.); glVertex3f( .75,  .75, 0.);
  glColor3f(1., 1., 0.); glVertex3f(-.75,  .75, 0.);
 glEnd();
} 
*/

void drawwidget_ogl(oglparameters *op)
{
	oglstate *ogl = op->ogl;
	GtkWidget *widget = op->widget;

//printf("%d %d\n", gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget));
	glViewport(0, 0, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget));
//printf("draw elements\n");
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	//DrawAQuad();
//printf("flush\n");
	glFlush();
//printf("glswap %ld %ld\n", (long unsigned int)ogl->did, ogl->wid);
	glXSwapBuffers(ogl->did, ogl->wid);
}

void unmakecurrent_ogl(oglparameters *op)
{
	oglstate *ogl = op->ogl;

// Destroy
	glXMakeCurrent(ogl->did, None, NULL);
	glXDestroyContext(ogl->display, ogl->context);
	XFreeColormap(ogl->display, ogl->xcolormap);
	//XCloseDisplay(display);
}

// Idle callback functions

void oglidle_thread_make_current(oglidle *i)
{
	oglstate *ogl = (oglstate*)&(i->ogl);
	oglparameters op;

	op.fmt = ogl->fmt;
	op.width = ogl->width;
	op.height = ogl->height;
	op.buf = NULL;
	op.ogl = ogl;
	op.widget = i->widget;
	oglidle_add(i, &makecurrent_ogl, (void *)&op);
}

void oglidle_thread_init_ogl(oglidle *i, YUVformats fmt, int width, int height, int linewidth, int codecheight)
{
	oglstate *ogl = (oglstate*)&(i->ogl);
	oglparameters op;
	int imagesize, j;

	op.fmt = ogl->fmt = fmt;
	op.width = ogl->width = width;
	op.height = ogl->height = height;
	op.linewidth = ogl->linewidth = linewidth;
	op.codecheight = ogl->codecheight = codecheight;
	op.ogl = ogl;
	op.widget = i->widget;
	op.buf = NULL;
	switch (op.fmt)
	{
		case RGBA :
			imagesize = op.linewidth * op.height * 4;
			op.buf = malloc(imagesize);
			for(j=0;j<imagesize;j++)
				op.buf[j] = 0x90;			
			break;
		case YUV422:
			break;
		case YUV420:
		default:
			break;
	}
	oglidle_add(i, &init_ogl, (void *)&op);
	if (op.buf)
		free(op.buf);
}

void oglidle_thread_draw_widget(oglidle *i)
{
	oglstate *ogl = (oglstate*)&(i->ogl);
	oglparameters op;

	op.fmt = ogl->fmt;
	op.width = ogl->width;
	op.height = ogl->height;
	op.buf = NULL;
	op.ogl = ogl;
	op.widget = i->widget;
	oglidle_add(i, &drawwidget_ogl, (void *)&op);
}

void oglidle_thread_close_ogl(oglidle *i)
{
	oglstate *ogl = (oglstate*)&(i->ogl);
	oglparameters op;

	op.fmt = ogl->fmt;
	op.width = ogl->width;
	op.height = ogl->height;
	op.buf = NULL;
	op.ogl = ogl;
	op.widget = i->widget;
	oglidle_add(i, &close_ogl, (void *)&op);
}

void oglidle_thread_unmake_current(oglidle *i)
{
	oglstate *ogl = (oglstate*)&(i->ogl);
	oglparameters op;

	op.fmt = ogl->fmt;
	op.width = ogl->width;
	op.height = ogl->height;
	op.buf = NULL;
	op.ogl = ogl;
	op.widget = i->widget;
	oglidle_add(i, &unmakecurrent_ogl, (void *)&op);
}

void oglidle_thread_tex2d_ogl(oglidle *i, char *buf)
{
	oglstate *ogl = (oglstate*)&(i->ogl);
	oglparameters op;

	op.fmt = ogl->fmt;
	op.width = ogl->width;
	op.height = ogl->height;
	op.linewidth = ogl->linewidth;
	op.buf = buf;
	op.ogl = ogl;
	op.widget = i->widget;
	oglidle_add(i, &tex2D_ogl, (void *)&op);
	oglidle_add(i, &drawwidget_ogl, (void *)&op);
}

// Drawing area events
void realize_da_event(GtkWidget *widget, gpointer data)
{
	oglidle *oi = (oglidle*)data;
	oglstate *ogl = (oglstate*)&(oi->ogl);

	// Start idle thread
	oglidle_start(oi);

	// Initalize GL
	oglidle_thread_make_current(oi);

	// Init
	oglidle_thread_init_ogl(oi, ogl->fmt, ogl->width, ogl->height, ogl->linewidth, ogl->codecheight);

//printf("realize_da_event\n");
}

gboolean draw_da_event(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	oglidle *oi = (oglidle*)data;

	oglidle_thread_draw_widget(oi);
//printf("oglidle_thread_draw_widget\n");
	return FALSE;
}

gboolean size_allocate_da_event(GtkWidget *widget, GdkRectangle *allocation, gpointer user_data)
{
	//oglidle *oi = (oglidle*)data;

//printf("resize %d %d\n", allocation->width, allocation->height);
	return FALSE;
}

void destroy_da_event(GtkWidget *widget, gpointer data)
{
	oglidle *oi = (oglidle*)data;

	// Close
	oglidle_thread_close_ogl(oi);

	// Destroy GL
	oglidle_thread_unmake_current(oi);
	
	// Stop idle thread
	oglidle_stop(oi);
//printf("destroy_da_event\n");
}

// Public functions
void reinit_ogl(oglidle *i, YUVformats fmt, int width, int height, int linewidth, int codecheight)
{
	oglidle_thread_close_ogl(i);
	oglidle_thread_init_ogl(i, fmt, width, height, linewidth, codecheight);
//printf("reinit_ogl\n");
}

void draw_texture(oglidle *i, char *buf)
{
	oglidle_thread_tex2d_ogl(i, buf);
}

// linux-graphics.h

#ifndef LINUXGRAPHICS_H
#define LINUXGRAPHICS_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

struct egl_data {
	EGLDisplay d;
	EGLSurface s;
	EGLConfig cfg[1];
	EGLContext cxt;
};
typedef struct egl_data egl_dat;

struct x_data {
	Display *d;
	Window w;
	unsigned kbrTimeout, kbrInterval;
};
typedef struct x_data x_dat;

_Static_assert(sizeof(ssize_t) == 8, "");
_Static_assert(sizeof(long long) == 8, "");
_Static_assert(sizeof(GLbyte) == 1, "");
_Static_assert(sizeof(GLshort) == sizeof(short), "");
_Static_assert(sizeof(GLint) == sizeof(int), "");
_Static_assert(sizeof(GLfloat) == sizeof(float), "");
_Static_assert(sizeof(GLdouble) == sizeof(double), "");
_Static_assert(sizeof(Window) == 8);

#endif

#ifndef INITGL_H
#define INITGL_H

#include <EGL/egl.h>
#ifdef USE_GLES2
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include "std.h"

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

struct keys {
	bool keyA, keyD, keyS, keyW, keyR, keyE;
	bool keyI, keyJ, keyK, keyL, keyU, keyO;
	bool keyLeftBracket, keyRightBracket;
	bool keyLeft, keyRight, keyUp, keyDown;
	bool keyT;
	bool keySpace, keyCTRL, keyEnter;
};
typedef struct keys keys;

_Static_assert(sizeof(ssize_t) == 8, "");
_Static_assert(sizeof(long long) == 8, "");
_Static_assert(sizeof(GLbyte) == 1, "");
_Static_assert(sizeof(GLshort) == sizeof(short), "");
_Static_assert(sizeof(GLint) == sizeof(int), "");
_Static_assert(sizeof(GLfloat) == sizeof(float), "");
_Static_assert(sizeof(GLdouble) == sizeof(double), "");
_Static_assert(sizeof(Window) == 8);

int kill(pid_t, int);
pid_t gettid(void);

char *safe_read(const char *const, ssize_t *);
bool draw(keys *const);
void fakeGluPerspective(void);
void print_curr_mv_matrix(float [4][4]);
bool elapsedTimeGreaterThanNS(struct timespec *const,
	struct timespec *const, int64_t);
void glPrintNum(uint64_t, uint32_t [2]);

#endif

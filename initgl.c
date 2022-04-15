#include "initgl.h"

static egl_dat initializeEgl(void) {
	egl_dat ed;
	ed.d = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	assert(ed.d != EGL_NO_DISPLAY);
	
	EGLint ret;
	
	ret = eglInitialize(ed.d, NULL, NULL);
	assert(ret == EGL_TRUE);
	
#ifdef USE_GLES2
	ret = eglBindAPI(EGL_OPENGL_ES_API);
#else
	ret = eglBindAPI(EGL_OPENGL_API);
#endif
	assert(ret == EGL_TRUE);
	
	const EGLint attrib_list[] = {
		EGL_ALPHA_SIZE, 8,
		EGL_BLUE_SIZE, 8,
#ifdef USE_GLES2
		EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,
#else
		EGL_CONFORMANT, EGL_OPENGL_BIT,
#endif
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};
	EGLint num_config;
	ret = eglChooseConfig(ed.d, attrib_list, ed.cfg, 1, &num_config);
	assert(ret == EGL_TRUE);
	
	// hxxps://community.arm.com/developer/tools-software/oss-platforms
	// /b/android-blog/posts/check-your-context-if-glcreateshader-returns-0
	// -and-gl_5f00_invalid_5f00_operation
#ifdef USE_GLES2
	EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
#else
	EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE };
#endif
	ed.cxt = eglCreateContext(ed.d, ed.cfg[0], EGL_NO_CONTEXT, attribs);
	
	assert(eglGetError() == EGL_SUCCESS);
	
	return ed;
}

static void setXkbrepeat(x_dat *const xd) {
	//XkbGetAutoRepeatRate(xd->d, XkbUseCoreKbd,
	//	&xd->kbrTimeout, &xd->kbrInterval);
	// escape hatch (copy and paste): $ xset r rate 500 20;
	xd->kbrTimeout = 500;
	xd->kbrInterval = 50;
	return;  // debug
	XkbSetAutoRepeatRate(xd->d, XkbUseCoreKbd, 25, 200);
}

static x_dat initializeXwin(void) {
	Status s = XInitThreads();
	assert(0 != s);
	
	x_dat xd;
	xd.d = XOpenDisplay(NULL);  // using Xlib, b/c XCB has poor documentation
	setXkbrepeat(&xd);
	Window x_drw = XDefaultRootWindow(xd.d);
	XSetWindowAttributes xswa = { 0 };
	xswa.event_mask = KeyPressMask | KeyReleaseMask |
		ResizeRedirectMask | ExposureMask;
	xd.w = XCreateWindow(
		xd.d,
		x_drw,
		10,
		10,
		640,
		480,
		0,
		CopyFromParent,
		InputOutput,
		CopyFromParent,
		CWEventMask,
		&xswa
	);
	XMapWindow(xd.d, xd.w);
	XSync(xd.d, false);
	return xd;
}

static void initializeSurface(egl_dat *ed, x_dat *xd) {
	ed->s = eglCreateWindowSurface(ed->d, ed->cfg[0], xd->w, NULL);
	
	int ret;
	ret = eglMakeCurrent(ed->d, ed->s, ed->s, ed->cxt);
	assert(ret == EGL_TRUE);
}

// Terminate the X11 event pump thread and destroy the global mutex.
static void terminatePXEthread(
	thrd_t *const thr,
	mtx_t *const pmtx,
	const pid_t *const pTiddy,
	const char *const pIsTiddyAlive)
{
	int ret;
	
	mutex_lock(pmtx);
	if (*pIsTiddyAlive > 0) {
		ret = kill(*pTiddy, SIGTERM);
		assert(0 == ret);
	}
	ret = thrd_join(*thr, NULL);  // wait for thread's death
	assert(ret == thrd_success);
	//g_isTiddyAlive = false;
	mutex_unlock(pmtx);
	mtx_destroy(pmtx);
}

static bool cleanup(
	egl_dat *ed,
	x_dat *xd,
	thrd_t *const thr,
	mtx_t *const pmtx,
	const pid_t *const pTiddy,
	const char *const pIsTiddyAlive)
{
	int ret;
	
	terminatePXEthread(thr, pmtx, pTiddy, pIsTiddyAlive);
	
	ret = eglMakeCurrent(ed->d, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	assert(ret == EGL_TRUE);
	ret = eglDestroyContext(ed->d, ed->cxt);
	assert(ret != EGL_FALSE && ret != EGL_BAD_DISPLAY &&
		ret != EGL_NOT_INITIALIZED && ret != EGL_BAD_CONTEXT);
	ret = eglDestroySurface(ed->d, ed->s);
	assert(ret != EGL_FALSE && ret != EGL_BAD_DISPLAY &&
		ret != EGL_NOT_INITIALIZED && ret != EGL_BAD_SURFACE);
	ret = eglTerminate(ed->d);
	assert(ret != EGL_FALSE && ret != EGL_BAD_DISPLAY);
	
	ret = XDestroyWindow(xd->d, xd->w);
	assert(ret != BadWindow);
	XkbSetAutoRepeatRate(xd->d, XkbUseCoreKbd, xd->kbrTimeout, xd->kbrInterval);
	ret = XCloseDisplay(xd->d);
	assert(ret != BadGC);

	return true;
}

static void updateKeys(const XEvent *const e, keys *const k, mtx_t *const pmtx) {
	const bool keyState = e->type == KeyPress;
	uint32_t keyCode = e->xkey.keycode;
	
	mutex_lock(pmtx);
	
	if (keyCode == 38 || keyCode == 40) {
		if (keyCode == 38)
			k->keyA = keyState;
		if (keyCode == 40)
			k->keyD = keyState;
		if (k->keyA && k->keyD)
			k->keyA = k->keyD = false;
	} else if (keyCode == 25 || keyCode == 39) {
		if (keyCode == 25)
			k->keyW = keyState;
		if (keyCode == 39)
			k->keyS = keyState;
		if (k->keyS && k->keyW)
			k->keyS = k->keyW = false;
	} else if (keyCode == 27) {  // r
		k->keyR = keyState;
	} else if (keyCode == 31 || keyCode == 45) {  // i, k
		if (keyCode == 31)
			k->keyI = keyState;
		if (keyCode == 45)
			k->keyK = keyState;
		if (k->keyI && k->keyK)
			k->keyI = k->keyK = false;
	} else if (keyCode == 44 || keyCode == 46) {  // j, l
		if (keyCode == 44)
			k->keyJ = keyState;
		if (keyCode == 46)
			k->keyL = keyState;
		if (k->keyJ && k->keyL)
			k->keyJ = k->keyL = false;
	} else if (keyCode == 34 || keyCode == 35) {
		if (keyCode == 34)
			k->keyLeftBracket = keyState;
		if (keyCode == 35)
			k->keyRightBracket = keyState;
		if (k->keyLeftBracket && k->keyRightBracket)
			k->keyLeftBracket = k->keyRightBracket = false;
	} else if (keyCode == 113 || keyCode == 114) {
		if (keyCode == 113)
			k->keyLeft = keyState;
		if (keyCode == 114)
			k->keyRight = keyState;
		if (k->keyLeft && k->keyRight)
			k->keyLeft = k->keyRight = false;
	} else if (keyCode == 111 || keyCode == 116) {
		if (keyCode == 111)
			k->keyUp = keyState;
		if (keyCode == 116)
			k->keyDown = keyState;
		if (k->keyUp && k->keyDown)
			k->keyUp = k->keyDown = false;
	} else if (keyCode == 30 || keyCode == 32) {
		if (keyCode == 30)
			k->keyU = keyState;
		if (keyCode == 32)
			k->keyO = keyState;
		if (k->keyU && k->keyO)
			k->keyU = k->keyO = false;
	} else if (keyCode == 26) {
		k->keyE = keyState;
	} else if (keyCode == 28) {
		k->keyT = keyState;
	} else if (keyCode == 65) {
		k->keySpace = keyState;
	} else if (keyCode == 37) {
		k->keyCTRL = keyState;
	} else if (keyCode == 36) {
		k->keyEnter = keyState;
	} else
		fprintf(stdout, "Key %d %s\n", keyCode,
			keyState ? "KeyPress" : "KeyRelease");
	
	mutex_unlock(pmtx);
}

struct pXEargs {
	x_dat *const xd;
	keys *const k;
	mtx_t *const pmtx;
	pid_t *ptiddy;
	char *pIsTiddyAlive; 
};

static int pumpXEvents(void *p) {
	assert(p);
	struct pXEargs *args = (struct pXEargs *)p;
	uint64_t now_xorg_timediff_sec = 0, keyPresses = 0;
	bool seenFirstEvent = false;
	struct timespec prev = { 0 };

	mutex_lock(args->pmtx);
	*args->ptiddy = gettid();  // non-portable :(
	mutex_unlock(args->pmtx);
	
	for (;;) {
		XEvent e = { 0 };
		struct timespec now = { 0 };
		
		assert(TIME_UTC == timespec_get(&now, TIME_UTC));
		if (elapsedTimeGreaterThanNS(&prev, &now, 1000000000)) {
			prev = now;
			keyPresses = 0;
		}
		
		XLockDisplay(args->xd->d);
		mutex_lock(args->pmtx);
		*args->pIsTiddyAlive = true;
		mutex_unlock(args->pmtx);
		XNextEvent(args->xd->d, &e);
		XUnlockDisplay(args->xd->d);
		
		if (e.type != KeyPress && e.type != KeyRelease)
			continue;
		if (e.type == KeyPress && keyPresses++ > 25)
			fprintf(stderr, "DEBUG: key ignored b/c quota exceeded\n");
		else {
			assert(TIME_UTC == timespec_get(&now, TIME_UTC));
			if (!seenFirstEvent) {
				now_xorg_timediff_sec = now.tv_sec - e.xkey.time/1000;
				seenFirstEvent = true;
			}
			uint64_t approxX11time_sec = now.tv_sec - now_xorg_timediff_sec;
			uint64_t keyAge = e.xkey.time/1000 - approxX11time_sec;
			if (e.xkey.time/1000 < approxX11time_sec)  // approx = inaccurate
				keyAge = approxX11time_sec - e.xkey.time/1000;
			if (keyAge < 2) {
				if (e.xkey.keycode == 24) {  // 'q'
					mutex_lock(args->pmtx);
					*args->pIsTiddyAlive = false;
					mtx_unlock(args->pmtx);
					//assert(ret = thrd_success);  // XXX debug broken
					return 0;
				}
				updateKeys(&e, args->k, args->pmtx);
			} else
				fprintf(stderr, "DEBUG: key age %llu, ignored\n",
					(long long unsigned)keyAge);
		}
	}
	assert(NULL);
}

// Wait for g_isTiddyAlive to be initialized.
static void waitForThreadToLaunch(mtx_t *pmtx, const char *const pIsTiddyAlive) {
	for (;;) {
		bool shouldBreak = false;
		mutex_lock(pmtx);
		if (*pIsTiddyAlive != -1)  // has been initialized
			shouldBreak = true;
		mutex_unlock(pmtx);
		if (shouldBreak)
			break;
		//fprintf(stderr, "waiting for thread to launch\n");
	}
}

static bool fn(void) {
	keys k = { 0 };
	thrd_t thr;
	mtx_t tiddy_mtx;
	pid_t tiddy = 1;  // Thread ID to DestroY
	char isTiddyAlive = -1;  // >0 means alive
	
	egl_dat ed = initializeEgl();
	x_dat xd = initializeXwin();
	initializeSurface(&ed, &xd);
	
	glViewport(0, 0, 640, 640);
	glEnable(GL_BLEND);
	// https://gamedev.stackexchange.com/questions/32027/
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	draw(&k);  // initial paint (before multithreading occurs)
	eglSwapBuffers(ed.d, ed.s);
	
	int ret;
	ret = mtx_init(&tiddy_mtx, mtx_plain);
	assert(ret == thrd_success);
	struct pXEargs pXEargs = (struct pXEargs){
		&xd,
		&k,
		&tiddy_mtx,
		&tiddy,
		&isTiddyAlive
	};
	ret = thrd_create(&thr, pumpXEvents, &pXEargs);
	assert(ret == thrd_success);
	waitForThreadToLaunch(&tiddy_mtx, &isTiddyAlive);

	struct timespec prev = { 0 };
	uint64_t frames = 0;
	for (;;) {
		struct timespec now = { 0 };
		assert(TIME_UTC == timespec_get(&now, TIME_UTC));
		if (elapsedTimeGreaterThanNS(&prev, &now, 1000000000)) {
			prev = now;
			fprintf(stderr, "DEBUG: %llu frames\n", (long long unsigned)frames);
			frames = 0;
		}

		mutex_lock(&tiddy_mtx);
		ret = draw(&k);
		mutex_unlock(&tiddy_mtx);
		if (!isTiddyAlive || !ret) {
			cleanup(&ed, &xd, &thr, &tiddy_mtx, &tiddy, &isTiddyAlive);
			return !isTiddyAlive;  // ???
		}
		
		eglSwapBuffers(ed.d, ed.s);
		frames++;
	}
	
	assert(NULL);
	return true;
}

void terminate(void);

int main(int argc, char *argv[]) {
	argv[0][0] += argc - argc;
	
	fn();
	terminate();
}

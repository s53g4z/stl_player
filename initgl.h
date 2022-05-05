#ifndef INITGL_H
#define INITGL_H

#ifndef MACOSX
#include "linux-graphics.h"
#endif

#include "std.h"
#include "util.h"

int kill(pid_t, int);
pid_t gettid(void);

bool draw(keys *const, const int *const, const int *const);
bool elapsedTimeGreaterThanNS(struct timespec *const,
	struct timespec *const, int64_t);

#endif

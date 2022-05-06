#ifndef STD_H
#define STD_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <float.h>
#include <time.h>
#if (!defined(MACOSX))  // i.e., is Linux
#include <threads.h>
#endif
#if (defined(M1MAC))
#include <pthread.h>
#endif
#include <signal.h>
#include <limits.h>

#undef bool
#undef false
#undef true
typedef unsigned char bool;
#define true 1
#define false 0

#ifndef MACOSX
#undef SSIZE_MAX
#define SSIZE_MAX ((ssize_t)(~0ULL >> 1))
#undef SIZE_MAX
#define SIZE_MAX ((size_t)(~0ULL))
#endif

#endif

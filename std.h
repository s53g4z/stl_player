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
#include <threads.h>
#include <signal.h>
#include <limits.h>

typedef unsigned char bool;
#define true 1
#define false 0

#undef SSIZE_MAX
#define SSIZE_MAX ((ssize_t)(~0ULL >> 1))
#undef SIZE_MAX
#define SIZE_MAX ((size_t)(~0ULL))

#endif

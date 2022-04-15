#include "stlplayer.h"
#include "initgl.h"  // gl includes
#include "util.h"  // forward declarations for util.c

char gSelf[4096];
int gSelf_len;

const int32_t NSONE = 1000000000;  // nanoseconds in 1 second ( = 1 billion)

// Helper for safe_read.
char *safe_read_fail(char *buf, ssize_t *has_read, int fd) {
	free(buf);
	*has_read = 0;
	assert(close(fd) == 0);
	return NULL;
}

char *safe_read(const char *const filename, ssize_t *has_read) {
	*has_read = 0;

	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;
	
	char *buf = malloc(1);
	ssize_t bufsiz = 1;
	for (;;) {
		if (*has_read > SSIZE_MAX - (ssize_t)buf) {
			return safe_read_fail(buf, has_read, fd);
		}
		ssize_t got = read(fd, buf + *has_read, bufsiz - *has_read);
		if (got == 0) {  // EOF
			assert(close(fd) == 0);
			return buf;
		}
		if (got < 0) {
			return safe_read_fail(buf, has_read, fd);
		}
		if (*has_read > SSIZE_MAX - got) {
			return safe_read_fail(buf, has_read, fd);
		}
		*has_read += got;
		assert(bufsiz >= *has_read);
		if (bufsiz == *has_read) {
			if (bufsiz > SSIZE_MAX / 2) {
				return safe_read_fail(buf, has_read, fd);
			}
			bufsiz *= 2;
			char *bigger = realloc(buf, bufsiz);
			if (!bigger) {
				return safe_read_fail(buf, has_read, fd);
			}
			buf = bigger;
		}
	}
	
	assert(0);
	return NULL;
}

bool elapsedTimeGreaterThanNS(struct timespec *const prev,
	struct timespec *const now, int64_t ns) {
	if (now->tv_sec - prev->tv_sec != 0)
		return true;  // lots of time has elapsed already
	return now->tv_nsec - prev->tv_nsec > ns;
}

// Never-null malloc().
void *nnmalloc(size_t sz) {
	void *rv = malloc(sz);
	if (!rv)
		exit(12);
	return rv;
}

// Never-null realloc().
void *nnrealloc(void *oldptr, size_t newsz) {
	void *rv = realloc(oldptr, newsz);
	if (!rv)
		exit(12);
	return rv;
}

bool isWhitespace(char ch) {
	return ch == ' ' || ch == '\t' || ch == '\n';
}

void trimWhitespace(const char **section, size_t *section_len) {
	while (isWhitespace(**section)) {
		(*section)++;
		(*section_len)--;
	}
	while (isWhitespace(*(*section + *section_len - 1)))
		(*section_len)--;
}

// Construct the stl's objects member.
void init_lvl_objects (stl *const lvl) {
	assert(lvl->objects_cap == 0);
	lvl->objects_cap = 1;
	lvl->objects = nnmalloc(lvl->objects_cap * sizeof(stl_obj));
}

// Push to stl's objects member.
void pushto_lvl_objects(stl *const lvl, stl_obj *obj) {
	if (lvl->objects_len == lvl->objects_cap) {
		assert(lvl->objects_cap < SIZE_MAX / 2);
		lvl->objects_cap *= 2;
		lvl->objects = nnrealloc(lvl->objects,
			lvl->objects_cap * sizeof(stl_obj));
	}
	(lvl->objects)[lvl->objects_len++] = *obj;
}

// Return the length of n written out as an ASCII string. Works on |n|<=99999
int intAsStrLen(int n) {
	//assert(n >= 0 && n <= 99999);
	int maybeDash = 0;
	if (n < 0)
		maybeDash = 1;
	assert(n != INT_MIN && abs(n) <= 99999);
	n = abs(n);
	
	if (n > 9999)
		return 5 + maybeDash;
	else if (n > 999)
		return 4 + maybeDash;
	else if (n > 99)
		return 3 + maybeDash;
	else if (n > 9)
		return 2 + maybeDash;
	else
		return 1 + maybeDash;
}

// Debugging function. Print a TM to stderr.
void printTM(uint8_t **const tm, const int width, const int height) {
	for (int h = 0; h < height; h++) {
		for (int w = 0; w < width; w++)
			fprintf(stderr, "%.3d ", tm[h][w]);
		fprintf(stderr, "\n");
	}
}

void must(unsigned long long condition) {
	if (!condition) {
		raise(SIGABRT);
		raise(SIGFPE);
		raise(SIGILL);
		raise(SIGSEGV);
		raise(SIGTERM);
		exit(1);
	}
}

// xxx bad includes
ssize_t readlink(const char *path, char *buf, size_t bufsize);

// Populate gSelf with the path to the executable (and gSelf_len appropriately).
void findSelfOnLinux(void) {
	ssize_t self_len = readlink("/proc/self/exe", gSelf, 4096);
	if (self_len == -1) {
		fprintf(stderr, "ERROR: could not find executable path\n");
		must(0);
		return;
	} else
		gSelf[self_len] = '\0';
	
	while (gSelf[self_len] != '/' && self_len != 0)
		self_len--;
	gSelf[self_len + 1] = '\0';
	assert((size_t)(self_len + 1) == strlen(gSelf));
	gSelf_len = self_len + 1;
	
	fprintf(stderr, "DEBUG: path w/out filename: %s\n", gSelf);
}

// Lock a mutex. Always succeeds.
void mutex_lock(mtx_t *const mtx) {
	int ret = mtx_lock(mtx);
	must(ret == thrd_success);
}

// Unlock a mutex. Always succeeds.
void mutex_unlock(mtx_t *const mtx) {
	int ret = mtx_unlock(mtx);
	must(ret == thrd_success);
}

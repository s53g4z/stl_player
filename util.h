// util.h

#ifndef UTIL_H
#define UTIL_H

extern char gSelf[];
extern int gSelf_len;

struct Point {
	int x, y;
};
typedef struct Point Point;

extern const int32_t NSONE;  // nanoseconds in 1 second ( = 1 billion)

void *nnmalloc(size_t);

char *safe_read(const char *const filename, ssize_t *has_read);
bool isWhitespace(char ch);
void trimWhitespace(const char **section, size_t *section_len);
int intAsStrLen(int n);
void printTM(uint8_t **const tm, const int width, const int height);
void must(unsigned long long condition);
void findSelfOnLinux(void);
void mutex_lock(mtx_t *const mtx);
void mutex_unlock(mtx_t *const mtx);

#endif

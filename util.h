#ifndef UTIL_H
#define UTIL_H

extern char gSelf[];
extern int gSelf_len;

struct Point {
	int x, y;
};
typedef struct Point Point;

struct char_stack {
	char *arr;
	size_t arr_len;
	size_t arr_capacity;
};
typedef struct char_stack char_stack;

char *safe_read(const char *const filename, ssize_t *has_read);
bool isWhitespace(char ch);
void trimWhitespace(const char **section, size_t *section_len);
int intAsStrLen(int n);
void printTM(uint8_t **const tm, const int width, const int height);
void must(unsigned long long condition);
void findSelfOnLinux(void);

#endif

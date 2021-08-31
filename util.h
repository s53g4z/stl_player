#ifndef UTIL_H
#define UTIL_H

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

#endif

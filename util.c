#include "stlplayer.h"
#include "initgl.h"  // gl includes
#include "util.h"  // forward declarations for util.c

// Overwrite v with its normalised (unit vector) form.
void normalise(float v[3]) {
	float len = sqrt(pow(v[0],2) + pow(v[1],2) + pow(v[2],2));
	if (len == 0) {
		fprintf(stderr, "WARN: normalise(): using len = 1\n");
		len = 1;  // will give the wrong answer ...
	}
	for (size_t i = 0; i < 3; i++) {
		v[i] /= len;
	}
}

// Return u*v in the third argument.
void crossProduct(float u[3], float v[3], float normal[3]) {
	normal[0] = u[1]*v[2] - v[1]*u[2];
	normal[1] = u[2]*v[0] - v[2]*u[0];
	normal[2] = u[0]*v[1] - v[0]*u[1];
}

void fakeGluPerspective(void) {
#ifndef USE_GLES2
	const float fovy = 45, aspect = 1, zNear = 0.99, zFar = 425;
	const float f = 1.0/(tan(fovy/2));
	const float matrix[] = {
		f/aspect, 0, 0, 0,
		0, f, 0, 0,
		0, 0, (zFar+zNear)/(zNear-zFar), (2*zFar*zNear)/(zNear-zFar),
		0, 0, -1, 0,
	};
	glMultMatrixf(matrix);
#endif
}

void print_curr_mv_matrix(float params[4][4]) {
	for (size_t c = 0; c < 4; c++)
		for (size_t r = 0; r < 4; r++)
			fprintf(stderr, "%+.2f %s", params[r][c], (r+1)%4 == 0 ? "\n" : "");
	fprintf(stderr, "\n");
}

char *safe_read(const char *const filename, ssize_t *has_read) {
	*has_read = 0;  // DO NOT COMMIT! xxx warn error hack
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;
	
	char *buf = malloc(1);
	ssize_t bufsiz = 1;
	*has_read = 0;
	for (;;) {
		if (*has_read > SSIZE_MAX - (ssize_t)buf) {
			free(buf);
			return NULL;
		}
		ssize_t got = read(fd, buf + *has_read, bufsiz - *has_read);
		if (got == 0)  // EOF
			return buf;
		if (got < 0) {
			free(buf);
			return NULL;
		}
		if (*has_read > SSIZE_MAX - got) {
			free(buf);
			return NULL;
		}
		*has_read += got;
		assert(bufsiz >= *has_read);
		if (bufsiz == *has_read) {
			if (bufsiz > SSIZE_MAX / 2) {
				free(buf);
				return NULL;
			}
			bufsiz *= 2;
			char *bigger = realloc(buf, bufsiz);
			if (!bigger) {
				free(buf);
				return NULL;
			}
			buf = bigger;
		}
	}
	
	return NULL;
}

bool elapsedTimeGreaterThanNS(struct timespec *const prev,
	struct timespec *const now, int64_t ns) {
	if (now->tv_sec - prev->tv_sec != 0)
		return true;  // lots of time has elapsed already
	return now->tv_nsec - prev->tv_nsec > ns;
}

struct coord {
	float x;
	float y;
};

// Helper fn for glPrintNum.
void calculate_texture_coordinates(int dig, struct coord tc[4]) {
	if (dig == 0) {
		tc[0] = (struct coord) { .x = 0.33, .y = 0.05 };
		tc[1] = (struct coord) { .x = 0.66, .y = 0.05 };
		tc[2] = (struct coord) { .x = 0.66, .y = 0.02 + 0.33/2 };
		tc[3] = (struct coord) { .x = 0.33, .y = 0.02 + 0.33/2 };
	} else {
		float tox = (dig-1)%3 * 0.33;  // texture offset x
		float toy = 0.33;  // texture offset y
		if (dig >= 1 && dig <= 3) {
			toy = 0.66;
		} else if ( dig >= 7 && dig <= 9) {
			toy = 0.00;
		}
		tc[0] = (struct coord) { .x = 0.00 + tox, .y = 0.00 + toy };
		tc[1] = (struct coord) { .x = 0.33 + tox, .y = 0.00 + toy };
		tc[2] = (struct coord) { .x = 0.33 + tox, .y = 0.33 + toy };
		tc[3] = (struct coord) { .x = 0.00 + tox, .y = 0.33 + toy };
	}
}

#ifdef USING_FULL_OPENGL
// Helper fn for glPrintNum.
void render_numbers(struct coord tc[4], ssize_t offsetX) {
	glEnable(GL_TEXTURE_2D);
	//glBindTexture(GL_TEXTURE_2D, textures[1]);
	glBegin(GL_QUADS);
		glColor3f(0.0, 1.0, 0.0);  // color backup in case texturing fails
		glTexCoord2f(tc[0].x, tc[0].y);
		glVertex3f(0 + offsetX, 0, -1);  // lower left
		glTexCoord2f(tc[1].x, tc[1].y);
		glVertex3f(1 + offsetX, 0, -1);  // lower right
		glTexCoord2f(tc[2].x, tc[2].y);
		glVertex3f(1 + offsetX, 1, -1);  // upper right
		glTexCoord2f(tc[3].x, tc[3].y);
		glVertex3f(0 + offsetX, 1, -1);  // upper left
	glEnd();
	glDisable(GL_TEXTURE_2D);
}

// Print a number to the left of the origin.
void glPrintNum(uint64_t num, uint32_t textures[2]) {
	glBindTexture(GL_TEXTURE_2D, textures[1]);  // the number texture
	
	ssize_t offsetX = -1;
	for (;;) {
		int dig = num % 10;
		num /= 10;
		
		struct coord tc[4];  // texture coordinates to use
		calculate_texture_coordinates(dig, tc);
		render_numbers(tc, offsetX);
		
		if (num == 0)
			break;
		offsetX -= 1;
	}
}
#endif

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

char_stack char_stack_init() {
	char_stack ch_stack;
	ch_stack.arr_len = 0;
	ch_stack.arr_capacity = 1;
	ch_stack.arr = nnmalloc(ch_stack.arr_capacity);
	return ch_stack;
}

void char_stack_destroy(char_stack *const ch_stack) {
	free(ch_stack->arr);
	ch_stack->arr_len = ch_stack->arr_capacity = 0;
}

void char_stack_print(const char_stack *const ch_stack) {
	for (size_t i = 0; i < ch_stack->arr_len; i++)
		fprintf(stderr, "[%c] ", ch_stack->arr[i]);
	fprintf(stderr, "\n");
}

size_t char_stack_len(const char_stack *const ch_stack) {
	return ch_stack->arr_len;
}

void char_stack_push(char_stack *const ch_stack, char ch) {
	assert(ch_stack->arr_capacity != 0);  // UAF guard
	if (ch_stack->arr_len == ch_stack->arr_capacity) {
		assert(ch_stack->arr_capacity < SIZE_MAX / 2);
		ch_stack->arr_capacity *= 2;
		ch_stack->arr = nnrealloc(ch_stack->arr, ch_stack->arr_capacity);
	}
	ch_stack->arr[(ch_stack->arr_len)++] = ch;
}

char char_stack_pop(char_stack *const ch_stack) {
	assert(ch_stack->arr_capacity != 0 && ch_stack->arr_len > 0);
	return ch_stack->arr[--(ch_stack->arr_len)];
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

// Return the length of n written out as an ASCII string. Works on 0<n<=99999
int intAsStrLen(int n) {
	assert(n > 0 && n <= 99999);
	if (n > 9999)
		return 5;
	else if (n > 999)
		return 4;
	else if (n > 99)
		return 3;
	else if (n > 9)
		return 2;
	else
		return 1;
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
	if (!condition)
		exit(1);
}

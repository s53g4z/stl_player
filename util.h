// util.h

#ifndef UTIL_H
#define UTIL_H

#ifndef MACOSX
#include "linux-graphics.h"
#else
#include <OpenGL/OpenGL.h>
#endif
#include "std.h"

extern char gSelf[];
extern int gSelf_len;

extern const int32_t NSONE;  // nanoseconds in 1 second ( = 1 billion)

struct keys {
	bool keyA, keyD, keyS, keyW, keyR, keyE;
	bool keyI, keyJ, keyK, keyL, keyU, keyO;
	bool keyLeftBracket, keyRightBracket;
	bool keyLeft, keyRight, keyUp, keyDown;
	bool keyT;
	bool keySpace, keyCTRL, keyEnter, keyEsc;
	bool keyPgUp, keyPgDown;
};
typedef struct keys keys;

enum stl_obj_type {
	STL_WIN,
	STL_BLOCK,
	STL_BONUS,
	STL_BRICK,
	STL_BRICK_DESTROYED,
	STL_INVISIBLE,
	STL_COIN,
	STL_TUX,
	STL_TUX_DEAD,
	STL_TUX_ASCENDED,
	STL_DEAD,
	STL_DEAD_MRICEBLOCK,
	STL_KICKED_MRICEBLOCK,
	STL_INVALID_OBJ,
	STL_NO_MORE_OBJ,
	SNOWBALL,
	MRICEBLOCK,
	STL_BOMB,
	STL_BOMB_TICKING,
	STL_BOMB_EXPLODING,
	STALACTITE,
	BOUNCINGSNOWBALL,
	FLYINGSNOWBALL,
	MONEY,
	SPIKY,
	JUMPY,
	STL_FLAME,
};

struct stl_obj {
	enum stl_obj_type type;
	int x, y;
};
typedef struct stl_obj stl_obj;

struct point {
	int x, y;
	struct point *next;
};
typedef struct point point;

struct level {
	bool hdr;
	int version;
	char *author, *name;
	int width, height, start_pos_x, start_pos_y;
	char *background, *music;
	int time, gravity;
	char *particle_system, *theme;
	uint8_t **interactivetm, **backgroundtm, **foregroundtm;
	stl_obj *objects;
	size_t objects_len, objects_cap;
	point *reset_points;
};
typedef struct level stl;

void *nnmalloc(size_t);

char *safe_read(const char *const filename, ssize_t *has_read);
bool isWhitespace(char ch);
void trimWhitespace(const char **section, size_t *section_len);
int intAsStrLen(int n);
void printTM(uint8_t **const tm, const int width, const int height);
void must(unsigned long long condition);
void findSelfOnLinux(void);
#if (!defined(MACOSX))  // i.e., is Linux
void mutexLock(mtx_t *const mtx);
void mutexUnlock(mtx_t *const mtx);
#endif
#if defined(MACOSX)
void findSelfOnMac(void);
#endif

#endif

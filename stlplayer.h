// stlplayer.h

#ifndef STLPLAYER_H
#define STLPLAYER_H

#include "std.h"
#include "util.h"

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
	STL_FLAME
};

struct WorldItem {
	enum stl_obj_type type;
	int x, y;
	int width, height;
	int state;  // manually set
	float speedX, speedY;
	uint32_t texnam, texnam2;
	bool gravity, patrol;
	void (*frame)(struct WorldItem *const w);
	struct WorldItem *next;
};
typedef struct WorldItem WorldItem;

typedef WorldItem Tux;

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
	char *author;
	char *name;
	int width, height;
	int start_pos_x, start_pos_y;
	char *background, *music;
	int time, gravity;
	char *particle_system, *theme;
	uint8_t **interactivetm, **backgroundtm, **foregroundtm;
	stl_obj *objects;  // array of struct stl_obj
	size_t objects_len, objects_cap;
	point *reset_points;
};
typedef struct level stl;

void *nnmalloc(size_t sz);
void *nnrealloc(void *, size_t);

stl_obj getSTLobj(const char **section, size_t *const section_len);
void init_lvl_objects(stl *const lvl);
void pushto_lvl_objects(stl *const lvl, stl_obj *obj);
bool parseTM(uint8_t ***ptm, const int width, const int height,
	const char **section, size_t *const section_len);
bool nextWordIs(const char *const word, const char **, size_t *);
bool writeStrTo(char **destination, const char **section, size_t *section_len);
stl lrFailCleanup(const char *const level_orig, stl *lvl);
stl levelReader(const char *const);
void stlPrinter(const stl *const lvl);

#endif

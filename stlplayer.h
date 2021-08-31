// stlplayer.h

#ifndef STLPLAYER_H
#define STLPLAYER_H

#include "std.h"

enum stl_obj_type {
	STL_WIN,
	STL_BLOCK,
	STL_BONUS,
	STL_BRICK,
	STL_COIN,
	STL_PLAYER,
	STL_PLAYER_DEAD,
	STL_PLAYER_ASCENDED,
	STL_DEAD,
	STL_INVALID_OBJ,
	STL_NO_MORE_OBJ,
	SNOWBALL,
	MRICEBLOCK,
	MRBOMB,
	STALACTITE,
	BOUNCINGSNOWBALL,
	FLYINGSNOWBALL,
	MONEY
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
};
typedef struct WorldItem WorldItem;

typedef WorldItem Player;

struct stl_obj {
	enum stl_obj_type type;
	int x, y;
};
typedef struct stl_obj stl_obj;

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
};
typedef struct level stl;

void *nnmalloc(size_t sz);
void *nnrealloc(void *, size_t);
WorldItem *worldItem_new(enum stl_obj_type, int, int, int, int, float, float, bool, char *,
	void (*)(WorldItem *const), bool, bool);
int leftOf(const WorldItem *const);
int rightOf(const WorldItem *const);
int topOf(const WorldItem *const);
int bottomOf(const WorldItem *const);

// debug
void initGLTextureNam(const uint32_t texnam, const char *const imgnam, bool ff);
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

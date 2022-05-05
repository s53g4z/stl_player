// stlplayer.h

#ifndef STLPLAYER_H
#define STLPLAYER_H

#ifndef MACOSX
#include "linux-graphics.h"
#else
#include <OpenGL/OpenGL.h>
#endif
#include "std.h"
#include "util.h"

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

extern bool displayingMessage;

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
bool draw(keys *const, const int *const, const int *const);
void core(keys *const, bool, const int *const, const int *const);

#endif

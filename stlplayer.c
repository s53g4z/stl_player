// stlplayer.c

#include "initgl.h"
#include "stlplayer.h"

const int SCREEN_WIDTH = 640, SCREEN_HEIGHT = 480;
const int TILE_WIDTH = 32, TILE_HEIGHT = 32;
int gScrollOffset = 0;

typedef int Direction;
enum { LEFT, RIGHT, UP, DOWN,
	GDIRECTION_HORIZ, GDIRECTION_VERT, GDIRECTION_BOTH, };
typedef int GeneralDirection;

WorldItem **worldItems;  // array of pointers to WorldItem
size_t worldItems_len, worldItems_cap;

static stl lvl;

Player *player;

WorldItem *worldItem_new(enum stl_obj_type type, int x, int y, int wi, int h,
	float spx, float spy, bool gravity, char *imgnam,
	void(*frame)(WorldItem *const), bool mirrable, bool patrol) {	
	assert(wi > 0 && h > 0);
	WorldItem *w = nnmalloc(sizeof(WorldItem));
	w->type = type;
	w->x = x;
	w->y = y;
	w->width = wi;
	w->height = h;
	w->speedX = spx;
	w->speedY = spy;
	w->frame = frame;
	w->gravity = gravity;
	w->patrol = patrol;  // just a value, does nothing on its own
	
	w->texnam = 0;
	w->texnam2 = 0;
	if (imgnam) {
		glGenTextures(1, &w->texnam);
		initGLTextureNam(w->texnam, imgnam, false);
		if (mirrable) {
			glGenTextures(1, &w->texnam2);
			initGLTextureNam(w->texnam2, imgnam, true);
		}
	}
	
	return w;
}

int leftOf(const WorldItem *const w) {
	return w->x;
}

int rightOf(const WorldItem *const w) {
	assert(w->x < INT_MAX - w->width);
	return w->x + w->width;
}

int topOf(const WorldItem *const w) {
	return w->y;
}

int bottomOf(const WorldItem *const w) {
	assert(w->y < INT_MAX - w->height);
	return w->y + w->height;
}

// Return true if c is between a and b.
bool isBetween(const int c, const int a, const int b) {
	assert(a <= b);
	return (a <= c && c <= b);
}

bool isCollidingHorizontally(const WorldItem *const w,
	const WorldItem *const v) {
	return v != w && (
		isBetween(topOf(v), topOf(w), bottomOf(w)) ||
		isBetween(bottomOf(v), topOf(w), bottomOf(w)) ||
		(topOf(w) >= topOf(v) && bottomOf(w) <= bottomOf(v)) ||
		(topOf(w) <= topOf(v) && bottomOf(w) >= bottomOf(v))
	);
}

bool isCollidingVertically(const WorldItem *const w,
	const WorldItem *const v) {
	return v != w && (
		isBetween(leftOf(v), leftOf(w), rightOf(w)) ||
		isBetween(rightOf(v), leftOf(w), rightOf(w)) ||
		(leftOf(w) >= leftOf(v) && rightOf(w) <= rightOf(v)) ||
		(leftOf(w) <= leftOf(v) && rightOf(w) >= rightOf(v))
	);
}

// iCW helper. Expand w in either the horizontal or vertical direction.
void iCW_expand_w(WorldItem *const w, GeneralDirection dir) {
	assert(w->x >= INT_MIN + 1 && w->y  >= INT_MIN + 1);
	assert(w->width <= INT_MAX - 2 && w->height <= INT_MAX - 2);
	if (dir == GDIRECTION_HORIZ || dir == GDIRECTION_BOTH) {
		w->x--;
		w->width += 2;
	}
	if (dir == GDIRECTION_VERT || dir == GDIRECTION_BOTH) {
		w->y--;
		w->height += 2;
	}
}

// iCW helper. Shrink w in either the horizontal or vertical direction.
void iCW_shrink_w(WorldItem *const w, GeneralDirection dir) {
	if (dir == GDIRECTION_HORIZ || dir == GDIRECTION_BOTH) {
		w->x++;
		w->width -= 2;
	}
	if (dir == GDIRECTION_VERT || dir == GDIRECTION_BOTH) {
		w->y++;
		w->height -= 2;
	}
}

// Return an array of WorldItems w is colliding with. The caller must free rv
// (but not the pointers stored in rv).
WorldItem **isCollidingWith(WorldItem *const w, size_t *const collisions_len,
	GeneralDirection gdir) {
	assert(gdir == GDIRECTION_HORIZ || gdir == GDIRECTION_VERT ||
		gdir == GDIRECTION_BOTH);
	iCW_expand_w(w, gdir);
	WorldItem **rv = malloc(sizeof(WorldItem *) * 1);
	*collisions_len = 0;
	size_t collisions_capacity = 1;
	for (size_t i = 0; i < worldItems_len; i++) {
		if (!isCollidingHorizontally(w, worldItems[i]) ||
			!isCollidingVertically(w, worldItems[i]))
			continue;
		
		if (*collisions_len == collisions_capacity) {
			assert(collisions_capacity <= SIZE_MAX / 2);
			collisions_capacity *= 2;
			WorldItem **newrv = nnrealloc(rv, sizeof(WorldItem *) *
				collisions_capacity);
			rv = newrv;
		}
		rv[(*collisions_len)++] = worldItems[i];
	}
	iCW_shrink_w(w, gdir);
	return rv;
}

// Helper for canMoveTo.
int canMoveX(WorldItem *const w) {
	if (w->speedX == 0)
		return 0;
	int canMove = 0;
	if (w->speedX > 0)
		assert(w->x <= INT_MAX - w->speedX);
	else
		assert(w->x >= INT_MIN - w->speedX);
	for (size_t i = 0; i < fabs(w->speedX); i++) {
		float origX = w->x;
		if (w->speedX > 0)
			w->x += i;
		else
			w->x -= i;
		bool shouldBreak = false;
		//if ((w->speedX < 0 && leftOf(w) - 1 == 0) || (w->speedX > 0 &&
		//	rightOf(w) + 1 == SCREEN_WIDTH))
		//	shouldBreak = true;
		if (w == player && ((w->speedX < 0 && leftOf(w) - 1 == 0) ||
			(gScrollOffset + SCREEN_WIDTH >= lvl.width * TILE_WIDTH &&
			w->speedX > 0 && rightOf(w) + 1 == SCREEN_WIDTH)))
			shouldBreak = true;
		else {
			size_t collisions_len;
			WorldItem **colls = isCollidingWith(w, &collisions_len,
				GDIRECTION_HORIZ);
			for (size_t i = 0; i < collisions_len; i++) {
				if (colls[i]->type == STL_COIN)
					continue;  // ignore coin collisions
				if ((w->speedX > 0 && rightOf(w) + 1 == leftOf(colls[i])) ||
					(w->speedX < 0 && leftOf(w) - 1 == rightOf(colls[i]))) {
					shouldBreak = true;
					break;
				}
			}
			free(colls);
		}
		w->x = origX;
		if (shouldBreak)
			break;
		if (w->speedX > 0)
			canMove++;
		else
			canMove--;
	}
	return canMove;
}

// Helper for canMoveTo.
int canMoveY(WorldItem *const w) {
	if (w->speedY == 0)
		return 0;
	int canMove = 0;  // todo: add overflow assertions
	for (size_t i = 0; i < fabs(w->speedY); i++) {
		int origY = w->y;
		if (w->speedY > 0)
			w->y += i;
		else
			w->y -= i;
		bool shouldBreak = false;
		if (w->speedY > 0 && bottomOf(w) + 1 == SCREEN_HEIGHT) {
			shouldBreak = true;
		//} else if (w->speedY < 0 && topOf(w) - 1 == 0) {
		//	w->speedY *= -1;  // bonk
		//	shouldBreak = true;
		} else {
			size_t collisions_len;
			WorldItem **colls = isCollidingWith(w, &collisions_len,
				GDIRECTION_VERT);
			for (size_t i = 0; i < collisions_len; i++) {
				if (colls[i]->type == STL_COIN)
					continue;  // ignore coin collisions
				if (w->speedY > 0 && bottomOf(w) + 1 == topOf(colls[i])) {
					shouldBreak = true;
					break;
				} else if (w->speedY < 0 &&
					topOf(w) - 1 == bottomOf(colls[i])) {
					w->speedY *= -1;  // bonk
					shouldBreak = true;
					break;
				}
			}
			free(colls);
		}
		w->y = origY;
		if (shouldBreak)
			break;
		if (w->speedY > 0)
			canMove++;
		else
			canMove--;
	}
	return canMove;
}

static int canMoveTo(WorldItem *const w, const GeneralDirection dir) {
	assert(dir == GDIRECTION_HORIZ || dir == GDIRECTION_VERT);
	if (dir == GDIRECTION_HORIZ)
		return canMoveX(w);
	else //if (dir == GDIRECTION_VERT)
		return canMoveY(w);
	//assert(NULL);
	//return 0;
}

static void player_toggle_size() {
	if (player->height == 32) {
		player->height = 64;
		player->y -= 32;
	} else {
		player->height = 32;
		player->y += 32;
	}
}

void deletefrom_worldItems(int index) {
	assert(index >= 0 && (size_t)index < worldItems_len);
	free(worldItems[index]);
	memmove(&(worldItems[index]), &(worldItems[index+1]),
		(worldItems_len - index - 1) * sizeof(WorldItem *));
	worldItems_len--;
}

void maybeScrollScreen() {
	if (player->x <= SCREEN_WIDTH / 3 ||
		gScrollOffset + SCREEN_WIDTH >= lvl.width * TILE_WIDTH)
		return;
	
	int diff = player->x - SCREEN_WIDTH / 3;
	for (size_t i = 0; i < worldItems_len; i++) {
		WorldItem *const w = worldItems[i];
		w->x -= diff;
	}
	gScrollOffset += diff;
	for (size_t i = 0; i < worldItems_len; i++) {
		const WorldItem *const w = worldItems[i];
		if (w->x < -100)
			deletefrom_worldItems(i--);
	}
}

static void processInput(const keys *const k) {
	assert(k && SCREEN_WIDTH && SCREEN_HEIGHT);
	
	if (k->keyD) {
		player->speedX = fabs(player->speedX);
		player->x += canMoveTo(player, GDIRECTION_HORIZ);
	}
	if (k->keyA) {
		assert(player->speedX != INT_MIN);
		if (player->speedX > 0)
			player->speedX *= -1;
		player->x += canMoveTo(player, GDIRECTION_HORIZ);
	}
	if (k->keyW)
		player->speedY = -10;
	if (k->keyR)
		player_toggle_size();
	
	maybeScrollScreen();
}

// Run the frame functions of elements of worldItems.
static void applyFrame() {
	for (size_t i = 0; i < worldItems_len; i++)  // add to end ok, don't delete!
		worldItems[i]->frame(worldItems[i]);
		
	// reap dead badguys and collected coins
	for (size_t i = 0; i < worldItems_len; i++)
		if (worldItems[i]->type == STL_DEAD)
			deletefrom_worldItems(i--);
}

static void applyGravity() {
	for (size_t i = 0; i < worldItems_len; i++) {
		WorldItem *const w = worldItems[i];
		if (w->gravity == false)  // not affected by gravity
			continue;
		int moveY = canMoveTo(w, GDIRECTION_VERT);
		if (moveY == 0)
			w->speedY = 1;
		else {
			w->speedY++;
			//w->speedY *= 2;
			w->y += moveY;
		}
	}
}

// Opposite of initialize().
void terminate() {
	for (size_t i = 0; i < worldItems_len; i++)
		free(worldItems[i]);
	free(worldItems);
	lrFailCleanup(NULL, &lvl);
}

static uint32_t prgm;
static uint32_t vtx_shdr;
static uint32_t frag_shdr;

static void printShaderLog(uint32_t shdr) {
	int log_len;
	glGetShaderiv(shdr, GL_INFO_LOG_LENGTH, &log_len);
	if (log_len > 0) {
		char *log = nnmalloc(log_len);
		glGetShaderInfoLog(shdr, log_len, NULL, log);
		fprintf(stderr, "SHDR_LOG: %s\n", log);
		free(log);
		//assert(NULL);
	}
}

static void initialize_prgm() {
	prgm = glCreateProgram();
	assert(prgm);
	vtx_shdr = glCreateShader(GL_VERTEX_SHADER);
	frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
	assert(vtx_shdr && frag_shdr);
	ssize_t src_len;
	char *const vtx_src = safe_read("shaders/vtx3.txt", &src_len);
	glShaderSource(vtx_shdr, 1, (const char *const *)&vtx_src, (int *)&src_len);
	free(vtx_src);
	char *const frag_src = safe_read("shaders/frag3.txt", &src_len);
	glShaderSource(frag_shdr, 1, (const char *const *)&frag_src,
		(int *)&src_len);
	free(frag_src);
	glAttachShader(prgm, vtx_shdr);
	glAttachShader(prgm, frag_shdr);
	
	glCompileShader(vtx_shdr);
	glCompileShader(frag_shdr);
	printShaderLog(vtx_shdr);
	printShaderLog(frag_shdr);
	
	glLinkProgram(prgm);
	glUseProgram(prgm);
	if (glGetError() != GL_NO_ERROR) {
		int32_t log_len;
		glGetProgramiv(prgm, GL_INFO_LOG_LENGTH, &log_len);
		if (log_len) {
			char *log = nnmalloc(log_len);
			glGetProgramInfoLog(prgm, log_len, NULL, log);
			fprintf(stderr, "PRGM_LOG: %s\n", log);
			free(log);
			assert(NULL);
		}
	}
}

// Callback for doing nothing.
void fnret(WorldItem *self) {
	assert(self);
	return;
}

void pushto_worldItems(const WorldItem *const v);
const WorldItem *worldItem_new_block(enum stl_obj_type type, int x, int y);

// Callback for player frame.
void fnpl(WorldItem *self) {
	size_t collisions_len;
	WorldItem **colls = isCollidingWith(self, &collisions_len, GDIRECTION_BOTH);
	for (size_t i = 0; i < collisions_len; i++) {
		int x = (colls[i]->x + gScrollOffset) / TILE_WIDTH;
		int y = colls[i]->y / TILE_HEIGHT;
		switch (colls[i]->type) {
			case SNOWBALL:
			case MRICEBLOCK:
			case MRBOMB:
			case STALACTITE:
			case BOUNCINGSNOWBALL:
			case FLYINGSNOWBALL:
				if (leftOf(self) - 1 == rightOf(colls[i]) ||
					rightOf(self) + 1 == leftOf(colls[i])) {
					fprintf(stderr, "You died.\n");
					//exit(0);  // kill self todo
				} else if (bottomOf(self) + 1 == topOf(colls[i]))
					colls[i]->type = STL_DEAD;
				break;
			case STL_BONUS:
				if (colls[i]->state == 1 &&  // bonus is active
					topOf(self) - 1 == bottomOf(colls[i]) && y > 0) {
					colls[i]->state = 0;  // deactivate (b/c one use only)
					lvl.interactivetm[y - 1][x] = 44;
					pushto_worldItems(worldItem_new_block(
						STL_COIN,
						colls[i]->x,
						colls[i]->y - TILE_HEIGHT
					));  // hairy!
				}
				break;
			case STL_BRICK:
				if (topOf(self) - 1 != bottomOf(colls[i]))
					break;  // else fall thru, assign dead, rm from lvl.i..TM
			case STL_COIN:
				colls[i]->type = STL_DEAD;
				assert(lvl.interactivetm[y][x] == 44 || lvl.interactivetm[y][x] == 77);
				lvl.interactivetm[y][x] = 0;
				break;
			case STL_WIN:
				fprintf(stderr, "You win!\n");
				break;  // next level todo
		}
	}
}

// Turn around a WorldItem (that has two texnams). Relatively cheap fn.
void turnAround(WorldItem *const self) {
	self->speedX *= -1;  // toggle horizontal direction
	uint32_t tmp = self->texnam;
	self->texnam = self->texnam2;
	self->texnam2 = tmp;
}

// for patrol or smthn
void maybeTurnAround(WorldItem *const self) {
	if (self->speedX == 0)
		return;
	int origX = self->x;
	// calculate hypothetical move
	if (self->speedX < 0)  // going left
		self->x -= self->width;
	else  // going right
		self->x += self->width;

	size_t collisions_len;
	WorldItem **colls = isCollidingWith(self, &collisions_len,
		GDIRECTION_VERT);
	bool onSolidSurface = false;
	for (size_t i = 0; i < collisions_len; i++) {
		if (bottomOf(self) + 1 == topOf(colls[i])) {
			onSolidSurface = true;
			break;
		}
	}
	free(colls);
	if (!onSolidSurface)  // if gonna fall a hypothetical move, turn around
		turnAround(self);
	self->x = origX;
}

// Callback for bot frame.
void fnbot(WorldItem *const self) {
	if (self->patrol)
		maybeTurnAround(self);  // patrol

	int moveX = canMoveTo(self, GDIRECTION_HORIZ);
	if (moveX == 0) {
		turnAround(self);
	}
	self->x += moveX;
}

enum WorldItemState { ALIVE, DEAD };

void fnsnowball(WorldItem *const self) {
	fnbot(self);
}

void fniceblock(WorldItem *const self) {
	fnbot(self);
}

// Push a WorldItem onto worldItems. (auto-inits and grows the array)
void pushto_worldItems(const WorldItem *const v) {
	WorldItem *const w = (WorldItem *const)v;
	if (worldItems_cap == 0) {  // uninitialized
		worldItems_cap = 1;
		worldItems = nnmalloc(worldItems_cap * sizeof(WorldItem *));
	}
	if (worldItems_len == worldItems_cap) {  // need to double capacity
		assert(worldItems_cap <= SIZE_MAX / 2);
		worldItems_cap *= 2;
		worldItems = nnrealloc(worldItems, worldItems_cap * sizeof(WorldItem *));
	}
	worldItems[worldItems_len++] = w;
}

static uint32_t gTextureNames[256];

void must(unsigned long long condition) {
	if (!condition)
		exit(1);
}

// Mirror a 64 * 64 * 3 array of {char r, g, b}.
void mirrorTexelImg(void *imgmem) {
	struct texel {
		char r, g, b;
	};
	struct texel *img = (struct texel *)imgmem;
	for (int height = 0; height < 64; height++)
		for (int width = 0; width < 32; width++) {
			struct texel tmp = img[height * 64 + width];
			img[height * 64 + width] = img[height * 64 + 64 - width - 1];
			img[height * 64 + 64 - width - 1] = tmp;
		}
}

// Upload the file specified by imgnam to the texture specified by texnam to
// make texnam usable by the GL.
void initGLTextureNam(const uint32_t texnam, const char *const imgnam,
	bool mirror) {
	ssize_t has_read;
	char *imgmem = safe_read(imgnam, &has_read);
	assert(has_read == 64 * 64 * 3);
	if (mirror) {  // flip-flop the image
		mirrorTexelImg(imgmem);
	}
	// Do NOT switch the active texture unit!
	// See https://web.archive.org/web/20210905013830/https://users.cs.jmu.edu/b
	//     ernstdh/web/common/lectures/summary_opengl-texture-mapping.php
	//glActiveTexture(GL_TEXTURE0 + w->texunit);  bug
	glBindTexture(GL_TEXTURE_2D, texnam);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGB,
		64,
		64,
		0,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		imgmem
	);
	free(imgmem);
	assert(glGetError() == GL_NO_ERROR);
}

static void maybeInitgTextureNames() {
	static bool ran = false;
	if (ran)
		return;
	ran = true;
	
	glGenTextures(256, gTextureNames);
	assert(glGetError() == GL_NO_ERROR);
	
	// todo: connect the rest of the valid texturename indexes to files
	initGLTextureNam(gTextureNames[10], "textures/snow4.data", false);
	initGLTextureNam(gTextureNames[11], "textures/snow5.data", false);
	initGLTextureNam(gTextureNames[12], "textures/snow6.data", false);
	initGLTextureNam(gTextureNames[13], "textures/snow7.data", false);
	initGLTextureNam(gTextureNames[14], "textures/snow8.data", false);
	initGLTextureNam(gTextureNames[15], "textures/snow9.data", false);
	initGLTextureNam(gTextureNames[16], "textures/snow11.data", false);
	gTextureNames[17] = gTextureNames[16];
	gTextureNames[18] = gTextureNames[16];
	initGLTextureNam(gTextureNames[19], "textures/snow13.data", false);
	initGLTextureNam(gTextureNames[20], "textures/snow14.data", false);
	initGLTextureNam(gTextureNames[21], "textures/snow15.data", false);
	initGLTextureNam(gTextureNames[22], "textures/snow16.data", false);
	initGLTextureNam(gTextureNames[23], "textures/snow17.data", false);
	initGLTextureNam(gTextureNames[25], "textures/background8.data", false);
	initGLTextureNam(gTextureNames[26], "textures/bonus2.data", false);
	initGLTextureNam(gTextureNames[27], "textures/block1.data", false);
	initGLTextureNam(gTextureNames[28], "textures/block2.data", false);
	initGLTextureNam(gTextureNames[29], "textures/block3.data", false);
	initGLTextureNam(gTextureNames[44], "textures/coin1.data", false);
	initGLTextureNam(gTextureNames[48], "textures/block5.data", false);
	initGLTextureNam(gTextureNames[77], "textures/brick0.data", false);
	gTextureNames[83] = gTextureNames[26];
	initGLTextureNam(gTextureNames[85], "textures/cloud-00.data", false);
	initGLTextureNam(gTextureNames[86], "textures/cloud-01.data", false);
	initGLTextureNam(gTextureNames[87], "textures/cloud-02.data", false);
	initGLTextureNam(gTextureNames[88], "textures/cloud-03.data", false);
	initGLTextureNam(gTextureNames[89], "textures/cloud-10.data", false);
	initGLTextureNam(gTextureNames[90], "textures/cloud-11.data", false);
	initGLTextureNam(gTextureNames[91], "textures/cloud-12.data", false);
	initGLTextureNam(gTextureNames[92], "textures/cloud-13.data", false);
	gTextureNames[102] = gTextureNames[26];
	initGLTextureNam(gTextureNames[113], "textures/snow20.data", false);
	initGLTextureNam(gTextureNames[114], "textures/snow21.data", false);
	gTextureNames[128] = gTextureNames[26];
	
	assert(glGetError() == GL_NO_ERROR);
}

void drawGLvertices(const float *const vertices, const uint32_t texnam);

// Draw a tile?
static void paintTile(uint8_t tileID, int x, int y) {
	if (x < -100) {
		fprintf(stderr, "skipping a paintTile\n");
		return;
	}
	if (tileID == 0)
		return;
	maybeInitgTextureNames();
	
	const float vertices[] = {
		x			, y,				1.0,
		x			, y+TILE_HEIGHT,	1.0,
		x+TILE_WIDTH, y,				1.0,
		x+TILE_WIDTH, y+TILE_HEIGHT,	1.0,
	};
	
	return drawGLvertices(vertices, gTextureNames[tileID]);
}

// Handy convenience function to make a new block. x and y are screen coords.
const WorldItem *worldItem_new_block(enum stl_obj_type type, int x, int y) {
	WorldItem *const w = worldItem_new(type, x, y, TILE_WIDTH, TILE_HEIGHT,
		0, 0, false, NULL, fnret, false, false);
	if (type == STL_BONUS)
		w->state = 1;
	return w;
}

// Draw some nice (non-interactive) scenery.
void paintTM(uint8_t **tm) {
	const size_t nTilesScrolledOver = gScrollOffset / TILE_WIDTH;
	const bool playerIsBetweenTiles = gScrollOffset % TILE_WIDTH != 0 &&
		gScrollOffset + SCREEN_WIDTH < lvl.width * TILE_WIDTH ? 1 : 0;
	for (int h = 0; h < SCREEN_HEIGHT / TILE_HEIGHT; h++)
		for (size_t w = nTilesScrolledOver;
			w < nTilesScrolledOver + SCREEN_WIDTH / TILE_WIDTH + playerIsBetweenTiles;
			w++) {
			int x = w * TILE_WIDTH - gScrollOffset;  // screen coordinates
			int y = h * TILE_HEIGHT;  // ibid
			paintTile(tm[h][w], x, y);
		}
}

static int cmpForUint8_t(const void *p, const void *q) {
	const uint8_t *a = (const uint8_t *)p;
	const uint8_t *b = (const uint8_t *)q;
	int m = *a, n = *b;
	return m - n;
}

// Helper for loadLevel.
static void load_lvl_interactives() {
	// Load in the interactives all at once, one time.
	// (Painting for interactives happens elsewhere, repeatedly.)
	for (int h = 0; h < lvl.height; h++)
		for (int w = 0; w < lvl.width; w++) {
			uint8_t tileID = lvl.interactivetm[h][w];
			int x = w * TILE_WIDTH - gScrollOffset;  // screen coordinates
			int y = h * TILE_HEIGHT;  // ibid
			const uint8_t blocks[] = {  // tileIDs for solid tiles
				10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 25,
				27, 28, 29, 48, 102, 113, 114, 128,
			};
			const uint8_t ignored_tiles[] = {
				7, 8, 9, 25, 85, 86, 87, 88, 89, 90, 91, 92, 126, 133,
			};
			if (bsearch(&tileID, blocks, sizeof(blocks)/sizeof(uint8_t), 
				sizeof(uint8_t), cmpForUint8_t))
				pushto_worldItems(worldItem_new_block(STL_BLOCK, x, y));
			else if (tileID == 26 || tileID == 83)
				pushto_worldItems(worldItem_new_block(STL_BONUS, x, y));
			else if (tileID == 77)
				pushto_worldItems(worldItem_new_block(STL_BRICK, x, y));
			else if (tileID == 132)
				pushto_worldItems(worldItem_new_block(STL_WIN, x, y));
			else if (tileID == 44)
				pushto_worldItems(worldItem_new_block(STL_COIN, x, y));
			else if (tileID > 0 &&
				!bsearch(&tileID, ignored_tiles,
					sizeof(ignored_tiles)/sizeof(uint8_t), sizeof(uint8_t),
					cmpForUint8_t))
				fprintf(stderr, "DEBUG: unknown tileID %u\n", tileID);
		}
}

// Helper for loadLevel.
static void load_lvl_objects() {
	for (size_t i = 0; i < lvl.objects_len; i++) {
		WorldItem *w = NULL;
		stl_obj *obj = &lvl.objects[i];
		if (obj->type == SNOWBALL) {
			w = worldItem_new(SNOWBALL, obj->x, obj->y,
				TILE_WIDTH, TILE_HEIGHT, -3, 1, true,
				"textures/snowball.data", fnsnowball, true, true);
		} else if (obj->type == MRICEBLOCK) {
			w = worldItem_new(MRICEBLOCK, obj->x, obj->y,
				TILE_WIDTH, TILE_HEIGHT, -3, 1, true,
				"textures/mriceblock.data", fniceblock, true, true);
		} else
			continue;
		pushto_worldItems(w);
	}
}

static bool loadLevel(const char *const level_filename) {
	for (size_t i = 0; i < worldItems_len; i++) {
		glDeleteTextures(1, &worldItems[i]->texnam);
		glDeleteTextures(1, &worldItems[i]->texnam2);
		free(worldItems[i]);
	}
	worldItems_len = 0;
	
	lvl = levelReader(level_filename);
	if (!lvl.hdr)
		return false;
	stlPrinter(&lvl);
	pushto_worldItems(worldItem_new(STL_PLAYER, lvl.start_pos_x,
		lvl.start_pos_y, TILE_WIDTH, TILE_HEIGHT, 8, 1, true,
		"textures/texture3.rgb", fnpl, false, false));
	player = worldItems[0];
	load_lvl_objects();
	load_lvl_interactives();
	
	return true;
}

static void initialize() {
	initialize_prgm();
	
	assert(loadLevel("gpl/levels/level1.stl"));
}

// Return true if w is off-screen.
bool isOffscreen(const WorldItem *const w) {
	int top, bottom, left, right;
	
	top = w->y;
	bottom = w->y + w->height;
	left = w->x;
	right = w->x + w->width;
	
	return (top > SCREEN_HEIGHT || bottom < 0 ||
		left > SCREEN_WIDTH || right < 0);
}

void drawGLvertices(const float *const vertices, const uint32_t texnam) {
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(0);
	glBindAttribLocation(prgm, 0, "vertices");
	
	static const float tcoords[] = {
		0.001, 0.001,
		0.001, 0.999,
		0.999, 0.001,
		0.999, 0.999,
	};
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tcoords);
	glEnableVertexAttribArray(1);
	glBindAttribLocation(prgm, 1, "tcoords");
	assert(glGetError() == GL_NO_ERROR);

	glBindTexture(GL_TEXTURE_2D, texnam);  // ???

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	must(glGetError() == GL_NO_ERROR);
}

static void drawWorldItems() {
	for (size_t i = 0; i < worldItems_len; i++) {
		const WorldItem *const w = worldItems[i];
		assert(w);
		if (isOffscreen(w) || w->texnam == 0)
			continue;
		
		const float vertices[] = {
			0+w->x, 0+w->y, 1.0,
			0+w->x, w->y+w->height, 1.0,
			w->x+w->width, 0+w->y, 1.0,
			w->x+w->width, w->y+w->height, 1.0,
		};
		drawGLvertices(vertices, w->texnam);
	}
}

static void clearScreen() {
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
}

static void core(keys *const k) {
	static bool initialized = false;
	if (!initialized) {
		initialize();
		initialized = true;
	}
	
	processInput(k);
	applyFrame();
	applyGravity();
	
	clearScreen();
	paintTM(lvl.backgroundtm);
	paintTM(lvl.interactivetm);
	paintTM(lvl.foregroundtm);
	drawWorldItems();
	
	if (player->type == STL_PLAYER_DEAD) {
		assert(loadLevel("levels/gpl/level1.stl"));
	} else if (player->type == STL_PLAYER_ASCENDED) {
		assert(loadLevel("levels/gpl/level2.stl"));
	}
}

// Entry point for initgl.
bool draw(keys *const k) {
	core(k);
	return true;
}


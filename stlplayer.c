// stlplayer.c

#include "initgl.h"
#include "stlplayer.h"

static const int SCREEN_WIDTH = 640, SCREEN_HEIGHT = 480;
static const int TILE_WIDTH = 32, TILE_HEIGHT = 32;
static int gScrollOffset = 0, gCurrLevel = 1;

typedef int Direction;
enum { LEFT, RIGHT, UP, DOWN,
	GDIRECTION_HORIZ, GDIRECTION_VERT, GDIRECTION_BOTH, };
typedef int GeneralDirection;

static WorldItem **worldItems;  // array of pointers to WorldItem
static size_t worldItems_len, worldItems_cap;

static stl lvl;

static Player *player;

static const float MRICEBLOCK_KICKSPEED = 10;
static const float PLAYER_BOUNCE_SPEED = -10;
static const float PLAYER_JUMP_SPEED = -10;
static const float PLAYER_RUN_SPEED = 7;
static const float BOUNCINGSNOWBALL_JUMP_SPEED = -9;
static const float FLYINGSNOWBALL_HOVER_SPEED = -2;
static float BADGUY_X_SPEED = -2;
static float JUMPY_JUMP_SPEED = -10;

static const uint8_t ignored_tiles[] = {
	6, 126, 133,
};  // todo burndown

enum gOTNi {
	STL_DEAD_MRICEBLOCK_TEXTURE_LEFT = 0,
	STL_DEAD_MRICEBLOCK_TEXTURE_RIGHT,
	STL_SNOWBALL_TEXTURE_LEFT,
	STL_SNOWBALL_TEXTURE_RIGHT,
	STL_BOUNCINGSNOWBALL_TEXTURE_LEFT,
	STL_BOUNCINGSNOWBALL_TEXTURE_RIGHT,
	STL_BOMB_TEXTURE_LEFT,
	STL_BOMB_TEXTURE_RIGHT,
	STL_BOMBX_TEXTURE_LEFT,
	STL_BOMBX_TEXTURE_RIGHT,
	STL_BOMB_EXPLODING_TEXTURE_1,
	STL_BOMB_EXPLODING_TEXTURE_2,
	STL_SPIKY_TEXTURE_LEFT,
	STL_SPIKY_TEXTURE_RIGHT,
	STL_FLYINGSNOWBALL_TEXTURE_LEFT,
	STL_FLYINGSNOWBALL_TEXTURE_RIGHT,
	STL_STALACTITE_TEXTURE,
	STL_JUMPY_TEXTURE,
	gOTNlen = 64,
};
static uint32_t gObjTextureNames[gOTNlen];  // shared across all levels

static void initGLTextureNam(const uint32_t texnam, const char *const imgnam,
	bool mirror, bool hasAlpha);

static WorldItem *worldItem_new(enum stl_obj_type type, int x, int y, int wi,
	int h, float spx, float spy, bool gravity, char *imgnam,
	void(*frame)(WorldItem *const), bool mirrable, bool patrol, bool hasAlpha) {
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
		initGLTextureNam(w->texnam, imgnam, false, hasAlpha);
		if (mirrable) {
			glGenTextures(1, &w->texnam2);
			initGLTextureNam(w->texnam2, imgnam, true, hasAlpha);
		}
	}
	
	return w;
}

static int leftOf(const WorldItem *const w) {
	return w->x;
}

static int rightOf(const WorldItem *const w) {
	assert(w->x < INT_MAX - w->width);
	return w->x + w->width;
}

static int topOf(const WorldItem *const w) {
	return w->y;
}

static int bottomOf(const WorldItem *const w) {
	assert(w->y < INT_MAX - w->height);
	return w->y + w->height;
}

// Return true if c is between a and b.
static bool isBetween(const int c, const int a, const int b) {
	assert(a <= b);
	return (a <= c && c <= b);
}

static bool isCollidingHorizontally(const WorldItem *const w,
	const WorldItem *const v) {
	return v != w && (
		isBetween(topOf(v), topOf(w), bottomOf(w)) ||
		isBetween(bottomOf(v), topOf(w), bottomOf(w)) ||
		(topOf(w) >= topOf(v) && bottomOf(w) <= bottomOf(v)) ||
		(topOf(w) <= topOf(v) && bottomOf(w) >= bottomOf(v))
	);
}

static bool isCollidingVertically(const WorldItem *const w,
	const WorldItem *const v) {
	return v != w && (
		isBetween(leftOf(v), leftOf(w), rightOf(w)) ||
		isBetween(rightOf(v), leftOf(w), rightOf(w)) ||
		(leftOf(w) >= leftOf(v) && rightOf(w) <= rightOf(v)) ||
		(leftOf(w) <= leftOf(v) && rightOf(w) >= rightOf(v))
	);
}

// iCW helper. Expand w in either the horizontal or vertical direction.
static void iCW_expand_w(WorldItem *const w, GeneralDirection dir) {
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
static void iCW_shrink_w(WorldItem *const w, GeneralDirection dir) {
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
static WorldItem **isCollidingWith(WorldItem *const w,
	size_t *const collisions_len, GeneralDirection gdir) {
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
static int canMoveX(WorldItem *const w) {
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
				if (colls[i]->type == STL_COIN || colls[i]->type == STL_DEAD)
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
static int canMoveY(WorldItem *const w) {
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

static void deletefrom_worldItems(int index) {
	assert(index >= 0 && (size_t)index < worldItems_len);
	free(worldItems[index]);
	memmove(&(worldItems[index]), &(worldItems[index+1]),
		(worldItems_len - index - 1) * sizeof(WorldItem *));
	worldItems_len--;
}

// Scroll everything diff pixels left.
static void scrollTheScreen(int diff) {
	for (size_t i = 0; i < worldItems_len; i++) {
		WorldItem *const w = worldItems[i];
		w->x -= diff;
	}
	gScrollOffset += diff;
}

// Delete all WorldItems far past the left edge of the screen.
static void cleanupWorldItems() {
	for (size_t i = 0; i < worldItems_len; i++) {
		const WorldItem *const w = worldItems[i];
		if (w->x < -100) {
			assert(i != 0);  // player should never get past x = 0
			deletefrom_worldItems(i--);
		}
	}
}

static void maybeScrollScreen() {
	if (player->x <= SCREEN_WIDTH / 3 ||
		gScrollOffset + SCREEN_WIDTH >= lvl.width * TILE_WIDTH)
		return;
	
	scrollTheScreen(player->x - SCREEN_WIDTH / 3);
	cleanupWorldItems();
}

static void wiSwapTextures(WorldItem *const w) {
	uint32_t tmp = w->texnam;
	w->texnam = w->texnam2;
	w->texnam2 = tmp;
}

static bool gPlayerLastDirectionWasRight = true;

static bool playerLandedOnSurface(WorldItem **const colls,
	const size_t colls_len) {
	for (size_t i = 0; i < colls_len; i++) {
		const WorldItem *const coll = colls[i];
		if (bottomOf(player) + 1 != topOf(coll))
			continue;
		switch (coll->type) {
			case STL_BRICK:
			case STL_BLOCK:
			case STALACTITE:
			case STL_BONUS:  // bonus are the [?] squares
			case STL_INVISIBLE:
				return true;
		}
	}
	return false;
}

static bool playerLandedOnBadguy(WorldItem **const colls,
	const size_t colls_len) {
	for (size_t i = 0; i < colls_len; i++) {
		const WorldItem *const coll = colls[i];
		if (bottomOf(player) + 1 != topOf(coll))
			continue;
		switch (coll->type) {
			case SNOWBALL:
			case MRICEBLOCK:
			case STL_BOMB:
			case STL_BOMB_TICKING:
			case STL_DEAD_MRICEBLOCK:
			case STL_KICKED_MRICEBLOCK:
			case BOUNCINGSNOWBALL:
			case FLYINGSNOWBALL:
				return true;
		}
	}
	return false;
}

static void processInput(const keys *const k) {
	assert(k && SCREEN_WIDTH && SCREEN_HEIGHT);
	
	if (k->keyD || k->keyRight) {
		if (player->speedX <= 0)
			player->speedX = 0.33;
		else
			player->speedX *= 2;
		if (player->speedX > fabs(PLAYER_RUN_SPEED))
			player->speedX = fabs(PLAYER_RUN_SPEED);
		//player->speedX = fabs(player->speedX);
		
		player->x += canMoveTo(player, GDIRECTION_HORIZ);
		if (!gPlayerLastDirectionWasRight)
			wiSwapTextures(player);
		gPlayerLastDirectionWasRight = true;
	}
	if (k->keyA || k->keyLeft) {
		if (player->speedX >= 0)
			player->speedX = -0.33;
		else
			player->speedX *= 2;
		assert(PLAYER_RUN_SPEED != INT_MIN);
		if (player->speedX < -fabs(PLAYER_RUN_SPEED))
			player->speedX = -fabs(PLAYER_RUN_SPEED);
		
		player->x += canMoveTo(player, GDIRECTION_HORIZ);
		if (gPlayerLastDirectionWasRight)
			wiSwapTextures(player);
		gPlayerLastDirectionWasRight = false;
	}
	if (!k->keyD && !k->keyRight && !k->keyA && !k->keyLeft) {  // slow down
		player->speedX *= 0.66;
		if (fabs(player->speedX) < 0.1)
			player->speedX = 0;
		player->x += canMoveTo(player, GDIRECTION_HORIZ);
	}
	
	static bool jumped = true;
	size_t colls_len;
	WorldItem **const colls = isCollidingWith(player, &colls_len,
		GDIRECTION_BOTH);
	bool onBadguy = playerLandedOnBadguy(colls, colls_len);
	bool onSurface = playerLandedOnSurface(colls, colls_len);
	// reset on all keys up
	if (!k->keyW && !k->keyUp && !k->keySpace) {
		if (player->speedY < 0)
			player->speedY /= 2;
		if (onSurface)
			jumped = false;
	}
	if (onBadguy)
		jumped = false;
	if (k->keyW || k->keyUp || k->keySpace) {
		if (!jumped && (onBadguy || onSurface)) {
			player->speedY = PLAYER_JUMP_SPEED;
			//player->y += canMoveTo(player, GDIRECTION_VERT);
			jumped = true;
		}
	}
	free(colls);
	
	// 0 means not carrying, 1 means could carry, 2 means carrying something
	if (k->keyCTRL && player->state == 0) {
		player->state = 1;
	} else if (!k->keyCTRL)
		player->state = 0;
	
	// debugging
	if (k->keyE) {
		player_toggle_size();
		if (player->speedX == PLAYER_RUN_SPEED) 
			player->speedX *= 2;
		else
			player->speedX = PLAYER_RUN_SPEED;
	}
	
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
		else if (worldItems[i]->type == STL_BRICK_DESTROYED) {
			int x = (worldItems[i]->x + gScrollOffset) / TILE_WIDTH;
			int y = worldItems[i]->y / TILE_HEIGHT;
			lvl.interactivetm[y][x] = 0;
			deletefrom_worldItems(i--);
		}
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
			w->speedY += 0.333;
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

// Initialize the GL program.
static void initialize_prgm() {
	prgm = glCreateProgram();
	assert(prgm);
	vtx_shdr = glCreateShader(GL_VERTEX_SHADER);
	frag_shdr = glCreateShader(GL_FRAGMENT_SHADER);
	assert(vtx_shdr && frag_shdr);
	ssize_t src_len;
	char *const vtx_src = safe_read("shaders/vtx.txt", &src_len);
	glShaderSource(vtx_shdr, 1, (const char *const *)&vtx_src, (int *)&src_len);
	free(vtx_src);
	char *const frag_src = safe_read("shaders/frag.txt", &src_len);
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
static void fnret(WorldItem *self) {
	assert(self);
	return;
}

static void pushto_worldItems(const WorldItem *const v);
static const WorldItem *worldItem_new_block(enum stl_obj_type type, int x, int y);

static bool hitScreenBottom(const WorldItem *const self) {
	return self->y + self->height + 1 >= SCREEN_HEIGHT;
}

// The player kicks the icecube.
static void playerKicks(WorldItem *coll) {
	assert(coll->type == STL_DEAD_MRICEBLOCK);
	coll->type = STL_KICKED_MRICEBLOCK;
	coll->gravity = true;
	
	if (player->speedY >= 0 && bottomOf(player) + 1 == topOf(coll))
		player->speedY = PLAYER_BOUNCE_SPEED;  // bounce off top of icecube
	
	// player is left of the iceblock
	if (player->x + player->width / 2 < coll->x + coll->width / 2) {
		coll->x = player->x + player->width;
		if (coll->speedX < 0)
			wiSwapTextures(coll);
		coll->speedX = MRICEBLOCK_KICKSPEED;  // iceblock goes right
	} else {  // player is right of the iceblock
		coll->x = player->x - coll->width;  // not a typo
		if (coll->speedX > 0)
			wiSwapTextures(coll);
		coll->speedX = -MRICEBLOCK_KICKSPEED;  // iceblock goes left
	}
}

// Callback for player frame.
static void fnpl(WorldItem *self) {
	if (hitScreenBottom(self)) {
		fprintf(stderr, "You died.\n");
		self->type = STL_PLAYER_DEAD;
	}
	size_t collisions_len;
	WorldItem **colls = isCollidingWith(self, &collisions_len, GDIRECTION_BOTH);
	for (size_t i = 0; i < collisions_len; i++) {
		int x = (colls[i]->x + gScrollOffset) / TILE_WIDTH;
		int y = colls[i]->y / TILE_HEIGHT;
		switch (colls[i]->type) {
			case SPIKY:
				fprintf(stderr, "You died.\n");
				self->type = STL_PLAYER_DEAD;
				break;
			case MRICEBLOCK:
				if (self->speedY >= 0)
					self->speedY = PLAYER_BOUNCE_SPEED;
				if (bottomOf(self) + 1 == topOf(colls[i])) {
					// todo have a fixed set of texnams to index into
					colls[i]->type = STL_DEAD_MRICEBLOCK;
					//glDeleteTextures(1, &colls[i]->texnam);  // todo: turn
					//glDeleteTextures(1, &colls[i]->texnam2);  // back on someday
					colls[i]->texnam =
						gObjTextureNames[STL_DEAD_MRICEBLOCK_TEXTURE_LEFT];
					colls[i]->texnam2 =
						gObjTextureNames[STL_DEAD_MRICEBLOCK_TEXTURE_RIGHT];
					if (colls[i]->speedX > 0) {  // going right
						wiSwapTextures(colls[i]);
					}
				} else if (true || leftOf(self) - 1 == rightOf(colls[i]) ||
					rightOf(self) + 1 == leftOf(colls[i])) {  // xxx
					fprintf(stderr, "You died.\n");
					self->type = STL_PLAYER_DEAD;
				}
				break;
			case STL_DEAD_MRICEBLOCK:  // just sitting there
				if (self->state == 0) {
					colls[i]->gravity = true;
					playerKicks(colls[i]);
				} else if (self->state == 1) {
					colls[i]->gravity = false;
					if (self->speedX > 0)  // player is going right
						colls[i]->x = self->x + self->width / 2;
					else if (self->speedX < 0)  // player is going left
						colls[i]->x = self->x - self->width / 2;
					colls[i]->y = self->y - 5;  // arbitrary
					//self->state = 2;
				}
				break;
			case STL_KICKED_MRICEBLOCK:
				if (self->speedY >= 0)
					self->speedY = PLAYER_BOUNCE_SPEED;
				if (bottomOf(self) + 1 == topOf(colls[i]))
					colls[i]->type = STL_DEAD_MRICEBLOCK;
				else if (leftOf(self) - 1 == rightOf(colls[i]) ||
					rightOf(self) + 1 == leftOf(colls[i])) {
					fprintf(stderr, "You died.\n");
					self->type = STL_PLAYER_DEAD;
				}
				break;
			case STALACTITE:
				if (topOf(self) - 1 == bottomOf(colls[i])) {
					fprintf(stderr, "You died.\n");
					self->type = STL_PLAYER_DEAD;
				}
				break;
			case SNOWBALL:
			case STL_BOMB:
			case STL_BOMB_TICKING:
			case BOUNCINGSNOWBALL:
			case FLYINGSNOWBALL:
				if (topOf(self) - 1 == bottomOf(colls[i]) ||
					leftOf(self) - 1 == rightOf(colls[i]) ||
					rightOf(self) + 1 == leftOf(colls[i])) {
					fprintf(stderr, "You died.\n");
					self->type = STL_PLAYER_DEAD;
				} else if (bottomOf(self) + 1 == topOf(colls[i])) {
					if (self->speedY >= 0)
						self->speedY = PLAYER_BOUNCE_SPEED;  // bounce off corpse
					if (colls[i]->type == STL_BOMB) {
						colls[i]->type = STL_BOMB_TICKING;
						assert(colls[i]->speedX != INT_MIN);
						colls[i]->speedX = -fabs(colls[i]->speedX);
						colls[i]->texnam = gObjTextureNames[STL_BOMBX_TEXTURE_LEFT];
						colls[i]->texnam2 = gObjTextureNames[STL_BOMBX_TEXTURE_RIGHT];
						colls[i]->patrol = false;
					} else if (colls[i]->type != STL_BOMB_TICKING)
						colls[i]->type = STL_DEAD;
				}
				break;
			case STL_BOMB_EXPLODING:
				fprintf(stderr, "You died.\n");
				self->type = STL_PLAYER_DEAD;
				break;
			case STL_BONUS:
				if (colls[i]->state == 1 &&  // bonus is active
					topOf(self) - 1 == bottomOf(colls[i])) {
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
				if (topOf(self) - 1 != bottomOf(colls[i]) || (
					!isBetween(rightOf(self), leftOf(colls[i]),
					rightOf(colls[i])) && !isBetween(leftOf(self),
					leftOf(colls[i]), rightOf(colls[i]))))
					break;
			case STL_COIN:
				colls[i]->type = STL_DEAD;
				//assert(lvl.interactivetm[y][x] == 44 || false);
				lvl.interactivetm[y][x] = 0;
				break;
			case MONEY:  // jumpy
			case JUMPY:
				fprintf(stderr, "You died.\n");
				self->type = STL_PLAYER_DEAD;
				break;
			case STL_WIN:
				fprintf(stderr, "You win!\n");
				self->type = STL_PLAYER_ASCENDED;
				break;  // next level todo
			case STL_INVISIBLE:
				if (colls[i]->state == 1 &&
					topOf(self) - 1 == bottomOf(colls[i])) {
					colls[i]->state = 0;
					wiSwapTextures(colls[i]);
				}
				break;
			//default:
				//fprintf(stderr, "WARN: fnpl() unhandled case\n");
				//break;
		}
	}
	free(colls);
}

// Turn around a WorldItem (that has two texnams). Relatively cheap fn.
static void turnAround(WorldItem *const self) {
	self->speedX *= -1;  // toggle horizontal direction
	uint32_t tmp = self->texnam;
	self->texnam = self->texnam2;
	self->texnam2 = tmp;
}

// for patrol or smthn
static void maybeTurnAround(WorldItem *const self) {
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
		if (colls[i]->type == STL_COIN)
			continue;  // coins are not a solid surface
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

// Callback for bot frame. Move the bot around horizontally.
static void fnbot(WorldItem *const self) {
	if (self->patrol)
		maybeTurnAround(self);  // patrol

	int moveX = canMoveTo(self, GDIRECTION_HORIZ);
	if (moveX == 0) {
		turnAround(self);
	}
	self->x += moveX;
}

static void fnsnowball(WorldItem *const self) {
	if (hitScreenBottom(self)) {
		self->type = STL_DEAD;
		return;
	}
	fnbot(self);
}

static void fnbouncingsnowball(WorldItem *const self) {
	if (hitScreenBottom(self)) {
		self->type = STL_DEAD;
		return;
	}
	fnbot(self);
	size_t collisions_len;
	WorldItem **colls = isCollidingWith(self, &collisions_len, GDIRECTION_BOTH);
	for (size_t i = 0; i < collisions_len; i++) {
		const WorldItem *const coll = colls[i];
		if (coll->type == STL_COIN)
			continue;  // coins are not solid blocks to bounce off
		if (bottomOf(self) + 1 == topOf(coll)) {  // bounce off surface
			self->speedY = BOUNCINGSNOWBALL_JUMP_SPEED;
			break;
		}
	}
	free(colls);
}

static void iceDestroyObstacles(WorldItem *const self) {
	assert(self->type == STL_KICKED_MRICEBLOCK);
	size_t collisions_len;
	WorldItem **colls = isCollidingWith(self, &collisions_len,
		GDIRECTION_BOTH);
	for (size_t i = 0; i < collisions_len; i++) {
		switch (colls[i]->type) {
			case STL_BRICK:
				if (bottomOf(self) + 1 != topOf(colls[i]))
					colls[i]->type = STL_BRICK_DESTROYED;  // break the brick
				break;
			case STL_DEAD_MRICEBLOCK:
			case SNOWBALL:
			case MRICEBLOCK:
			case STL_BOMB:
			case STALACTITE:
			case BOUNCINGSNOWBALL:
			case FLYINGSNOWBALL:
			case MONEY:
			case JUMPY:
				colls[i]->type = STL_DEAD;  // kill that one
				break;
			case STL_KICKED_MRICEBLOCK:  // kill both
				self->type = STL_DEAD;  // die self for real
				colls[i]->type = STL_DEAD;  // kill the colls[i]
				break;
		}
	}
	free(colls);
}

static void fniceblock(WorldItem *const self) {
	if (hitScreenBottom(self)) {
		self->type = STL_DEAD;
		return;
	}
	if (self->type == STL_DEAD_MRICEBLOCK) {
		// don't move around
		return;
	} else if (self->type == STL_KICKED_MRICEBLOCK) {
		self->patrol = false;
		if (self->speedX < 0)
			self->speedX = -MRICEBLOCK_KICKSPEED;
		else
			self->speedX = MRICEBLOCK_KICKSPEED;
		
		int origSpeedX = self->speedX;
		do {
			iceDestroyObstacles(self);
			if (self->type == STL_DEAD)
				break;  // the dead don't move no more
			int moveX = canMoveTo(self, GDIRECTION_HORIZ);
			if (moveX == 0) {
				turnAround(self);  // in shame
				return;  // lost cause
			}
			self->x += moveX;
			self->speedX -= moveX;
		} while (0 != self->speedX);
		self->speedX = origSpeedX;  // do it all over again at the next frame
	} else
		fnbot(self);
}

// Set self type to exploding, and reset the framesElapsed counter.
static void bombExplodes(WorldItem *const self, int *const framesElapsed) {
	self->type = STL_BOMB_EXPLODING;
	self->gravity = false;
	self->x -= self->width;
	self->y -= self->height;
	self->width *= 3;
	self->height *= 3;
	self->texnam = gObjTextureNames[STL_BOMB_EXPLODING_TEXTURE_1];  // darker
	self->texnam2 = gObjTextureNames[STL_BOMB_EXPLODING_TEXTURE_2];  // brighter
	*framesElapsed = 0;  // reset the frame counter
}

static void bombHandleExplosionCollisions(WorldItem *const self) {
	size_t collisions_len;
	WorldItem **const colls = isCollidingWith(self, &collisions_len,
		GDIRECTION_BOTH);
	for (size_t i = 0; i < collisions_len; i++) {
		WorldItem *const coll = colls[i];
		switch (coll->type) {
			case STL_PLAYER:
				fprintf(stderr, "You died.\n");  // todo turn into a fn
				coll->type = STL_PLAYER_DEAD;
				break;
			case MRICEBLOCK:
			case STL_DEAD_MRICEBLOCK:
			case STL_KICKED_MRICEBLOCK:
			case SNOWBALL:
			case STALACTITE:
			case BOUNCINGSNOWBALL:
			case FLYINGSNOWBALL:
			case MONEY:
			case JUMPY:
				coll->type = STL_DEAD;
				break;
			case STL_BRICK:
			case STL_COIN:
				coll->type = STL_BRICK_DESTROYED;
				break;
			case STL_BOMB:
				coll->type = STL_BOMB_TICKING;
				coll->speedX = -fabs(coll->speedX);
				coll->texnam = gObjTextureNames[STL_BOMBX_TEXTURE_LEFT];
				coll->texnam2 = gObjTextureNames[STL_BOMBX_TEXTURE_RIGHT];
				coll->patrol = false;
				break;
			case STL_BONUS:
				coll->state = 0;  // destroy the bonus block
		}
	}
	free(colls);
}

static void fnbomb(WorldItem *const self) {
	static int framesElapsed = 0;
	
	if (hitScreenBottom(self)) {
		self->type = STL_BOMB_EXPLODING;
	}
	
	if (self->type == STL_BOMB_TICKING && framesElapsed >= 120) {
		bombExplodes(self, &framesElapsed);
	}
	
	if (self->type == STL_BOMB_TICKING) {
		// chase the player
		if ((player->x < self->x && self->speedX > 0) ||
			(player->x > self->x && self->speedX < 0)) {
			assert(self->speedX != INT_MIN);
			self->speedX *= -1;
			wiSwapTextures(self);
		}
		fnbot(self);
		framesElapsed++;
	} else if (self->type == STL_BOMB_EXPLODING) {
		if (framesElapsed >= 60)
			self->type = STL_DEAD;
		else {
			if (framesElapsed % 5 == 0)
				wiSwapTextures(self);
			bombHandleExplosionCollisions(self);
			framesElapsed++;
		}
	}
	else
		fnbot(self);
}

static void fnspiky(WorldItem *const self) {
	fnbot(self);
}

static void fnflyingsnowball(WorldItem *const self) {
	static int totalDistMoved = 0;
	
	int moveY = canMoveTo(self, GDIRECTION_VERT);
	if (moveY == 0 || totalDistMoved >= 4 * TILE_HEIGHT) {
		assert(self->speedY != INT_MIN);
		self->speedY *= -1;  // for next time
		totalDistMoved = 0;
	} else {
		self->y += moveY;
		totalDistMoved += abs(moveY);
	}
}

static void fnstalactite(WorldItem *const self) {
	int playerCenterX = player->x + player->width / 2;
	int selfCenterX = self->x + self->width / 2;
	if (abs(selfCenterX - playerCenterX) > 4 * TILE_WIDTH)
		return;
	
	static int framesWaited = 0;
	assert(framesWaited >= 0 && framesWaited <= 30);
	if (framesWaited == 30)
		self->gravity = true;
	else
		framesWaited++;
}

static void fnjumpy(WorldItem *const self) {
	size_t colls_len;
	WorldItem **colls = isCollidingWith(self, &colls_len, GDIRECTION_VERT);
	for (size_t i = 0; i < colls_len; i++) {
		const WorldItem *const coll = colls[i];
		if (coll->type == STL_COIN)
			continue;
		else if (bottomOf(self) + 1 == topOf(coll)) {
			self->speedY = JUMPY_JUMP_SPEED;
			break;
		}
	}
	free(colls);
}

// Push a WorldItem onto worldItems. (auto-inits and grows the array)
static void pushto_worldItems(const WorldItem *const v) {
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

static uint32_t gTextureNames[257];  // 0-256, plus 1 for the background image

// Mirror a 64 * 64 * 4 array of {char r, g, b, a}.
static void mirrorTexelImgAlpha(void *imgmem) {
	struct texel {
		char r, g, b, a;
	};
	struct texel *img = (struct texel *)imgmem;
	for (int height = 0; height < 64; height++)
		for (int width = 0; width < 32; width++) {
			struct texel tmp = img[height * 64 + width];
			img[height * 64 + width] = img[height * 64 + 64 - width - 1];
			img[height * 64 + 64 - width - 1] = tmp;
		}
}

// Mirror a 64 * 64 * 3 array of {char r, g, b}.
static void mirrorTexelImg(void *imgmem, bool hasAlpha) {
	if (hasAlpha)
		return mirrorTexelImgAlpha(imgmem);  // copy-paste of this fn, but w/ alpha support 
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
static void initGLTextureNam(const uint32_t texnam, const char *const imgnam,
	bool mirror, bool hasAlpha) {
	ssize_t has_read;
	char *imgmem = safe_read(imgnam, &has_read);
	assert((!hasAlpha && has_read == 64 * 64 * 3) ||
		(hasAlpha && has_read == 64 * 64 * 4));
	if (mirror) {  // flip-flop the image
		mirrorTexelImg(imgmem, hasAlpha);
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
	int imgformat = GL_RGB;
	if (hasAlpha)
		imgformat = GL_RGBA;
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		imgformat,
		64,
		64,
		0,
		imgformat,
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
	
	glGenTextures(257, gTextureNames);
	assert(glGetError() == GL_NO_ERROR);
	
	// todo: connect the rest of the valid texturename indexes to files
	initGLTextureNam(gTextureNames[7], "textures/snow1.data", false, true);
	initGLTextureNam(gTextureNames[8], "textures/snow2.data", false, true);
	initGLTextureNam(gTextureNames[9], "textures/snow3.data", false, true);
	initGLTextureNam(gTextureNames[10], "textures/snow4.data", false, false);
	initGLTextureNam(gTextureNames[11], "textures/snow5.data", false, false);
	initGLTextureNam(gTextureNames[12], "textures/snow6.data", false, false);
	initGLTextureNam(gTextureNames[13], "textures/snow7.data", false, false);
	initGLTextureNam(gTextureNames[14], "textures/snow8.data", false, false);
	initGLTextureNam(gTextureNames[15], "textures/snow9.data", false, false);
	initGLTextureNam(gTextureNames[16], "textures/snow11.data", false, false);
	gTextureNames[17] = gTextureNames[16];
	gTextureNames[18] = gTextureNames[16];
	initGLTextureNam(gTextureNames[19], "textures/snow13.data", false, false);
	initGLTextureNam(gTextureNames[20], "textures/snow14.data", false, false);
	initGLTextureNam(gTextureNames[21], "textures/snow15.data", false, false);
	initGLTextureNam(gTextureNames[22], "textures/snow16.data", false, false);
	initGLTextureNam(gTextureNames[23], "textures/snow17.data", false, false);
	initGLTextureNam(gTextureNames[24], "textures/background7.data", false, true);
	initGLTextureNam(gTextureNames[25], "textures/background8.data", false, true);
	initGLTextureNam(gTextureNames[26], "textures/bonus2.data", false, false);
	initGLTextureNam(gTextureNames[27], "textures/block1.data", false, false);
	initGLTextureNam(gTextureNames[28], "textures/block2.data", false, false);
	initGLTextureNam(gTextureNames[29], "textures/block3.data", false, false);
	initGLTextureNam(gTextureNames[30], "textures/snow18.data", false, false);
	initGLTextureNam(gTextureNames[31], "textures/snow19.data", false, false);
	
	initGLTextureNam(gTextureNames[32], "textures/darksnow1.data", false, true);
	gTextureNames[33] = gTextureNames[32];
	gTextureNames[34] = gTextureNames[32];
	initGLTextureNam(gTextureNames[36], "textures/darksnow5.data", false, true);
	gTextureNames[35] = gTextureNames[36];
	gTextureNames[37] = gTextureNames[36];
	gTextureNames[38] = gTextureNames[36];
	gTextureNames[39] = gTextureNames[36];
	gTextureNames[40] = gTextureNames[36];
	gTextureNames[41] = gTextureNames[36];
	gTextureNames[42] = gTextureNames[36];
	gTextureNames[43] = gTextureNames[36];
	
	initGLTextureNam(gTextureNames[44], "textures/coin1.data", false, true);
	
	gTextureNames[45] = gTextureNames[44];
	gTextureNames[46] = gTextureNames[44];
	
	initGLTextureNam(gTextureNames[47], "textures/block4.data", false, false);
	initGLTextureNam(gTextureNames[48], "textures/block5.data", false, false);
	
	initGLTextureNam(gTextureNames[57], "textures/pipe5.data", false, true);
	initGLTextureNam(gTextureNames[58], "textures/pipe6.data", false, true);
	initGLTextureNam(gTextureNames[59], "textures/pipe7.data", false, true);
	initGLTextureNam(gTextureNames[60], "textures/pipe8.data", false, true);
	initGLTextureNam(gTextureNames[61], "textures/block10.data", false, true);
	
	initGLTextureNam(gTextureNames[76], "textures/waves-1.data", false, true);
	initGLTextureNam(gTextureNames[77], "textures/brick0.data", false, false);
	initGLTextureNam(gTextureNames[78], "textures/brick1.data", false, false);
	gTextureNames[83] = gTextureNames[26];
	initGLTextureNam(gTextureNames[84], "textures/bonus2-d.data", false, false);
	initGLTextureNam(gTextureNames[85], "textures/Acloud-00.data", false, true);
	initGLTextureNam(gTextureNames[86], "textures/Acloud-01.data", false, true);
	initGLTextureNam(gTextureNames[87], "textures/Acloud-02.data", false, true);
	initGLTextureNam(gTextureNames[88], "textures/Acloud-03.data", false, true);
	initGLTextureNam(gTextureNames[89], "textures/Acloud-10.data", false, true);
	initGLTextureNam(gTextureNames[90], "textures/Acloud-11.data", false, true);
	initGLTextureNam(gTextureNames[91], "textures/Acloud-12.data", false, true);
	initGLTextureNam(gTextureNames[92], "textures/Acloud-13.data", false, true);
	gTextureNames[102] = gTextureNames[26];  // drawn as bonus2 but does nothing
	gTextureNames[103] = gTextureNames[26];
	gTextureNames[104] = gTextureNames[77];
	gTextureNames[105] = gTextureNames[78];
	initGLTextureNam(gTextureNames[106], "textures/background1.data", false, true);
	initGLTextureNam(gTextureNames[107], "textures/background2.data", false, true);
	initGLTextureNam(gTextureNames[108], "textures/background3.data", false, true);
	initGLTextureNam(gTextureNames[109], "textures/background4.data", false, true);
	initGLTextureNam(gTextureNames[110], "textures/background5.data", false, true);
	initGLTextureNam(gTextureNames[111], "textures/background6.data", false, true);
	initGLTextureNam(gTextureNames[112], "textures/transparent2.data", false, true);
	initGLTextureNam(gTextureNames[113], "textures/snow20.data", false, false);
	initGLTextureNam(gTextureNames[114], "textures/snow21.data", false, false);
	initGLTextureNam(gTextureNames[122], "textures/snowbg1.data", false, true);
	initGLTextureNam(gTextureNames[123], "textures/snowbg2.data", false, true);
	initGLTextureNam(gTextureNames[124], "textures/snowbg3.data", false, true);
	initGLTextureNam(gTextureNames[125], "textures/snowbg4.data", false, true);
	gTextureNames[128] = gTextureNames[26];
	gTextureNames[201] = gTextureNames[76];
	
	assert(glGetError() == GL_NO_ERROR);
}

static void drawGLvertices(const float *const vertices, const uint32_t texnam);

static int cmpForUint8_t(const void *p, const void *q) {
	const uint8_t *a = (const uint8_t *)p;
	const uint8_t *b = (const uint8_t *)q;
	int m = *a, n = *b;
	return m - n;
}

// Draw a tile?
static void paintTile(uint8_t tileID, int x, int y) {
	if (x < -100) {
		fprintf(stderr, "skipping a paintTile\n");
		return;
	}
	if (tileID == 0 || bsearch(&tileID, ignored_tiles,
		sizeof(ignored_tiles)/sizeof(uint8_t), sizeof(uint8_t), cmpForUint8_t))
		return;
	
	const float vertices[] = {
		x			, y,				1.0,
		x			, y+TILE_HEIGHT,	1.0,
		x+TILE_WIDTH, y,				1.0,
		x+TILE_WIDTH, y+TILE_HEIGHT,	1.0,
	};
	
	return drawGLvertices(vertices, gTextureNames[tileID]);
}

// Handy convenience function to make a new block. x and y are screen coords.
static const WorldItem *worldItem_new_block(enum stl_obj_type type, int x,
	int y) {
	int width = TILE_WIDTH, height = TILE_HEIGHT;
	if (type == SNOWBALL || type == MRICEBLOCK) {
		width -= 2;  // slightly smaller hitbox
		height -= 2;  // ibid
		assert(width == 30 && height == 30);
	}
	WorldItem *const w = worldItem_new(type, x, y, width, height,
		0, 0, false, NULL, fnret, false, false, false);
	if (type == STL_BONUS || type == STL_INVISIBLE)
		w->state = 1;
	return w;
}

// Draw some nice (non-interactive) scenery.
static void paintTM(uint8_t **tm) {
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

// Helper for loadLevel.
static void loadLevelInteractives() {
	// Load in the interactives all at once, one time.
	// (Painting for interactives happens elsewhere, repeatedly.)
	for (int h = 0; h < lvl.height; h++)
		for (int w = 0; w < lvl.width; w++) {
			uint8_t tileID = lvl.interactivetm[h][w];
			int x = w * TILE_WIDTH - gScrollOffset;  // screen coordinates
			int y = h * TILE_HEIGHT;  // ibid
			const uint8_t blocks[] = {  // tileIDs for solid tiles
				10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 25,
				27, 28, 29, 30, 31, 35, 36, 37, 38, 39, 40, 41, 42, 43, 47, 48,
				57, 58, 59, 60, 61, 84, 102, 103, 105, 113, 114,
				124, 125, 128,
			};
			if (bsearch(&tileID, blocks, sizeof(blocks)/sizeof(uint8_t), 
				sizeof(uint8_t), cmpForUint8_t))
				pushto_worldItems(worldItem_new_block(STL_BLOCK, x, y));
			else if (tileID == 26 || tileID == 83)
				pushto_worldItems(worldItem_new_block(STL_BONUS, x, y));
			else if (tileID == 77 || tileID == 78 || tileID == 104)
				pushto_worldItems(worldItem_new_block(STL_BRICK, x, y));
			else if (tileID == 112) {
				WorldItem *const wi = worldItem_new(STL_INVISIBLE, x, y,
					TILE_WIDTH, TILE_HEIGHT, 0, 0, false,
					"textures/transparent.data", fnret, false, false, true);
				wi->texnam2 = gTextureNames[112];
				wi->state = 1;
				pushto_worldItems(wi);
				assert(lvl.interactivetm[h][w] == 112);
				lvl.interactivetm[h][w] = 0;
			} else if (tileID == 132)
				pushto_worldItems(worldItem_new_block(STL_WIN, x, y));
			else if (tileID == 44 || tileID == 45 || tileID == 46)
				pushto_worldItems(worldItem_new_block(STL_COIN, x, y));
			else if ((tileID >= 85 && tileID <= 92) || tileID == 76 ||
				(tileID >= 7 && tileID <= 9) || tileID == 24 || tileID == 25 ||
				tileID == 122 || tileID == 123 || tileID == 201 ||
				(tileID >= 106 && tileID <= 111) ||
				(tileID >= 32 && tileID <= 34)) {
				// for some reason, cloud tiles show up in interactive-tm
				// 76 is a wave (water wave)
				// 7, 8, 9 are snow layer for the ground
				// 24 and 25 are patches of grass o_O
				// 106-111 are a pile of snow
				// 201 is "wave-trans-*.png"
				// 32-34 are dark snow layer for the ground
			} else if (tileID > 0 &&
				!bsearch(&tileID, ignored_tiles,
					sizeof(ignored_tiles)/sizeof(uint8_t), sizeof(uint8_t),
					cmpForUint8_t))
				fprintf(stderr, "DEBUG: unknown tileID %u\n", tileID);
		}
}

static void fndummy(WorldItem *const self) {
	assert(self);
	// do nothing
}

// Helper for loadLevel.
static void loadLevelObjects() {
	for (size_t i = 0; i < lvl.objects_len; i++) {
		WorldItem *w = NULL;
		stl_obj *obj = &lvl.objects[i];
		if (obj->type == SNOWBALL) {
			w = worldItem_new(SNOWBALL, obj->x, obj->y - 1,
				TILE_WIDTH, TILE_HEIGHT, BADGUY_X_SPEED, 1, true,
				NULL, fnsnowball, true, true, false);
			w->texnam = gObjTextureNames[STL_SNOWBALL_TEXTURE_LEFT];
			w->texnam2 = gObjTextureNames[STL_SNOWBALL_TEXTURE_RIGHT];
		} else if (obj->type == MRICEBLOCK) {
			w = worldItem_new(MRICEBLOCK, obj->x, obj->y - 1,
				TILE_WIDTH, TILE_HEIGHT, BADGUY_X_SPEED, 1, true,
				"textures/mriceblock.data", fniceblock, true, true, false);
		} else if (obj->type == BOUNCINGSNOWBALL) {
			w = worldItem_new(BOUNCINGSNOWBALL, obj->x, obj->y - 1,
				TILE_WIDTH, TILE_HEIGHT, BADGUY_X_SPEED, 1, true,
				NULL, fnbouncingsnowball, true, false, false);
			w->texnam = gObjTextureNames[STL_BOUNCINGSNOWBALL_TEXTURE_LEFT];
			w->texnam2 = gObjTextureNames[STL_BOUNCINGSNOWBALL_TEXTURE_RIGHT];
		} else if (obj->type == STL_BOMB) {
			w = worldItem_new(STL_BOMB, obj->x, obj->y - 1,
				TILE_WIDTH, TILE_HEIGHT, BADGUY_X_SPEED, 1, true, NULL, fnbomb, true, true,
				true);
			w->texnam = gObjTextureNames[STL_BOMB_TEXTURE_LEFT];
			w->texnam2 = gObjTextureNames[STL_BOMB_TEXTURE_RIGHT];
		} else if (obj->type == SPIKY) {
			w = worldItem_new(SPIKY, obj->x, obj->y - 1,
				TILE_WIDTH, TILE_HEIGHT, BADGUY_X_SPEED, 1, true, NULL, fnspiky, true, true,
				true);
			w->texnam = gObjTextureNames[STL_SPIKY_TEXTURE_LEFT];
			w->texnam2 = gObjTextureNames[STL_SPIKY_TEXTURE_RIGHT];
		} else if (obj->type == MONEY || obj->type == JUMPY) {
			w = worldItem_new(JUMPY, obj->x, obj->y - 1,
				TILE_WIDTH, TILE_HEIGHT, 0, JUMPY_JUMP_SPEED, true, NULL,
				fnjumpy, false, false, false);
			w->texnam = gObjTextureNames[STL_JUMPY_TEXTURE];
		} else if (obj->type == FLYINGSNOWBALL) {
			w = worldItem_new(FLYINGSNOWBALL, obj->x, obj->y - 1,
				TILE_WIDTH, TILE_HEIGHT, 0, FLYINGSNOWBALL_HOVER_SPEED, false,
				NULL, fnflyingsnowball, false, false, false);
			w->texnam = gObjTextureNames[STL_FLYINGSNOWBALL_TEXTURE_LEFT];
			w->texnam = gObjTextureNames[STL_FLYINGSNOWBALL_TEXTURE_RIGHT];
		} else if (obj->type == STALACTITE) {
			w = worldItem_new(STALACTITE, obj->x, obj->y - 1,
				TILE_WIDTH, TILE_HEIGHT, 0, 2, false, NULL, fnstalactite, false,
				false, false);
			w->texnam = gObjTextureNames[STL_STALACTITE_TEXTURE];
		} else {
			fprintf(stderr, "WARN: skipping the load of an obj!\n");
			continue;
		}
		pushto_worldItems(w);
	}
}

static bool loadLevel(const char *const level_filename) {
	// clean up from previous level load
	for (size_t i = 0; i < worldItems_len; i++) {
		//glDeleteTextures(1, &worldItems[i]->texnam);  // debug: turn back on
		//glDeleteTextures(1, &worldItems[i]->texnam2);  // someday.
		free(worldItems[i]);
	}
	worldItems_len = 0;
	gScrollOffset = 0;
	
	// load the new level
	lrFailCleanup(NULL, &lvl);
	lvl = levelReader(level_filename);
	if (!lvl.hdr)
		return false;
	stlPrinter(&lvl);
	pushto_worldItems(worldItem_new(STL_PLAYER, lvl.start_pos_x,
		lvl.start_pos_y, 30, 30, 0, 1, true,
		"textures/tux.data", fnpl, true, false, true));
	player = worldItems[0];
	loadLevelObjects();
	loadLevelInteractives();
	
	return true;
}

// Populate the gObjTextureNames array with textures (that last the whole game).
static bool populateGOTN() {
	glGenTextures(gOTNlen, gObjTextureNames);
	
	initGLTextureNam(gObjTextureNames[STL_DEAD_MRICEBLOCK_TEXTURE_LEFT],
		"textures/mriceblock-flat-left.data", false, false);
	initGLTextureNam(gObjTextureNames[STL_DEAD_MRICEBLOCK_TEXTURE_RIGHT],
		"textures/mriceblock-flat-left.data", true, false);
	initGLTextureNam(gObjTextureNames[STL_SNOWBALL_TEXTURE_LEFT],
		"textures/Asnowball.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_SNOWBALL_TEXTURE_RIGHT],
		"textures/Asnowball.data", true, true);
	initGLTextureNam(gObjTextureNames[STL_BOUNCINGSNOWBALL_TEXTURE_LEFT],
		"textures/Abouncingsnowball.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_BOUNCINGSNOWBALL_TEXTURE_RIGHT],
		"textures/Abouncingsnowball.data", true, true);
	initGLTextureNam(gObjTextureNames[STL_BOMB_TEXTURE_LEFT],
		"textures/bomb.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_BOMB_TEXTURE_RIGHT],
		"textures/bomb.data", true, true);
	initGLTextureNam(gObjTextureNames[STL_BOMBX_TEXTURE_LEFT],
		"textures/bombx.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_BOMBX_TEXTURE_RIGHT],
		"textures/bombx.data", true, true);
	initGLTextureNam(gObjTextureNames[STL_BOMB_EXPLODING_TEXTURE_1],
		"textures/bomb-explosion.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_BOMB_EXPLODING_TEXTURE_2],
		"textures/bomb-explosion-1.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_SPIKY_TEXTURE_LEFT],
		"textures/spiky.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_SPIKY_TEXTURE_RIGHT],
		"textures/spiky.data", true, true);
	initGLTextureNam(gObjTextureNames[STL_FLYINGSNOWBALL_TEXTURE_LEFT],
		"textures/flyingsnowball.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_FLYINGSNOWBALL_TEXTURE_RIGHT],
		"textures/flyingsnowball.data", true, true);
	initGLTextureNam(gObjTextureNames[STL_STALACTITE_TEXTURE],
		"textures/stalactite.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_JUMPY_TEXTURE], "textures/jumpy.data",
		false, true);
		
	return glGetError() == GL_NO_ERROR;
}

static bool loadLevelBackground() {
	char *filename = lvl.background;
	
	// investigate taking off the filename extension
	if (strlen(filename) > 4 &&
		0 == strcmp(".jpg", filename + strlen(filename) - 4)) {
		filename = nnrealloc(filename, strlen(filename) + 1 + 1);  // ".data\0"
		strcpy(filename + strlen(filename) - 4, ".data");
		lvl.background = filename;
	}
	
	size_t path_len = strlen("textures/") + strlen(filename) + 1;
	char *path = nnmalloc(path_len);
	strcpy(path, "textures/");
	strcat(path, filename);
	//path[path_len - 1] = '\0';
	
	ssize_t imgdat_len;
	char *imgdat = safe_read(path, &imgdat_len);
	free(path);
	assert(imgdat_len == 640 * 480 * 4);
	glBindTexture(GL_TEXTURE_2D, gTextureNames[256]);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(
		GL_TEXTURE_2D,
		0,
		GL_RGBA,
		640,
		480,
		0,
		GL_RGBA,
		GL_UNSIGNED_BYTE,
		imgdat
	);
	free(imgdat);
	
	return glGetError() == GL_NO_ERROR;
}

static void initialize() {
	initialize_prgm();
	maybeInitgTextureNames();
	
	assert(populateGOTN());
	assert(loadLevel("gpl/levels/level8.stl"));  // xxx
	gCurrLevel = 8;  // hack for debugging xxx
	
	assert(loadLevelBackground());
}

// Return true if w is off-screen.
static bool isOffscreen(const WorldItem *const w) {
	int top, bottom, left, right;
	
	top = w->y;
	bottom = w->y + w->height;
	left = w->x;
	right = w->x + w->width;
	
	return (top > SCREEN_HEIGHT || bottom < 0 ||
		left > SCREEN_WIDTH || right < 0);
}

static void drawGLvertices(const float *const vertices, const uint32_t texnam) {
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

static void drawLevelBackground() {
	float backgroundVertices[] = {
		0 - gScrollOffset % SCREEN_WIDTH,				0,				1.0,
		0 - gScrollOffset % SCREEN_WIDTH,				SCREEN_HEIGHT,	1.0,
		SCREEN_WIDTH - gScrollOffset % SCREEN_WIDTH,	0,				1.0,
		SCREEN_WIDTH - gScrollOffset % SCREEN_WIDTH,	SCREEN_HEIGHT,	1.0,
	};
	drawGLvertices(backgroundVertices, gTextureNames[256]);
	
	// if the previous draw doesn't cover the entire screen ...
	if (gScrollOffset % SCREEN_WIDTH != 0) {
		backgroundVertices[0] = SCREEN_WIDTH - gScrollOffset % SCREEN_WIDTH;
		backgroundVertices[3] = SCREEN_WIDTH - gScrollOffset % SCREEN_WIDTH;
		backgroundVertices[6] = 2 * SCREEN_WIDTH - gScrollOffset % SCREEN_WIDTH;
		backgroundVertices[9] = 2 * SCREEN_WIDTH - gScrollOffset % SCREEN_WIDTH;
		drawGLvertices(backgroundVertices, gTextureNames[256]);
	}
}

static void clearScreen() {
	glClearColor(30.0/255, 85.0/255, 150.0/255, 1);  // light blue
	glClear(GL_COLOR_BUFFER_BIT);
	drawLevelBackground();
}

static point selectResetPoint() {
	point *rp = lvl.reset_points;
	point ret = { -1, -1, NULL };
	while (rp) {  // assume lvl.reset_points is sorted
		if (gScrollOffset + SCREEN_WIDTH / 3 >= rp->x) {
			ret.x = rp->x;
			ret.y = rp->y;
		} else
			break;
		rp = rp->next;
	}
	return ret;
}

static void reloadLevel(keys *const k, bool ignoreCheckpoints) {
	assert(k);
	if (gCurrLevel > 8)
		gCurrLevel = 1;  // hack
	
	point rp;
	if (!ignoreCheckpoints)
		rp = selectResetPoint();
	
	const char *file = NULL;
	if (gCurrLevel == 1)  // todo: make some string malloc()'ing behavior here
		file = "gpl/levels/level1.stl";
	else if (gCurrLevel == 2)
		file = "gpl/levels/level2.stl";
	else if (gCurrLevel == 3)
		file = "gpl/levels/level3.stl";
	else if (gCurrLevel == 4)
		file = "gpl/levels/level4.stl";
	else if (gCurrLevel == 5)
		file = "gpl/levels/level5.stl";
	else if (gCurrLevel == 6)
		file = "gpl/levels/level6.stl";
	else if (gCurrLevel == 7)
		file = "gpl/levels/level7.stl";
	else if (gCurrLevel == 8)
		file = "gpl/levels/level8.stl";
	assert(loadLevel(file));
	
	if (!ignoreCheckpoints && rp.x != -1 && rp.y != -1) {
		if (rp.x - SCREEN_WIDTH / 3 > 0)
			scrollTheScreen(rp.x - SCREEN_WIDTH / 3);
		player->x = SCREEN_WIDTH / 3;
		player->y = rp.y;
	}
	
	gPlayerLastDirectionWasRight = true;  // player starts facing right
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
	
	if (player->type == STL_PLAYER_DEAD) {  // reload the current level
		reloadLevel(k, false);
	} else if (player->type == STL_PLAYER_ASCENDED) {  // reload the next level
		gCurrLevel++;
		reloadLevel(k, true);
	}
}

// Entry point for initgl.
bool draw(keys *const k) {
	core(k);
	return true;
}


// stlplayer.c

#include "initgl.h"
#include "stlplayer.h"

static const int SCREEN_WIDTH = 640, SCREEN_HEIGHT = 480;
static const int TILE_WIDTH = 32, TILE_HEIGHT = 32;
static int gScrollOffset = 0, gCurrLevel = 1, gNDeaths = 0;

typedef int Direction;
enum { LEFT, RIGHT, UP, DOWN,
	GDIRECTION_HORIZ, GDIRECTION_VERT, GDIRECTION_BOTH, };
typedef int GeneralDirection;

static const int BUCKETS_SIZE = 250;
static WorldItem **gBuckets;  // array of pointers to WorldItem (ll)
static size_t gBuckets_len;

static stl lvl;

static Player *player;
static WorldItem *gPlayerCarry;

static const float MRICEBLOCK_KICKSPEED = 8;
static const float PLAYER_BOUNCE_SPEED = -8;
static const float PLAYER_JUMP_SPEED = -8;
static const float PLAYER_RUN_SPEED = 6;
static const float BOUNCINGSNOWBALL_JUMP_SPEED = -7;
static const float FLYINGSNOWBALL_HOVER_SPEED = -2;
static float BADGUY_X_SPEED = -2;
static float JUMPY_JUMP_SPEED = -8;

static const uint8_t ignored_tiles[] = {
	6, 126, 133,
};  // todo burndown

enum gOTNi {
	STL_TUX_LEFT = 0,
	STL_TUX_RIGHT,
	STL_ICEBLOCK_TEXTURE_LEFT,
	STL_ICEBLOCK_TEXTURE_RIGHT,
	STL_DEAD_MRICEBLOCK_TEXTURE_LEFT,
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
	int h, float spx, float spy, bool gravity, void(*frame)(WorldItem *const),
	bool patrol, uint32_t texnam, uint32_t texnam2) {
	assert(wi > 0 && h > 0 && wi < BUCKETS_SIZE);
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
	w->next = NULL;
	
	w->texnam = texnam;
	w->texnam2 = texnam2;
	
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

// Delete w from gBuckets. (w is not freed here.)
static void delFromBuckets(WorldItem *const w) {
	assert(w->x > -BUCKETS_SIZE);
	const size_t bucket = w->x / BUCKETS_SIZE;
	assert(bucket < gBuckets_len && gBuckets[bucket]);
	
	WorldItem *curr = gBuckets[bucket];
	while (curr) {
		if (curr->next == w) {
			curr->next = curr->next->next;
			w->next = NULL;
			return;
		}
		curr = curr->next;
	}
	
	assert(NULL);  // wrong bucket?
}

// Add w to gBuckets.
static void addToBuckets(WorldItem *const w) {
	assert(BUCKETS_SIZE > 0 && w->x > -BUCKETS_SIZE && w->next == NULL);
	
	const size_t bucket = w->x / BUCKETS_SIZE;
	assert(bucket < gBuckets_len);

	WorldItem *dnext = gBuckets[bucket]->next;
	gBuckets[bucket]->next = w;
	w->next = dnext;
}

static void setX(WorldItem *const w, int x) {
	const ssize_t bucketOrig = w->x / BUCKETS_SIZE;
	const ssize_t bucketNew = x / BUCKETS_SIZE;
	assert(bucketOrig >= 0 && bucketNew >= 0);
	
	if (bucketNew != bucketOrig) {
		delFromBuckets(w);
		w->x = x;
		addToBuckets(w);
	} else
		w->x = x;
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
		setX(w, w->x - 1);
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
		setX(w, w->x + 1);
		w->width -= 2;
	}
	if (dir == GDIRECTION_VERT || dir == GDIRECTION_BOTH) {
		w->y++;
		w->height -= 2;
	}
}

// Traverse list looking for collisions with w.
static void checkLLCollisions(const WorldItem *const w, WorldItem *list,
	WorldItem ***rv, size_t *const pcolls_len, size_t *const pcolls_cap) {
	for (; list; list = list->next) {
		if (!isCollidingHorizontally(w, list) ||
			!isCollidingVertically(w, list)) {
			continue;
		}
		
		if (*pcolls_len == *pcolls_cap) {
			assert(*pcolls_cap <= SIZE_MAX / 2);
			*pcolls_cap *= 2;
			*rv = nnrealloc(*rv, sizeof(WorldItem *) * *pcolls_cap);
		}
		(*rv)[(*pcolls_len)++] = list;
	}
}

// Return an array of WorldItems w is colliding with. The caller must free rv
// (but not the pointers stored in rv).
static WorldItem **isCollidingWith(WorldItem *const w, size_t *const pcolls_len,
	GeneralDirection gdir) {
	assert(gdir == GDIRECTION_HORIZ || gdir == GDIRECTION_VERT ||
		gdir == GDIRECTION_BOTH);
	iCW_expand_w(w, gdir);
	WorldItem **rv = malloc(sizeof(WorldItem *) * 1);
	*pcolls_len = 0;
	size_t colls_cap = 1;
	
	const size_t bucket = w->x / BUCKETS_SIZE;
	if (bucket > 0)
		checkLLCollisions(w, gBuckets[bucket - 1]->next, &rv, pcolls_len,
			&colls_cap);
	checkLLCollisions(w, gBuckets[bucket]->next, &rv, pcolls_len, &colls_cap);
	if (bucket < gBuckets_len - 1)
		checkLLCollisions(w, gBuckets[bucket + 1]->next, &rv, pcolls_len,
			&colls_cap);
	
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
			setX(w, w->x + i);
		else
			setX(w, w->x - i);
		bool shouldBreak = false;
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
		setX(w, origX);
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
}

// Free w->next in the list.
static void delNodeAfter(WorldItem *w) {
	WorldItem *trash = w->next;
	w->next = w->next->next;
	memset(trash, 0xe5, sizeof(*trash));
	free(trash);
}

// Scroll everything diff pixels left.
static void scrollTheScreen(int diff) {
	for (size_t i = 0; i < gBuckets_len; i++) {
		WorldItem *node = gBuckets[i];
		while (node && node->next) {
			node->next->x -= diff;
			if (node->next->x < -100) {
				node->next->x = -100;  // pile them up lol
			}
			const size_t properBucket = node->next->x / BUCKETS_SIZE;
			if (properBucket != i) {
				WorldItem *migrate = node->next;
				node->next = node->next->next;
				migrate->next = NULL;
				addToBuckets(migrate);
			} else
				node = node->next;
		}
	}
	
	gScrollOffset += diff;
}

// Delete all WorldItems far past the left edge of the screen.
static void cleanupWorldItems() {
	WorldItem *w = gBuckets[0];
	while (w && w->next) {
		if (w->next->x <= -100) {
			WorldItem *trash = w->next;
			w->next = w->next->next;
			memset(trash, 0xe4, sizeof(*w));
			free(trash);
		} else
			w = w->next;
	}
}

static void maybeScrollScreen() {
	if (player->x <= SCREEN_WIDTH / 3 ||
		gScrollOffset + SCREEN_WIDTH >= lvl.width * TILE_WIDTH)
		return;
	
	scrollTheScreen(player->x - SCREEN_WIDTH / 3);
	//cleanupWorldItems();  // let cleanup be done by the caller
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
		
		setX(player, player->x + canMoveTo(player, GDIRECTION_HORIZ));
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
		
		setX(player, player->x + canMoveTo(player, GDIRECTION_HORIZ));
		if (gPlayerLastDirectionWasRight)
			wiSwapTextures(player);
		gPlayerLastDirectionWasRight = false;
	}
	if (!k->keyD && !k->keyRight && !k->keyA && !k->keyLeft) {  // slow down
		player->speedX *= 0.66;
		if (fabs(player->speedX) < 0.1)
			player->speedX = 0;
		setX(player, player->x + canMoveTo(player, GDIRECTION_HORIZ));
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
	} else if (!k->keyCTRL) {
		player->state = 0;
		gPlayerCarry = 0;
	}
	
	maybeScrollScreen();
}

static void applyGravity() {
	for (size_t i = 0; i < gBuckets_len; i++)
		for (WorldItem *curr = gBuckets[i]->next; curr; curr = curr->next) {
			if (curr->gravity == false)
				continue;
			int moveY = canMoveTo(curr, GDIRECTION_VERT);
			if (moveY == 0)
				curr->speedY = 1;
			else {
				curr->speedY += 0.25;
				curr->y += moveY;
			}
		}
}

// Opposite of initialize().
void terminate() {
	for (size_t i = 0; i < gBuckets_len; i++)
		for (WorldItem *w = gBuckets[i]; w;) {
			WorldItem *next = w->next;
			free(w);
			w = next;
		}
	memset(gBuckets, 0xe4, gBuckets_len * sizeof(WorldItem *));  // debug
	free(gBuckets);
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

static WorldItem *worldItem_new_block(enum stl_obj_type type, int x, int y);

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
		setX(coll, player->x + player->width);
		if (coll->speedX < 0)
			wiSwapTextures(coll);
		coll->speedX = MRICEBLOCK_KICKSPEED;  // iceblock goes right
	} else {  // player is right of the iceblock
		setX(coll, player->x - coll->width);
		if (coll->speedX > 0)
			wiSwapTextures(coll);
		coll->speedX = -MRICEBLOCK_KICKSPEED;  // iceblock goes left
	}
}

static void killPlayer() {
	fprintf(stderr, "You died.\n");
	player->type = STL_PLAYER_DEAD;
	gPlayerCarry = NULL;
}

// Turn around a WorldItem (that has two texnams). Relatively cheap fn.
static void turnAround(WorldItem *const self) {
	assert(self->speedX != INT_MIN);
	self->speedX *= -1;  // toggle horizontal direction
	wiSwapTextures(self);
}

// for patrol or smthn
static void maybeTurnAround(WorldItem *const self) {
	if (self->speedX == 0)
		return;
	int origX = self->x;
	// calculate hypothetical move
	if (self->speedX < 0)  // going left
		setX(self, self->x - self->width);
	else  // going right
		setX(self, self->x + self->width);

	size_t collisions_len;
	WorldItem **colls = isCollidingWith(self, &collisions_len, GDIRECTION_VERT);
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
	setX(self, origX);
}

// Callback for bot frame. Move the bot around horizontally.
static void fnbot(WorldItem *const self) {
	if (self->patrol)
		maybeTurnAround(self);  // patrol

	int moveX = canMoveTo(self, GDIRECTION_HORIZ);
	if (moveX == 0) {
		turnAround(self);
	}
	setX(self, self->x + moveX);
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

static void fnspiky(WorldItem *const self) {
	if (hitScreenBottom(self)) {
		self->type = STL_DEAD;
		return;
	}
	fnbot(self);
}

// Return a new WorldItem bouncingsnowball.
static WorldItem *worldItem_new_bsnowball(int x, int y) {
	WorldItem *bs = worldItem_new(BOUNCINGSNOWBALL, x, y - 1,
		TILE_WIDTH - 2, TILE_HEIGHT - 2, BADGUY_X_SPEED, 1, true,
		fnbouncingsnowball, false, 
		gObjTextureNames[STL_BOUNCINGSNOWBALL_TEXTURE_LEFT],
		gObjTextureNames[STL_BOUNCINGSNOWBALL_TEXTURE_RIGHT]);
	return bs;
}

// Return a new WorldItem snowball.
static WorldItem *worldItem_new_snowball(int x, int y, bool patrol) {
	WorldItem *sb = worldItem_new(SNOWBALL, x, y - 1,
		TILE_WIDTH - 2, TILE_HEIGHT - 2, BADGUY_X_SPEED, 1, true,
		fnsnowball, patrol, gObjTextureNames[STL_SNOWBALL_TEXTURE_LEFT],
		gObjTextureNames[STL_SNOWBALL_TEXTURE_RIGHT]);
	return sb;
}

static WorldItem *worldItem_new_spiky(int x, int y, bool patrol) {
	WorldItem *spiky = worldItem_new(SPIKY, x, y - 1,
		TILE_WIDTH - 2, TILE_HEIGHT - 2, BADGUY_X_SPEED, 1, true,
		fnspiky, patrol, gObjTextureNames[STL_SPIKY_TEXTURE_LEFT],
		gObjTextureNames[STL_SPIKY_TEXTURE_RIGHT]);
	return spiky;
}

// Callback for player frame.
static void fnpl(WorldItem *self) {
	if (hitScreenBottom(self)) {
		killPlayer();
	}
	if (gPlayerCarry) {
		if (self->speedX < 0)
			setX(gPlayerCarry, self->x - gPlayerCarry->width);
		else if (self->speedX > 0)
			setX(gPlayerCarry, self->x + self->width);
		gPlayerCarry->y = self->y;
	}

	size_t collisions_len;
	WorldItem **colls = isCollidingWith(self, &collisions_len, GDIRECTION_BOTH);
	for (size_t i = 0; i < collisions_len; i++) {
		int x = (colls[i]->x + gScrollOffset) / TILE_WIDTH;
		int y = colls[i]->y / TILE_HEIGHT;
		switch (colls[i]->type) {
			case SPIKY:
				killPlayer();
				break;
			case MRICEBLOCK:
				if (self->speedY >= 0)
					self->speedY = PLAYER_BOUNCE_SPEED;
				if (bottomOf(self) - topOf(colls[i]) < (TILE_HEIGHT - 2) / 3) {
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
				} else
					killPlayer();
				break;
			case STL_DEAD_MRICEBLOCK:  // just sitting there
				if (self->state == 0) {
					colls[i]->gravity = true;
					playerKicks(colls[i]);
				} else if (self->state == 1) {
					colls[i]->gravity = false;
					if (self->speedX > 0)  // player is going right
						setX(colls[i], self->x + self->width / 2);
					else if (self->speedX < 0)  // player is going left
						setX(colls[i], self->x - self->width / 2);
					colls[i]->y = self->y;
					self->state = 2;
					gPlayerCarry = colls[i];
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
				if (bottomOf(self) + 1 == topOf(colls[i])) {
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
				} else if (topOf(self) - 1 == bottomOf(colls[i]) ||
					leftOf(self) - 1 == rightOf(colls[i]) ||
					rightOf(self) + 1 == leftOf(colls[i])) {
					fprintf(stderr, "You died.\n");
					self->type = STL_PLAYER_DEAD;
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
					lvl.interactivetm[y][x] = 84;
					lvl.interactivetm[y - 1][x] = 44;
					addToBuckets(worldItem_new_block(
						STL_COIN,
						colls[i]->x,
						colls[i]->y - TILE_HEIGHT
					));  // hairy! xxx
				} else if (colls[i]->state == 2 &&  // bonus egg
					topOf(self) - 1 == bottomOf(colls[i])) {
					colls[i]->state = 0;
					lvl.interactivetm[y][x] = 84;
					WorldItem *snowball = worldItem_new_snowball(
						colls[i]->x,
						colls[i]->y - TILE_HEIGHT,
						false
					);
					turnAround(snowball);
					addToBuckets(snowball);  // hairy! xxx
				} else if (colls[i]->state == 3 &&  // bonus star
					topOf(self) - 1 == bottomOf(colls[i])) {
					colls[i]->state = 0;
					lvl.interactivetm[y][x] = 84;
					WorldItem *bsnowball = worldItem_new_bsnowball(
						colls[i]->x,
						colls[i]->y - TILE_HEIGHT
					);
					turnAround(bsnowball);  // face right
					addToBuckets(bsnowball);  // hairy! xxx
				} else if (colls[i]->state == 4 &&  // bonus 1up
					topOf(self) - 1 == bottomOf(colls[i])) {
					colls[i]->state = 0;
					lvl.interactivetm[y][x] = 84;
					WorldItem *spiky = worldItem_new_spiky(
						colls[i]->x,
						colls[i]->y - TILE_HEIGHT,
						false
					);
					turnAround(spiky);  // face right
					addToBuckets(spiky);  // hairy! xxx
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

static void deadIceOnPlayerHandleCollisions(void) {
	size_t colls_len;
	WorldItem **colls = isCollidingWith(gPlayerCarry, &colls_len,
		GDIRECTION_BOTH);
	for (size_t i = 0; i < colls_len; i++) {
		switch (colls[i]->type) {
			case SNOWBALL:
			case STL_KICKED_MRICEBLOCK:
			case STL_DEAD_MRICEBLOCK:
			case MRICEBLOCK:
			case STL_BOMB:
			case STALACTITE:
			case BOUNCINGSNOWBALL:
			case FLYINGSNOWBALL:
			case MONEY:
			case JUMPY:
				gPlayerCarry->type = STL_DEAD;
				colls[i]->type = STL_DEAD;
		}
	}
	if (gPlayerCarry->type == STL_DEAD) {
		player->state = 0;
		gPlayerCarry = NULL;
	}
}

static void fniceblock(WorldItem *const self) {
	if (hitScreenBottom(self)) {
		self->type = STL_DEAD;
		return;
	}
	if (self->type == STL_DEAD_MRICEBLOCK) {
		if (self == gPlayerCarry) {
			deadIceOnPlayerHandleCollisions();
		}
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
			setX(self, self->x + moveX);
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
	setX(self, self->x - self->width);
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
				killPlayer();
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
				int x = (coll->x + gScrollOffset) / TILE_WIDTH;
				int y = coll->y / TILE_HEIGHT;
				lvl.interactivetm[y][x] = 84;
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

static void fnflyingsnowball(WorldItem *const self) {
	static int totalDistMoved = 0;
	
	const int speedYOrig = self->speedY;
	const int moveY = canMoveTo(self, GDIRECTION_VERT);
	if (moveY == 0 || totalDistMoved >= 3 * TILE_HEIGHT) {
		assert(self->speedY != INT_MIN);
		if (self->speedY == speedYOrig)
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

// Run the frame functions of elements of worldItems.
static void applyFrame() {
	fnpl(player);  // the player's callback frame must be run first

	for (size_t i = 0; i < gBuckets_len; i++)
		for (WorldItem *curr = gBuckets[i]->next; curr; curr = curr->next) {
			if (curr == player)
				continue;
			curr->frame(curr);
		}
	
	for (size_t i = 0; i < gBuckets_len; i++) {
		WorldItem *w = gBuckets[i];
		while (w && w->next) {
			if (w->next->type == STL_DEAD) {
				delNodeAfter(w);
			} else if (w->next->type == STL_BRICK_DESTROYED) {
				int x = (w->next->x + gScrollOffset) / TILE_WIDTH;
				int y = w->next->y / TILE_HEIGHT;
				lvl.interactivetm[y][x] = 0;
				delNodeAfter(w);
			} else
				w = w->next;
		}
	}
}

// [0-256], plus background image, plus "transparent" tile
static uint32_t gTextureNames[258];

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
	assert(!ran);
	ran = true;
	
	glGenTextures(258, gTextureNames);
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
	initGLTextureNam(gTextureNames[26], "textures/bonus2.data", false, true);
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
	
	initGLTextureNam(gTextureNames[49], "textures/block6.data", false, false);
	initGLTextureNam(gTextureNames[50], "textures/block7.data", false, false);
	initGLTextureNam(gTextureNames[51], "textures/block8.data", false, false);
	initGLTextureNam(gTextureNames[52], "textures/block9.data", false, false);
	
	initGLTextureNam(gTextureNames[53], "textures/pipe1.data", false, true);
	initGLTextureNam(gTextureNames[54], "textures/pipe2.data", false, true);
	initGLTextureNam(gTextureNames[55], "textures/pipe3.data", false, true);
	initGLTextureNam(gTextureNames[56], "textures/pipe4.data", false, true);
	
	initGLTextureNam(gTextureNames[57], "textures/pipe5.data", false, true);
	initGLTextureNam(gTextureNames[58], "textures/pipe6.data", false, true);
	initGLTextureNam(gTextureNames[59], "textures/pipe7.data", false, true);
	initGLTextureNam(gTextureNames[60], "textures/pipe8.data", false, true);
	initGLTextureNam(gTextureNames[61], "textures/block10.data", false, true);
	
	initGLTextureNam(gTextureNames[75], "textures/water.data", false, true);
	
	initGLTextureNam(gTextureNames[76], "textures/waves-1.data", false, true);
	initGLTextureNam(gTextureNames[77], "textures/brick0.data", false, false);
	initGLTextureNam(gTextureNames[78], "textures/brick1.data", false, false);
	gTextureNames[83] = gTextureNames[26];
	initGLTextureNam(gTextureNames[84], "textures/bonus2-d.data", false, true);
	initGLTextureNam(gTextureNames[85], "textures/Acloud-00.data", false, true);
	initGLTextureNam(gTextureNames[86], "textures/Acloud-01.data", false, true);
	initGLTextureNam(gTextureNames[87], "textures/Acloud-02.data", false, true);
	initGLTextureNam(gTextureNames[88], "textures/Acloud-03.data", false, true);
	initGLTextureNam(gTextureNames[89], "textures/Acloud-10.data", false, true);
	initGLTextureNam(gTextureNames[90], "textures/Acloud-11.data", false, true);
	initGLTextureNam(gTextureNames[91], "textures/Acloud-12.data", false, true);
	initGLTextureNam(gTextureNames[92], "textures/Acloud-13.data", false, true);
	gTextureNames[102] = gTextureNames[26];  // bonus egg
	gTextureNames[103] = gTextureNames[26];  // bonus star
	gTextureNames[104] = gTextureNames[77];
	gTextureNames[105] = gTextureNames[78];
	initGLTextureNam(gTextureNames[79], "textures/pole.data", false, true);
	initGLTextureNam(gTextureNames[106], "textures/background1.data", false, true);
	initGLTextureNam(gTextureNames[107], "textures/background2.data", false, true);
	initGLTextureNam(gTextureNames[108], "textures/background3.data", false, true);
	initGLTextureNam(gTextureNames[109], "textures/background4.data", false, true);
	initGLTextureNam(gTextureNames[110], "textures/background5.data", false, true);
	initGLTextureNam(gTextureNames[111], "textures/background6.data", false, true);
	initGLTextureNam(gTextureNames[112], "textures/transparent2.data", false, true);
	initGLTextureNam(gTextureNames[113], "textures/snow20.data", false, false);
	initGLTextureNam(gTextureNames[114], "textures/snow21.data", false, false);
	
	gTextureNames[119] = gTextureNames[36];
	gTextureNames[120] = gTextureNames[36];
	gTextureNames[121] = gTextureNames[36];
	
	initGLTextureNam(gTextureNames[122], "textures/snowbg1.data", false, true);
	initGLTextureNam(gTextureNames[123], "textures/snowbg2.data", false, true);
	initGLTextureNam(gTextureNames[124], "textures/snowbg3.data", false, true);
	initGLTextureNam(gTextureNames[125], "textures/snowbg4.data", false, true);
	gTextureNames[128] = gTextureNames[26];  // bonus 1up
	
	initGLTextureNam(gTextureNames[200], "textures/water-trans.data", false, true);
	initGLTextureNam(gTextureNames[201], "textures/waves-trans.data", false, true);
	
	initGLTextureNam(gTextureNames[257], "textures/transparent.data", false, true);
	
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
static WorldItem *worldItem_new_block(enum stl_obj_type type, int x, int y) {
	int width = TILE_WIDTH, height = TILE_HEIGHT;
	if (type == SNOWBALL || type == MRICEBLOCK || type == STL_BOMB) {
		width -= 2;  // slightly smaller hitbox
		height -= 2;  // ibid
		assert(width == 30 && height == 30);
	}
	WorldItem *const w = worldItem_new(type, x, y, width, height,
		0, 0, false, fnret, false, 0, 0);
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
			const int x = w * TILE_WIDTH - gScrollOffset;  // screen coordinates
			const int y = h * TILE_HEIGHT;  // ibid
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
				49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 84, 105,
				113, 114, 119, 120, 121, 124, 125,
			};
			if (bsearch(&tileID, blocks, sizeof(blocks)/sizeof(uint8_t), 
				sizeof(uint8_t), cmpForUint8_t))
				addToBuckets(worldItem_new_block(STL_BLOCK, x, y));
			else if (tileID == 26 || tileID == 83 || tileID == 102 ||
				tileID == 103 || tileID == 128) {
				WorldItem *w = worldItem_new_block(STL_BONUS, x, y);
				if (tileID == 102)
					w->state = 2;
				if (tileID == 103)
					w->state = 3;
				if (tileID == 128)
					w->state = 4;
				addToBuckets(w);
			} else if (tileID == 77 || tileID == 78 || tileID == 104)
				addToBuckets(worldItem_new_block(STL_BRICK, x, y));
			else if (tileID == 112) {
				WorldItem *const wi = worldItem_new(STL_INVISIBLE, x, y,
					TILE_WIDTH, TILE_HEIGHT, 0, 0, false,
					fnret, false, gTextureNames[257], gTextureNames[112]);
				wi->state = 1;
				addToBuckets(wi);
				assert(lvl.interactivetm[h][w] == 112);
				lvl.interactivetm[h][w] = 0;
			} else if (tileID == 132)
				addToBuckets(worldItem_new_block(STL_WIN, x, y));
			else if (tileID == 44 || tileID == 45 || tileID == 46)
				addToBuckets(worldItem_new_block(STL_COIN, x, y));
			else if ((tileID >= 85 && tileID <= 92) || tileID == 76 ||
				(tileID >= 7 && tileID <= 9) || tileID == 24 || tileID == 25 ||
				tileID == 122 || tileID == 123 || tileID == 201 ||
				(tileID >= 106 && tileID <= 111) ||
				(tileID >= 32 && tileID <= 34) || tileID == 79 ||
				tileID == 75) {
				// for some reason, cloud tiles show up in interactive-tm
				// 75 and 76 are water and wave
				// 7, 8, 9 are snow layer for the ground
				// 24 and 25 are patches of grass o_O
				// 106-111 are a pile of snow
				// 201 is "wave-trans-*.png"
				// 32-34 are dark snow layer for the ground
				// 79 is a pole
			} else if (tileID > 0 &&
				!bsearch(&tileID, ignored_tiles,
					sizeof(ignored_tiles)/sizeof(uint8_t), sizeof(uint8_t),
					cmpForUint8_t))
				fprintf(stderr, "DEBUG: unknown tileID %u\n", tileID);
		}
}

// Helper for loadLevel.
static void loadLevelObjects() {
	for (size_t i = 0; i < lvl.objects_len; i++) {
		WorldItem *w = NULL;
		stl_obj *obj = &lvl.objects[i];
		if (obj->type == SNOWBALL) {
			w = worldItem_new_snowball(obj->x, obj->y, true);
		} else if (obj->type == MRICEBLOCK) {
			w = worldItem_new(MRICEBLOCK, obj->x, obj->y - 1,
				TILE_WIDTH - 2, TILE_HEIGHT - 2, BADGUY_X_SPEED, 1, true,
				fniceblock, true, gObjTextureNames[STL_ICEBLOCK_TEXTURE_LEFT],
				gObjTextureNames[STL_ICEBLOCK_TEXTURE_RIGHT]);
		} else if (obj->type == BOUNCINGSNOWBALL) {
			w = worldItem_new_bsnowball(obj->x, obj->y);
		} else if (obj->type == STL_BOMB) {
			w = worldItem_new(STL_BOMB, obj->x, obj->y - 1,
				TILE_WIDTH - 2, TILE_HEIGHT - 2, BADGUY_X_SPEED, 1, true,
				fnbomb, true, gObjTextureNames[STL_BOMB_TEXTURE_LEFT],
				gObjTextureNames[STL_BOMB_TEXTURE_RIGHT]);
		} else if (obj->type == SPIKY) {
			w = worldItem_new_spiky(obj->x, obj->y, true);
		} else if (obj->type == MONEY || obj->type == JUMPY) {
			w = worldItem_new(JUMPY, obj->x, obj->y - 1,
				TILE_WIDTH - 2, TILE_HEIGHT - 2, 0, JUMPY_JUMP_SPEED, true,
				fnjumpy, false, gObjTextureNames[STL_JUMPY_TEXTURE], 0);
		} else if (obj->type == FLYINGSNOWBALL) {
			w = worldItem_new(FLYINGSNOWBALL, obj->x, obj->y - 1,
				TILE_WIDTH - 2, TILE_HEIGHT - 2, 0, FLYINGSNOWBALL_HOVER_SPEED, 
				false, fnflyingsnowball, false,
				gObjTextureNames[STL_FLYINGSNOWBALL_TEXTURE_LEFT],
				gObjTextureNames[STL_FLYINGSNOWBALL_TEXTURE_RIGHT]);
		} else if (obj->type == STALACTITE) {
			w = worldItem_new(STALACTITE, obj->x, obj->y - 1,
				TILE_WIDTH - 2, TILE_HEIGHT - 2, 0, 2, false,
				fnstalactite, false, gObjTextureNames[STL_STALACTITE_TEXTURE],
				0);
		} else {
			fprintf(stderr, "WARN: skipping the load of an obj!\n");
			continue;
		}
		addToBuckets(w);
	}
}

// Free every node pointed to by head.
static void freeLinkedList(WorldItem *head) {
	while (head) {
		WorldItem *const next = head->next;
		memset(head, 0xe5, sizeof(*head));  // debug
		free(head);
		head = next;
	}
}

// Initialize gBuckets for use.
static void initBuckets() {
	assert(lvl.width > 0 && BUCKETS_SIZE > 0);
	
	gBuckets_len = lvl.width * TILE_WIDTH / BUCKETS_SIZE;
	if (lvl.width * TILE_WIDTH % BUCKETS_SIZE != 0)
		gBuckets_len++;
	
	gBuckets = nnmalloc(gBuckets_len * sizeof(WorldItem *));
	for (size_t i = 0; i < gBuckets_len; i++) {  // make dummy nodes
		gBuckets[i] = nnmalloc(sizeof(WorldItem));
		memset(gBuckets[i], 0xe4, sizeof(WorldItem));
		gBuckets[i]->next = NULL;
	}
}

static bool loadLevel(const char *const level_filename) {
	for (size_t i = 0; i < gBuckets_len; i++) {
		freeLinkedList(gBuckets[i]);  // (free the dummy nodes too)
	}
	free(gBuckets);
	//gBuckets_len = 0;
	gScrollOffset = 0;
	
	// load the new level
	lrFailCleanup(NULL, &lvl);
	lvl = levelReader(level_filename);
	if (!lvl.hdr)
		return false;
	stlPrinter(&lvl);
	
	initBuckets();
	player = worldItem_new(STL_PLAYER, lvl.start_pos_x, lvl.start_pos_y,
		TILE_WIDTH / 3 * 2, TILE_HEIGHT - 2, 0, 1, true, fnpl, false,
		gObjTextureNames[STL_TUX_LEFT], gObjTextureNames[STL_TUX_RIGHT]);
	addToBuckets(player);
	
	loadLevelObjects();
	loadLevelInteractives();
	
	return true;
}

// Populate the gObjTextureNames array with textures (that last the whole game).
static bool populateGOTN() {
	static bool ran = false;
	assert(!ran);
	ran = true;
	
	glGenTextures(gOTNlen, gObjTextureNames);
	
	initGLTextureNam(gObjTextureNames[STL_TUX_LEFT], "textures/tux.data",
		false, true);
	initGLTextureNam(gObjTextureNames[STL_TUX_RIGHT], "textures/tux.data",
		true, true);
	
	initGLTextureNam(gObjTextureNames[STL_ICEBLOCK_TEXTURE_LEFT],
		"textures/mriceblock.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_ICEBLOCK_TEXTURE_RIGHT],
		"textures/mriceblock.data", true, true);
	
	initGLTextureNam(gObjTextureNames[STL_DEAD_MRICEBLOCK_TEXTURE_LEFT],
		"textures/mriceblock-flat-left.data", false, true);
	initGLTextureNam(gObjTextureNames[STL_DEAD_MRICEBLOCK_TEXTURE_RIGHT],
		"textures/mriceblock-flat-left.data", true, true);
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
	
	if (strlen(filename) == 0) {
		gTextureNames[256] = gTextureNames[0];
		return true;
	}
	
	// investigate taking off the filename extension
	if (strlen(filename) > 4 &&
		(0 == strcmp(".jpg", filename + strlen(filename) - 4) ||
		0 == strcmp(".png", filename + strlen(filename) - 4))) {
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

static uint32_t alphatiles[256];

static void initialize_alphatile(char ch) {
	const char *const prefix = "textures/alphabet/";
	char *str = nnmalloc(strlen(prefix) + strlen("X.data") + 1);
	strcpy(str, prefix);
	str[strlen(prefix)] = ch;
	str[strlen(prefix) + 1] = '\0';  // for strcat
	strcat(str, ".data");
	initGLTextureNam(alphatiles[(int)ch], str, false, true);
	free(str);
}

static void initialize_alphatiles(void) {
	glGenTextures(256, alphatiles);
	
	for (char ch = 'a'; ch <= 'z'; ch++) {
		initialize_alphatile(ch);
	}
	for (char ch = '0'; ch <= '9'; ch++) {
		initialize_alphatile(ch);
	}
	initGLTextureNam(alphatiles[255], "textures/alphabet/red.data", false,
		true);
	initGLTextureNam(alphatiles[254], "textures/alphabet/green.data", false,
		true);
	
	assert(glGetError() == GL_NO_ERROR);
}

static void initialize(void) {
	initialize_prgm();
	maybeInitgTextureNames();
	
	assert(populateGOTN());
	assert(loadLevel("gpl/levels/level24.stl"));  // xxx
	gCurrLevel = 24;  // hack for debugging xxx
	
	assert(loadLevelBackground());
	
	initialize_alphatiles();
}

// Return true if w is off-screen.
static bool isOffscreen(const WorldItem *const w) {
	return (topOf(w) > SCREEN_HEIGHT || bottomOf(w) < 0 ||
		leftOf(w) > SCREEN_WIDTH || rightOf(w) < 0);
}

static void drawGLvertices(const float *const vertices, const uint32_t texnam) {
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices);
	glEnableVertexAttribArray(0);
	glBindAttribLocation(prgm, 0, "vertices");
	
	static const float tcoords[] = {
		0.0001, 0.0001,
		0.0001, 0.9999,
		0.9999, 0.0001,
		0.9999, 0.9999,
	};
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, tcoords);
	glEnableVertexAttribArray(1);
	glBindAttribLocation(prgm, 1, "tcoords");
	assert(glGetError() == GL_NO_ERROR);

	glBindTexture(GL_TEXTURE_2D, texnam);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	must(glGetError() == GL_NO_ERROR);
}

static void drawWorldItems() {
	for (size_t i = 0; i < gBuckets_len; i++)
		for (const WorldItem *w = gBuckets[i]->next; w; w = w->next) {
			if (isOffscreen(w) || w->texnam == 0)
				continue;
			
			const float vertices[] = {
				0 + w->x,			0 + w->y,			1.0,
				0 + w->x,			w->y + w->height,	1.0,
				w->x + w->width,	0 + w->y,			1.0,
				w->x + w->width,	w->y + w->height,	1.0,
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

static char *buildLevelString() {
	const char *const prefix = "gpl/levels/level";
	char *file = malloc(strlen(prefix) + intAsStrLen(gCurrLevel) +
		strlen(".stl") + 1);
	strcpy(file, prefix);
	sprintf(file + strlen(prefix), "%d", gCurrLevel);
	file[strlen(prefix) + intAsStrLen(gCurrLevel)] = '\0';
	strcat(file, ".stl");
	fprintf(stderr, "DEBUG: loading %s\n", file);
	return file;
}

static void reloadLevel(keys *const k, bool ignoreCheckpoints) {
	assert(k);
	if (gCurrLevel > 24)
		gCurrLevel = 1;  // hack
	
	point rp;
	if (!ignoreCheckpoints)
		rp = selectResetPoint();
	
	char *filename = buildLevelString();
	assert(loadLevel(filename));
	free(filename);
	
	assert(loadLevelBackground());
	
	if (!ignoreCheckpoints && rp.x != -1 && rp.y != -1) {
		if (rp.x - SCREEN_WIDTH / 3 > 0)
			scrollTheScreen(rp.x - SCREEN_WIDTH / 3);
		setX(player, SCREEN_WIDTH / 3);
		cleanupWorldItems();
		player->y = rp.y;
	}
	
	gPlayerLastDirectionWasRight = true;  // player starts facing right
}

static void verifyBuckets() {
	for (size_t i = 0; i < gBuckets_len; i++)
		for (WorldItem *w = gBuckets[i]->next; w; w = w->next)
			assert(w->x / BUCKETS_SIZE == (ssize_t)i);
}

static bool displayingMessage = false;

// Return the number of ch encountered in msg. todo move to util.c
static size_t count(const char *msg, const char ch) {
	size_t n = 0;
	while (*msg)
		if (*msg++ == ch)
			n++;
	return n;
}

// Given a msg of lines, return the length of the longest one. todo move to util.c
static size_t longestLine(const char *msg) {
	size_t longest = 0;
	size_t current = 0;
	for (; *msg; msg++) {
		if (*msg == '\n') {
			if (current > longest)
				longest = current;
			current = 0;
		} else
			current++;
	}
	if (current > longest)
		longest = current;
	return longest;
}

static void displayMessage(const char *msg, uint32_t backgroundID) {
	size_t msg_width = longestLine(msg) * TILE_WIDTH / 2;
	size_t msg_height = (count(msg, '\n') + 1) * TILE_HEIGHT / 2;
	
	size_t xpos = 0;
	if (msg_width < (size_t)SCREEN_WIDTH)
		xpos = (SCREEN_WIDTH - msg_width) / 2;
	size_t ypos = 0;
	if (msg_height < (size_t)SCREEN_HEIGHT)
		ypos = (SCREEN_HEIGHT - msg_height) / 2;
	size_t xpos_orig = xpos;  //, ypos_orig = ypos;
	
	for (; *msg; msg++) {
		const float vertices[] = {
			xpos,				ypos,				1,
			xpos,				ypos + TILE_HEIGHT / 2,	1,
			xpos + TILE_WIDTH / 2,	ypos,				1,
			xpos + TILE_WIDTH / 2,	ypos + TILE_HEIGHT / 2,	1,
		};
		if ((*msg >= 'a' && *msg <= 'z') || (*msg >= '0' && *msg <= '9')) {
			drawGLvertices(vertices, alphatiles[backgroundID]);
			drawGLvertices(vertices, alphatiles[(int)*msg]);
			xpos += TILE_WIDTH / 2;
		} else if (*msg == ' ') {
			drawGLvertices(vertices, alphatiles[backgroundID]);
			xpos += TILE_WIDTH / 2;
		} else if (*msg == '\n') {
			xpos = xpos_orig;
			ypos += TILE_HEIGHT / 2;
		} else
			fprintf(stderr, "DEBUG: unimplemented alphatile character '%c'\n",
				*msg);
	}
}

static void displayDeathMessage() {
	displayMessage("you died\nplease press enter", 255);
}

static void displayPassMessage() {
	char *prefix = "level complete\n";
	char *suffix = " deaths\nplease press enter";
	char *msg = nnmalloc(strlen(prefix) + intAsStrLen(gNDeaths) +
		strlen(suffix) + 1);
	strcpy(msg, prefix);
	sprintf(msg + strlen(prefix), "%d", gNDeaths);
	msg[strlen(prefix) + intAsStrLen(gNDeaths)] = '\0';
	strcat(msg, suffix);
	
	displayMessage(msg, 254);
	
	free(msg);
}

static void core(keys *const k, bool runPhysics) {
	static bool initialized = false;
	if (!initialized) {
		initialize();
		initialized = true;
	}
	
	verifyBuckets();
	
	if (runPhysics) {
		processInput(k);
		applyFrame();
		cleanupWorldItems();  // prevent items with negative x from building up
		applyGravity();
	}
	
	clearScreen();
	paintTM(lvl.backgroundtm);
	paintTM(lvl.interactivetm);
	paintTM(lvl.foregroundtm);
	drawWorldItems();
	
	if (player->type == STL_PLAYER_DEAD) {  // reload the current level
		displayDeathMessage();
		displayingMessage = true;
		if (k->keyEnter) {
			displayingMessage = false;
			if (gNDeaths < 99999)
				gNDeaths++;
			reloadLevel(k, false);
		}
	} else if (player->type == STL_PLAYER_ASCENDED) {  // reload the next level
		displayPassMessage();
		displayingMessage = true;
		if (k->keyEnter) {
			displayingMessage = false;
			gCurrLevel++;
			gNDeaths = 0;
			reloadLevel(k, true);
		}
	}
}

const int32_t NSONE = 1000000000LL;  // nanoseconds in 1 second ( = 1 billion)

// Like strcmp(), but for struct timespec.
int tscmp(const struct timespec *const t1, const struct timespec *const t2) {
	if (t1->tv_sec != t2->tv_sec)
		return (int)t1->tv_sec - (int)t2->tv_sec;
	else
		return t1->tv_nsec - t2->tv_nsec;
}

// Increment t1 by ns.
void tsadd(struct timespec *const t1, int32_t ns) {
	assert(ns >= 0 && ns < NSONE);
	if (t1->tv_nsec > NSONE - 1 - ns) {
		t1->tv_sec++;
		t1->tv_nsec += ns - NSONE;
	} else
		t1->tv_nsec += ns;
	assert(t1->tv_nsec > 0 && t1->tv_nsec < NSONE && t1->tv_sec >= 0);
}

// Entry point for initgl.
bool draw(keys *const k) {
	static struct timespec then = { 0 }, now = { 0 };
	assert(TIME_UTC == timespec_get(&now, TIME_UTC));
	if (then.tv_sec == 0) {
		then = now;
		while (0 == tscmp(&then, &now)) {
			fprintf(stderr, "DEBUG: bootstraping timer ...\n");
			assert(TIME_UTC == timespec_get(&now, TIME_UTC));
		}
		srand((uint32_t)now.tv_nsec);
	}
	
	bool physicsRanTimes = 0;
	while (tscmp(&then, &now) < 0) {
		core(k, true && !displayingMessage);
		tsadd(&then, NSONE / 60);
		physicsRanTimes++;
		
		if (physicsRanTimes >= 5) {  // system is too slow
			then = now;
			break;
		}
		
		// prevent bouncing between drawing a dummy frame and running twice
		if (rand() > (RAND_MAX / 3 * 2)) {  // randomly skip [2nd,+inf) draws
			if (physicsRanTimes > 1)
				fprintf(stderr, "DEBUG: randomly skipping a physics run\n");
			break;
		}
	}
	if (physicsRanTimes == 0) {
		fprintf(stderr, "DEBUG: physics ran 0 times (so drawing dummy frame)\n");
		core(k, false);
	} else if (physicsRanTimes > 1)
		fprintf(stderr, "DEBUG: physics ran %d times\n", physicsRanTimes);
	
	return true;
}


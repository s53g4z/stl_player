// levelreader.c

#include "stlplayer.h"
#include "util.h"

static const char *nextSectionFrom(const char *ptr, const char *const end, 
	size_t *const sect_len) {
	*sect_len = 0;
	const char *start;
	
	while (ptr != end && *ptr != '(')
		ptr++;
	if (ptr == end)
		return NULL;  // no sect found
	
	start = ptr++;
	
	// until the next paren
	while (ptr != end && *ptr != '(' && *ptr != ')')
		ptr++;
	if (ptr == end)
		return NULL;  // malformed?
	
	*sect_len = ptr - start;
	return start;
}

// levelReader failure cleanup. Returns with lvl->hdr = false.
stl lrFailCleanup(const char *const level_orig, stl *lvl) {
	free((char *)level_orig);
	
	free(lvl->author);
	free(lvl->name);
	free(lvl->background);
	free(lvl->music);
	free(lvl->particle_system);
	free(lvl->theme);
	for (int h = 0; h < lvl->height; h++) {
		free(lvl->interactivetm[h]);
		free(lvl->backgroundtm[h]);
		free(lvl->foregroundtm[h]);
	}
	free(lvl->interactivetm);
	free(lvl->backgroundtm);
	free(lvl->foregroundtm);
	free(lvl->objects);
	memset(lvl, 0xe5, sizeof(*lvl));
	
	lvl->hdr = false;
	return *lvl;
}

void stlPrinter(const stl *const lvl) {
	fprintf(stderr, "hdr: %s\n", lvl->hdr ? "true" : "false");
	fprintf(stderr, "version: %d\n", lvl->version);
	fprintf(stderr, "author: %s\n", lvl->author);
	fprintf(stderr, "name (of the level): %s\n", lvl->name);
	fprintf(stderr, "width: %d\n", lvl->width);
	fprintf(stderr, "height: %d\n", lvl->height);
	fprintf(stderr, "start_pos: %d, %d\n", lvl->start_pos_x, lvl->start_pos_y);
	fprintf(stderr, "background: %s\n", lvl->background);
	fprintf(stderr, "music: %s\n", lvl->music);
	fprintf(stderr, "time: %d\n", lvl->time);
	fprintf(stderr, "gravity: %d\n", lvl->gravity);
	fprintf(stderr, "particle_system: %s\n", lvl->particle_system);
	fprintf(stderr, "theme: %s\n", lvl->theme);
	fprintf(stderr, "objects {\n");
	for (size_t i = 0; i < lvl->objects_len; i++) {
		char *str = "u n k n o w n ";
		int type = lvl->objects[i].type;
		switch(type) {
			case STL_INVALID_OBJ: str = "STL_INVALID_OBJ"; break;
			case STL_NO_MORE_OBJ: str = "STL_NO_MORE_OBJ"; break;
			case SNOWBALL: str = "SNOWBALL"; break;
			case MRICEBLOCK: str = "MRICEBLOCK"; break;
			case STL_BOMB: str = "MRBOMB"; break;
			case STALACTITE: str = "STALACTITE"; break;
			case BOUNCINGSNOWBALL: str = "BOUNCINGSNOWBALL"; break;
			case FLYINGSNOWBALL: str = "FLYINGSNOWBALL"; break;
			case MONEY: str = "MONEY"; break;
		}
		fprintf(stderr, "\t%s at %d, %d\n", str, lvl->objects[i].x,
			lvl->objects[i].y);
	}
	fprintf(stderr, "}\n");
	fprintf(stderr, "objects_len/cap: %lu/%lu\n", lvl->objects_len,
		lvl->objects_cap);
	
	if (lvl->height != 15)
		fprintf(stderr, "WARN: lvl.height unexpected (%d)\n", lvl->height);
}

stl levelReader(const char *const filename) {
	stl lvl = { 0 };
	
	ssize_t level_len_ss;
	char *file = safe_read(filename, &level_len_ss);
	if (!file || level_len_ss < 1) {
		lvl.hdr = false;
		return lvl;
	}
	file = nnrealloc(file, level_len_ss + 1);  // for *scanf()
	file[level_len_ss] = '\0';  // ibid
	const char *level = file;
	file = NULL;  // dont use again
	const char *const level_orig = level;
	size_t level_len = level_len_ss;
	
	const char *section = NULL;  // you have the level, now get a section
	size_t section_len;
	while ((section = nextSectionFrom(level, level + level_len, &section_len))) {
		const char *const section_orig = section;
		const size_t section_len_orig = section_len;
		trimWhitespace(&section, &section_len);
		
		if (!lvl.hdr) {
			if (*section++ != '(')
				return lrFailCleanup(level_orig, &lvl);
			else
				section_len--;
			trimWhitespace(&section, &section_len);
			const char *const su_level_str = "supertux-level";
			if (section_len >= strlen(su_level_str) &&
				0 == strncmp(section, su_level_str, strlen(su_level_str))) {
				lvl.hdr = true;
			} else
				return lrFailCleanup(level_orig, &lvl);
		} else {
			if (*section++ != '(')
				return lrFailCleanup(level_orig, &lvl);
			else
				section_len--;
			trimWhitespace(&section, &section_len);  // optional for well-formed??

			if (nextWordIs("version", &section, &section_len)) {
				if (1 != sscanf(section, "%d", &lvl.version) || lvl.version != 1)
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("author", &section, &section_len)) {
				if (!writeStrTo(&lvl.author, &section, &section_len))
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("name", &section, &section_len)) {
				if (!writeStrTo(&lvl.name, &section, &section_len))
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("width", &section, &section_len)) {
				if (1 != sscanf(section, "%d", &lvl.width) || lvl.width < 0)
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("height", &section, &section_len)) {
				if (1 != sscanf(section, "%d", &lvl.height) || lvl.height < 0)
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("start_pos_x", &section, &section_len)) {
				if (1 != sscanf(section, "%d", &lvl.start_pos_x) ||
					lvl.start_pos_x < 0)
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("start_pos_y", &section, &section_len)) {
				if (1 != sscanf(section, "%d", &lvl.start_pos_y) ||
					lvl.start_pos_y < 0)
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("background", &section, &section_len)) {
				if (!writeStrTo(&lvl.background, &section, &section_len))
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("music", &section, &section_len)) {
				if (!writeStrTo(&lvl.music, &section, &section_len))
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("time", &section, &section_len)) {
				if (1 != sscanf(section, "%d", &lvl.time) || lvl.time < 0)
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("gravity", &section, &section_len)) {
				if (1 != sscanf(section, "%d", &lvl.gravity) || lvl.gravity < 0)
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("particle_system", &section, &section_len)) {
				if (!writeStrTo(&lvl.particle_system, &section, &section_len))
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("theme", &section, &section_len)) {
				if (!writeStrTo(&lvl.theme, &section, &section_len))
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("interactive-tm", &section, &section_len)) {
				if (!parseTM(&lvl.interactivetm, lvl.width, lvl.height,
					&section, &section_len))
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("background-tm", &section, &section_len)) {
				if (!parseTM(&lvl.backgroundtm, lvl.width, lvl.height,
					&section, &section_len))
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("foreground-tm", &section, &section_len)) {
				if (!parseTM(&lvl.foregroundtm, lvl.width, lvl.height,
					&section, &section_len))
					return lrFailCleanup(level_orig, &lvl);
			} else if (nextWordIs("objects", &section, &section_len)) {
				// note: objects[_len|_cap]
				if (lvl.objects_cap == 0)
					init_lvl_objects(&lvl);
				for (;;) {
					stl_obj obj = getSTLobj(&section, &section_len);
					if (obj.type == STL_NO_MORE_OBJ)
						break;
					else if (obj.type == STL_INVALID_OBJ)
						return lrFailCleanup(level_orig, &lvl);
					pushto_lvl_objects(&lvl, &obj);
				}
			}
		}
		
		level_len = level_len - (section_orig + section_len_orig - level);
		level = section_orig + section_len_orig;
	}
	
	free((char *)level_orig);
	return lvl;
}

// Parse past a '(' in search of an stl_obj. Return true on success, else marks
// the object as invalid and returns false.
bool parse_paren(stl_obj *const obj,
	const char **section, size_t *const section_len) {
	(*section_len)--;
	if (*(*section)++ == '(')
		return true;
	obj->type = STL_INVALID_OBJ;
	return false;
}

// Helper for get_stl_obj. Scans for a ([x|y] [0-9]+) and returns true or else
// marks the object as invalid and returns false.
bool scanForObjComponent(stl_obj *const obj, const char *x_or_y,
	const char **section, size_t *const section_len) {
	assert(*x_or_y == 'x' || *x_or_y == 'y');
	
	if (!parse_paren(obj, section, section_len))
		return false;
	int n;
	if (!nextWordIs(x_or_y, section, section_len) ||
		1 != sscanf(*section, "%d", &n)) {
		obj->type = STL_INVALID_OBJ;
		return false;
	}
	if (n < 0)
		n = 0;
	if (*x_or_y == 'x')
		obj->x = n;
	else if (*x_or_y == 'y')
		obj->y = n;
	int nAsStrLen = intAsStrLen(n);
	*section += nAsStrLen;
	*section_len -= nAsStrLen;
	(*section_len)--;
	if (*(*section)++ != ')') {
		obj->type = STL_INVALID_OBJ;
		return false;
	}
	return true;
}

// Helper for getSTLobj. On success, true is returned and *section is moved
// past the rest of the object.
bool consumeRestOfObj(stl_obj *const obj, const enum stl_obj_type type,
	const char **section, size_t *const section_len) {
	obj->type = type;
	if (!scanForObjComponent(obj, "x", section, section_len))
		return false;
	trimWhitespace(section, section_len);
	if (!scanForObjComponent(obj, "y", section, section_len))
		return false;
	trimWhitespace(section, section_len);
	(*section_len)--;
	if (*(*section)++ != ')') {
		obj->type = STL_INVALID_OBJ;
		return false;
	}
	return true;
}

// Return a populated stl_obj. If a well-formed object is found, *section is 
// moved past the object.
stl_obj getSTLobj(const char **section, size_t *const section_len) {
	stl_obj obj = { 0 };
	obj.type = STL_INVALID_OBJ;  // type is not known yet
	
	trimWhitespace(section, section_len);
	if (**section == ')') {
		obj.type = STL_NO_MORE_OBJ;
		return obj;
	}
	if (!parse_paren(&obj, section, section_len))
		return obj;
	if (nextWordIs("snowball", section, section_len)) {
		consumeRestOfObj(&obj, SNOWBALL, section, section_len);
	} else if (nextWordIs("mriceblock", section, section_len)) {
		consumeRestOfObj(&obj, MRICEBLOCK, section, section_len);
	} else if (nextWordIs("mrbomb", section, section_len)) {
		consumeRestOfObj(&obj, STL_BOMB, section, section_len);
	} else if (nextWordIs("stalactite", section, section_len)) {
		consumeRestOfObj(&obj, STALACTITE, section, section_len);
	} else if (nextWordIs("bouncingsnowball", section, section_len)) {
		consumeRestOfObj(&obj, BOUNCINGSNOWBALL, section, section_len);
	} else if (nextWordIs("flyingsnowball", section, section_len)) {
		consumeRestOfObj(&obj, FLYINGSNOWBALL, section, section_len);
	} else if (nextWordIs("money", section, section_len)) {
		consumeRestOfObj(&obj, MONEY, section, section_len);
	}
	return obj;
}

// Parse a TM using *section to fill *ptm with char values.
bool parseTM(uint8_t ***ptm, const int width, const int height,
	const char **section, size_t *const section_len) {
	*ptm = nnmalloc(height * sizeof(uint8_t *));
	for (int h = 0; h < height; h++) {
		(*ptm)[h] = nnmalloc(width * sizeof(uint8_t));
		memset((*ptm)[h], 0x00, width * sizeof(uint8_t));
	}
	for (int h = 0; h < height; h++)
		for (int w = 0; w < width; w++) {
			trimWhitespace(section, section_len);
			int ch;
			if (1 != sscanf(*section, "%u", &ch) || ch > 255)
				return false;
			(*ptm)[h][w] = (uint8_t)ch;
			
			int lengthOfchAsAStr = 1;
			if (ch > 99)
				lengthOfchAsAStr = 3;
			else if (ch > 9)
				lengthOfchAsAStr = 2;
			*section_len -= lengthOfchAsAStr;
			
			*section += lengthOfchAsAStr;
		}
	//printTM(*ptm, width, height);
	return true;
}

// levelReader helper. Read a word from *section and write it to *destination.
bool writeStrTo(char **const destination, const char **section,
	size_t *const section_len) {
	if (*(*section)++ != '"')
		return false;
	else
		(*section_len)--;
	size_t dest_len = 0;
	while (*section_len > 0 && **section != '"') {
		dest_len++;
		(*section)++;
		(*section_len)--;
	}
	if (*section_len == 0)
		return false;
	*destination = nnmalloc(dest_len + 1);
	strncpy(*destination, *section - dest_len,
		dest_len);
	(*destination)[dest_len] = '\0';
	if (*(*section)++ != '"')
		return false;
	trimWhitespace(section, section_len);
	if (*(*section) != ')')
		return false;
	return true;
}

// levelReader helper. Move section past the word, and trim any whitespace.
// Return true on success. Parse failure leaves section[_len] unmodified.
bool nextWordIs(const char *const word, const char **section,
	size_t *const section_len) {
	if (*section_len < strlen(word) ||
		0 != strncmp(*section, word, strlen(word)) ||
		!isWhitespace(*(*section + strlen(word))))
		return false;
	
	*section += strlen(word);
	*section_len -= strlen(word);
	trimWhitespace(section, section_len);
	return true;
}

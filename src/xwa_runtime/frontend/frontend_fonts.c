#include "xwa_runtime/frontend/frontend_fonts.h"

#include "aeron/aeron.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "xwa/frontend/frontend_text.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XWA_FONTS_PATH "frontend/fonts/fonts.yaml"
#define XWA_FONTS_DIR "frontend/fonts"
#define XWA_FONTS_MAX_FILES 8
#define XWA_FONTS_MAX_FILE_SIZE (64u * 1024u * 1024u)

typedef struct XwaFontSettings {
	char file[128];
	int metrics_font;
	float scale, spacing, baseline;
} XwaFontSettings;

typedef struct XwaFontSize {
	XwaFontSettings settings;
	int face; /* in g_fontFaces */
	int missing_logged;
	/* Advances in 1/256 units with metrics: font, worked out on first use.
	   INT32_MIN means not worked out yet, and -1 that the font has no glyph. */
	int32_t advance256[256];
	XwaFrontendFont font;
} XwaFontSize;

/* Windows-1252 bytes 0x80 to 0x9F. The bytes it leaves undefined keep their
   value, which no font draws. */
static const uint16_t k_cp1252[32] = {
	0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
	0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
	0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
};

static uint32_t g_fontCodepoints[256];
static AeronFontFace* g_fontFaces[XWA_FONTS_MAX_FILES];
static char g_fontFaceFiles[XWA_FONTS_MAX_FILES][128];
static int g_fontFaceCount;
static XwaFontSize* g_fontSizes[256]; /* by font size, NULL when the file doesn't set it */
static int g_fontsLoaded;

/* ---- the file ---------------------------------------------------------- */

static int fonts_error(const AeronConfigNode* node, const char* format, ...) {
	char message[256];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	Aeron_LogError("xwa.frontend", "%s:%d: %s", XWA_FONTS_PATH, AeronConfigNode_Line(node), message);
	return 0;
}

static int fonts_check_keys(const AeronConfigNode* node, const char* const* keys, const char* what) {
	for (size_t i = 0; i < AeronConfigNode_MapCount(node); i++) {
		const char* key = AeronConfigNode_MapKeyAt(node, i);
		int k = 0;
		while (keys[k] && strcmp(keys[k], key) != 0) {
			k++;
		}
		if (!keys[k]) {
			return fonts_error(AeronConfigNode_MapValueAt(node, i), "unknown key '%s' in %s", key, what);
		}
	}
	return 1;
}

static int fonts_number(const AeronConfigNode* node, const char* key, float min, float max, float* out) {
	const AeronConfigNode* value = AeronConfigNode_MapGet(node, key);
	const AeronConfigNodeType type = AeronConfigNode_Type(value);
	float number;

	if (!value) {
		return 1;
	}
	if (type != AERON_CONFIG_INT && type != AERON_CONFIG_FLOAT) {
		return fonts_error(value, "'%s' must be a number", key);
	}
	number = (float)AeronConfigNode_Float(value, 0.0);
	if (!(number >= min && number <= max)) {
		return fonts_error(value, "'%s' must be from %g to %g", key, min, max);
	}
	*out = number;
	return 1;
}

/* Reads the keys of `node` over `settings`, which holds the default values. */
static int fonts_read_settings(const AeronConfigNode* node, XwaFontSettings* settings, const char* what) {
	static const char* const keys[] = { "file", "metrics", "scale", "spacing", "baseline", NULL };
	const AeronConfigNode* file = AeronConfigNode_MapGet(node, "file");
	const AeronConfigNode* metrics = AeronConfigNode_MapGet(node, "metrics");

	if (AeronConfigNode_Type(node) != AERON_CONFIG_MAP) {
		return fonts_error(node, "%s must be a map", what);
	}
	if (!fonts_check_keys(node, keys, what)) {
		return 0;
	}
	if (file) {
		const char* name = AeronConfigNode_String(file, NULL);
		if (AeronConfigNode_Type(file) != AERON_CONFIG_STRING || !name || !name[0]) {
			return fonts_error(file, "'file' must be a file name");
		}
		if (strlen(name) >= sizeof(settings->file)) {
			return fonts_error(file, "'file' is too long");
		}
		strcpy(settings->file, name);
	}
	if (metrics) {
		const char* mode = AeronConfigNode_String(metrics, "");
		if (AeronConfigNode_Type(metrics) != AERON_CONFIG_STRING ||
			(strcmp(mode, "game") != 0 && strcmp(mode, "font") != 0)) {
			return fonts_error(metrics, "'metrics' must be game or font");
		}
		settings->metrics_font = strcmp(mode, "font") == 0;
	}
	return fonts_number(node, "scale", 0.1f, 10.0f, &settings->scale) &&
		   fonts_number(node, "spacing", -1.0f, 1.0f, &settings->spacing) &&
		   fonts_number(node, "baseline", -2.0f, 2.0f, &settings->baseline);
}

/* Returns the index of the face for `file`, loading it if needed. */
static int fonts_face(const AeronConfigNode* node, const char* file) {
	char path[192];
	AeronFontFace* face;

	for (int i = 0; i < g_fontFaceCount; i++) {
		if (strcmp(g_fontFaceFiles[i], file) == 0) {
			return i;
		}
	}
	if (g_fontFaceCount >= XWA_FONTS_MAX_FILES) {
		fonts_error(node, "more than %d font files", XWA_FONTS_MAX_FILES);
		return -1;
	}
	snprintf(path, sizeof(path), "%s/%s", XWA_FONTS_DIR, file);
	face = AeronFontFace_LoadVfs(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, path, XWA_FONTS_MAX_FILE_SIZE);
	if (!face) {
		fonts_error(node, "%s can't be read as a font", path);
		return -1;
	}
	g_fontFaces[g_fontFaceCount] = face;
	strcpy(g_fontFaceFiles[g_fontFaceCount], file);
	return g_fontFaceCount++;
}

static int fonts_add_size(const AeronConfigNode* node, int font_size, const XwaFontSettings* settings) {
	XwaFontSize* size;
	int face;

	if (!settings->file[0]) {
		return fonts_error(node, "font size %d has no 'file'", font_size);
	}
	face = fonts_face(node, settings->file);
	if (face < 0) {
		return 0;
	}
	size = (XwaFontSize*)calloc(1, sizeof(*size));
	if (!size) {
		return 0;
	}
	size->settings = *settings;
	size->face = face;
	for (int i = 0; i < 256; i++) {
		size->advance256[i] = INT32_MIN;
	}
	g_fontSizes[font_size] = size;
	return 1;
}

static int fonts_read(const AeronConfigNode* root) {
	static const char* const keys[] = { "version", "default", "sizes", NULL };
	const AeronConfigNode* version = AeronConfigNode_MapGet(root, "version");
	const AeronConfigNode* defaults = AeronConfigNode_MapGet(root, "default");
	const AeronConfigNode* sizes = AeronConfigNode_MapGet(root, "sizes");
	XwaFontSettings base;

	if (AeronConfigNode_Type(root) != AERON_CONFIG_MAP) {
		return fonts_error(root, "the file must be a map");
	}
	if (!fonts_check_keys(root, keys, "the file")) {
		return 0;
	}
	if (version &&
		(AeronConfigNode_Type(version) != AERON_CONFIG_INT || AeronConfigNode_Int(version, 0) != 1)) {
		return fonts_error(version, "'version' must be 1");
	}
	memset(&base, 0, sizeof(base));
	base.scale = 1.0f;
	if (defaults && !fonts_read_settings(defaults, &base, "'default'")) {
		return 0;
	}
	if (sizes && AeronConfigNode_Type(sizes) != AERON_CONFIG_MAP) {
		return fonts_error(sizes, "'sizes' must be a map");
	}
	for (size_t i = 0; sizes && i < AeronConfigNode_MapCount(sizes); i++) {
		const char* key = AeronConfigNode_MapKeyAt(sizes, i);
		const AeronConfigNode* value = AeronConfigNode_MapValueAt(sizes, i);
		XwaFontSettings settings = base;
		char* end;
		long font_size;

		errno = 0;
		font_size = strtol(key, &end, 10);
		if (errno || end == key || *end || font_size < 1 || font_size > 255) {
			return fonts_error(value, "'%s' isn't a font size from 1 to 255", key);
		}
		if (!fonts_read_settings(value, &settings, "a font size") ||
			!fonts_add_size(value, (int)font_size, &settings)) {
			return 0;
		}
	}
	if (base.file[0]) {
		for (int font_size = 1; font_size < 256; font_size++) {
			if (!g_fontSizes[font_size] && !fonts_add_size(defaults, font_size, &base)) {
				return 0;
			}
		}
	}
	return 1;
}

static void fonts_free(void) {
	for (int i = 0; i < 256; i++) {
		free(g_fontSizes[i]);
		g_fontSizes[i] = NULL;
	}
	for (int i = 0; i < g_fontFaceCount; i++) {
		AeronFontFace_Release(g_fontFaces[i]);
		g_fontFaces[i] = NULL;
	}
	g_fontFaceCount = 0;
}

static void fonts_load(void) {
	AeronConfigFile* file = NULL;
	AeronConfigError error;

	if (g_fontsLoaded) {
		return;
	}
	g_fontsLoaded = 1;
	for (int i = 0; i < 256; i++) {
		g_fontCodepoints[i] = i >= 0x80 && i < 0xA0 ? k_cp1252[i - 0x80] : (uint32_t)i;
	}
	memset(&error, 0, sizeof(error));
	if (!AeronConfigFile_LoadYamlEx(Aeron_GetVfs(), AERON_VFS_ROOT_MOD, XWA_FONTS_PATH, &file, &error)) {
		if (error.code != AERON_CONFIG_ERROR_NOT_FOUND) {
			Aeron_LogError("xwa.frontend", "%s:%d: %s", XWA_FONTS_PATH, error.line,
						   error.message[0] ? error.message : "cannot be read");
			Aeron_LogError("xwa.frontend", "%s is ignored", XWA_FONTS_PATH);
		}
		return;
	}
	if (!fonts_read(AeronConfigFile_Root(file))) {
		Aeron_LogError("xwa.frontend", "%s is ignored", XWA_FONTS_PATH);
		fonts_free();
	} else {
		Aeron_LogInfo("xwa.frontend", "%s: %d font files", XWA_FONTS_PATH, g_fontFaceCount);
	}
	AeronConfigFile_Destroy(file);
}

static XwaFontSize* fonts_size(int font_size) {
	fonts_load();
	return font_size >= 0 && font_size < 256 ? g_fontSizes[font_size] : NULL;
}

/* ---- metrics ----------------------------------------------------------- */

static float fonts_em_units(const XwaFontSize* size, int font_size) {
	return (float)font_size * size->settings.scale;
}

int XwaFrontendFonts_Advance256(int font_size, unsigned char ch, int classic) {
	XwaFontSize* size = fonts_size(font_size);
	int32_t* advance;

	if (!size || !size->settings.metrics_font || ch < 32) {
		return classic * 256;
	}
	advance = &size->advance256[ch];
	if (*advance == INT32_MIN) {
		float em;
		if (AeronFontFace_Advance(g_fontFaces[size->face], g_fontCodepoints[ch], &em)) {
			*advance = (int32_t)lroundf((em + size->settings.spacing) * fonts_em_units(size, font_size) * 256.0f);
			if (*advance < 0) {
				*advance = 0;
			}
		} else {
			*advance = -1;
			if (!size->missing_logged) {
				size->missing_logged = 1;
				Aeron_LogWarn("xwa.frontend", "%s has no glyph for byte 0x%02X, font size %d",
							  size->settings.file, ch, font_size);
			}
		}
	}
	return *advance >= 0 ? *advance : classic * 256;
}

int XwaFrontendFonts_UsesFontMetrics(int font_size) {
	const XwaFontSize* size = fonts_size(font_size);
	return size && size->settings.metrics_font;
}

int XwaFrontendFonts_Height(int font_size, int classic) {
	const XwaFontSize* size = fonts_size(font_size);
	float ascent, descent;
	long height;

	if (!size || !size->settings.metrics_font) {
		return classic;
	}
	AeronFontFace_VMetrics(g_fontFaces[size->face], &ascent, &descent);
	height = lroundf((ascent + descent) * fonts_em_units(size, font_size));
	return height > 0 ? (int)height : 1;
}

void XwaFrontendFonts_AdvanceText(int* x, int* x_frac, int font_size, unsigned char ch, int classic, int scale256) {
	/* Without the font's metrics this is the original advance times
	   scale256, with no rounding. */
	const int64_t total =
		*x_frac + (XwaFrontendFonts_UsesFontMetrics(font_size)
					   ? ((int64_t)XwaFrontendFonts_Advance256(font_size, ch, classic) * scale256 + 128) / 256
					   : (int64_t)classic * scale256);
	const int64_t whole = total >= 0 ? total / 256 : -((255 - total) / 256);

	*x += (int)whole;
	*x_frac = (int)(total - whole * 256);
}

const XwaFrontendFont* XwaFrontendFonts_Get(int font_size) {
	XwaFontSize* size = fonts_size(font_size);
	const AeronFontFace* face;
	float ascent, descent;

	if (!size) {
		return NULL;
	}
	face = g_fontFaces[size->face];
	AeronFontFace_VMetrics(face, &ascent, &descent);
	size->font.face = face;
	size->font.em_units = fonts_em_units(size, font_size);
	/* The baseline splits the line height as the ascent and descent do. With
	   metrics: game this puts it where Windows put it in the original font. */
	size->font.baseline_units =
		(ascent + descent > 0.0f
			 ? (float)FrontendText_GetFontHeight(font_size) * ascent / (ascent + descent)
			 : 0.0f) +
		size->settings.baseline * size->font.em_units;
	size->font.codepoints = g_fontCodepoints;
	return &size->font;
}

void XwaFrontendFonts_Shutdown(void) {
	fonts_free();
	g_fontsLoaded = 0;
}

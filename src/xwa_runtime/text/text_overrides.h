#ifndef XWA_RUNTIME_TEXT_TEXT_OVERRIDES_H
#define XWA_RUNTIME_TEXT_TEXT_OVERRIDES_H

/* Lets a mod replace any text of the game by its ID, in text/strings.yaml in
 * the mod folder.
 *
 * Each line of the game's text files (STRINGS.TXT, FRONTTXT.TXT,
 * SHIPLIST.TXT, SPECDESC.TXT and others) starts with an ID between two '!',
 * such as "!STR_STATISTICS!Pilot Statistics". The game looks up each line
 * through Linez_ResolveString, which returns the mod's text for the ID when
 * this file has one:
 *
 *   strings:
 *     MSG_LASERS_ARMED: '\03Primary cannons armed'
 *     STR_STATISTICS: 'Pilot Terminal'
 *
 * Keys are the IDs, in upper or lower case. A value replaces the text after
 * the ID, exactly as the file would write it. Game escapes such as \03 stay
 * as they are, since YAML keeps backslashes in single quotes. The file is
 * UTF-8. The game uses Latin-1, so a text with a character that Latin-1
 * doesn't have is ignored. The mod's text replaces the text of xwa.tab.
 *
 * The game reads the file once at startup. Without the file, the game shows
 * its own text. */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Reads text/strings.yaml from the mod folder. */
void XwaText_Load(void);

/* Adds or replaces the text of `id`. `text` is Latin-1, like the game's text.
 * Returns 0 when `id` is empty or has a '!', or when out of memory. */
int XwaText_Set(const char* id, const char* text);

/* Returns the mod's text for `id`, or NULL. `id` can be in upper or lower
 * case and can end with '!', so a file line can be passed from the character
 * after its first '!'. The game may change the text, as it does with the
 * texts of xwa.tab, so the text isn't const. Texts are never freed, because
 * the game keeps pointers to them. */
char* XwaText_Find(const char* id);

/* Turns UTF-8 `text` into Latin-1, in place. Returns 0 when the text isn't
   valid UTF-8 or has a character that Latin-1 doesn't have. */
int XwaText_ToLatin1(char* text);

/* Returns the Unicode code point of a byte of the game's text. The game's
   files are Windows-1252, which is Latin-1 with more characters from 0x80
   to 0x9F. */
uint32_t XwaText_Codepoint(unsigned char byte);

/* Writes the game's text `text` to `out` in UTF-8, for SDL and the log. When
   `out` is too small, the text ends at the last character that fits. */
void XwaText_ToUtf8(const char* text, char* out, size_t outSize);

#ifdef __cplusplus
}
#endif

#endif /* XWA_RUNTIME_TEXT_TEXT_OVERRIDES_H */

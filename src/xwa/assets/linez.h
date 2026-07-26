#ifndef XWA_ASSETS_LINEZ_H
#define XWA_ASSETS_LINEZ_H

#ifdef __cplusplus
extern "C" {
#endif

extern char* g_linezTextBuffer;
extern char** g_linezDict;
extern int g_linezDictCount;

int Linez_LoadDict(char* fileName);
int Linez_CompareCachedEntries(const void* left, const void* right);
void Linez_FreeDict(void);
char* Linez_ResolveString(char* key);
int Linez_IsLoaded(void);

#ifdef __cplusplus
}
#endif

#endif

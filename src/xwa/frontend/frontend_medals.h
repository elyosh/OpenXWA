#ifndef XWA_FRONTEND_FRONTEND_MEDALS_H
#define XWA_FRONTEND_FRONTEND_MEDALS_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
	FRONTEND_MEDAL_VALUE_COUNT = 20,
};

extern int g_medalValues[FRONTEND_MEDAL_VALUE_COUNT];
extern int g_medalCount;

void FrontendMedals_LoadTable(void);

#ifdef __cplusplus
}
#endif

#endif

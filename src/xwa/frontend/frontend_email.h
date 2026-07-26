#ifndef XWA_FRONTEND_FRONTEND_EMAIL_H
#define XWA_FRONTEND_FRONTEND_EMAIL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FrontendEmailEntry {
	int field00;
	int emailIndex;
	char from[64];
	char subject[128];
	char body[1024];
} FrontendEmailEntry;

extern int g_frontendFamilyHasNewEmail;
extern int g_frontendEmailCount;
extern FrontendEmailEntry* g_frontendEmailEntries;

int FrontendEmail_LoadList(void);
int FrontendEmail_DrawInbox(void);

#ifdef __cplusplus
}
#endif

#endif

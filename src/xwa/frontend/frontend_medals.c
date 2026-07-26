#include "xwa/frontend/frontend_medals.h"

#include "xwa/assets/file_io.h"
#include "xwa/frontend/frontend_scratch.h"

#include <stdlib.h>
#ifndef XWA_MODERN
#include <stdio.h>
#endif

// GLOBAL: XWA 0x9F4AC0
int g_medalValues[FRONTEND_MEDAL_VALUE_COUNT];
// GLOBAL: XWA 0x9F4B1C
int g_medalCount;

// FLAGS: /O2 /G6
// FUNCTION: XWA 0x564280
void FrontendMedals_LoadTable(void) {
	XwaFile* stream;

	stream = File_Open(AERON_VFS_ROOT_ASSET, "frontres\\medals\\medals.txt", "r");
	g_medalCount = 0;
	if (stream != NULL) {
		int count;

		count = 0;
		while (1) {
#ifdef XWA_MODERN
			if (!File_ReadLine(stream, g_frontendScratchBuffer, 128)) {
				break;
			}
#else
			if (fgets(g_frontendScratchBuffer, 128, (FILE*)stream) == NULL) {
				break;
			}
#endif
			g_medalValues[count++] = atoi(g_frontendScratchBuffer);
			if (count >= FRONTEND_MEDAL_VALUE_COUNT) {
				break;
			}
		}

		g_medalCount = count;
		File_Close(stream);
	}
}

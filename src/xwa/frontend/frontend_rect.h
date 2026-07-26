#ifndef XWA_FRONTEND_FRONTEND_RECT_H
#define XWA_FRONTEND_FRONTEND_RECT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FrontendRect {
	int32_t left;
	int32_t top;
	int32_t right;
	int32_t bottom;
} FrontendRect;

#ifdef __cplusplus
}
#endif

#endif

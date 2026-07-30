#ifndef XWA_NET_DIRECTPLAY_PRIVATE_H
#define XWA_NET_DIRECTPLAY_PRIVATE_H

#include "xwa/net/net_types.h"
#include "xwa_runtime/compat/directx/dx_win_types.h"

typedef struct XwaDirectPlay4 XwaDirectPlay4;

typedef struct XwaDirectPlayName {
	uint32_t size;
	uint32_t flags;
	const char* shortName;
	const char* longName;
} XwaDirectPlayName;

typedef struct XwaDirectPlay4Vtbl {
	void* reserved0[2];
	uint32_t(XWA_DXAPI* Release)(XwaDirectPlay4* self);
	void* reserved3;
	HRESULT(XWA_DXAPI* Close)(XwaDirectPlay4* self);
	HRESULT(XWA_DXAPI* CreateGroup)(XwaDirectPlay4* self, int32_t* outGroupId, void* name, void* data,
									uint32_t dataSize, uint32_t flags);
	void* reserved6[3];
	HRESULT(XWA_DXAPI* DestroyPlayer)(XwaDirectPlay4* self, int32_t playerId);
	void* reserved10[4];
	HRESULT(XWA_DXAPI* GetCaps)(XwaDirectPlay4* self, void* caps, uint32_t flags);
	void* reserved15[11];
	HRESULT(XWA_DXAPI* Send)(XwaDirectPlay4* self, int32_t fromPlayerId, int32_t toPlayerId, uint32_t flags,
							 void* data, uint32_t dataSize);
	void* reservedAfterSend[3];
	HRESULT(XWA_DXAPI* SetPlayerName)(XwaDirectPlay4* self, uint32_t playerId, XwaDirectPlayName* name,
									  uint32_t flags);
} XwaDirectPlay4Vtbl;

struct XwaDirectPlay4 {
	const XwaDirectPlay4Vtbl* lpVtbl;
};

#endif

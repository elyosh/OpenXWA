#include "xwa_runtime/compat/directx/ddraw_compat_internal.h"

#include "aeron/aeron.h"
#include "aeron/log.h"
#include "aeron/render.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Direct3D -> Aeron compatibility shim (execute-buffer, IDirect3DDevice v1).
 *
 * The recovered std3D renderer drives Direct3D exactly as the original did: it
 * QIs its render-target DirectDraw surface for an IDirect3DDevice, enumerates
 * texture formats, creates a viewport and an execute buffer, fills the buffer
 * with TL vertices + a D3DOP instruction stream, and calls Execute per batch.
 * This layer provides those COM objects over Aeron. Object layout shared with
 * the DirectDraw side lives in ddraw_compat_internal.h.
 *
 * The object model, lifecycle, viewport/execute-buffer management, scene passes,
 * the execute-buffer interpreter (Execute), and the texture pipeline
 * (EnumTextureFormats / GetHandle / Load) are implemented here. The recovered
 * std3D_* functions reach this shim device directly (frontend and flight). */

/* --- object layouts ------------------------------------------------------ */

typedef struct D3DShim {
	const IDirect3DVtbl* lpVtbl;
	int refcount;
	DDrawShim* owner;
} D3DShim;

typedef struct D3DViewportShim {
	const IDirect3DViewportVtbl* lpVtbl;
	int refcount;
	int x;
	int y;
	int width;
	int height;
	float min_z;
	float max_z;
} D3DViewportShim;

typedef struct D3DExecBufShim {
	const IDirect3DExecuteBufferVtbl* lpVtbl;
	int refcount;
	void* data;
	uint32_t size;
	uint32_t vertex_offset;
	uint32_t vertex_count;
	uint32_t instruction_offset;
	uint32_t instruction_length;
} D3DExecBufShim;

typedef struct D3DSceneSegment D3DSceneSegment;

typedef enum D3DTexBlendMode {
	D3D_TEXBLEND_DECAL = 1,
	D3D_TEXBLEND_MODULATE = 2,
	D3D_TEXBLEND_MODULATEALPHA = 4,
} D3DTexBlendMode;

/* Folded std3D render state (see D3DCompat_ApplyRenderState). Persists on the
 * device across Execute calls, mirroring the IDirect3DDevice. */
typedef struct D3DRenderState {
	AeronCompareOp depth_compare;  /* from ZFUNC (ALWAYS when z-compare disabled) */
	int depth_write;               /* from ZWRITEENABLE */
	int alpha_test;                /* from ALPHATESTENABLE */
	int blend_enabled;             /* from ALPHABLENDENABLE */
	D3DTexBlendMode texture_blend; /* from TEXTUREMAPBLEND */
	int min_filter;                /* from TEXTUREMIN (D3DTEXTUREFILTER) */
	int mag_filter;                /* from TEXTUREMAG (1=nearest, 2=linear) */
	AeronAddressMode address_mode; /* from TEXTUREADDRESS */
} D3DRenderState;

struct D3DDeviceShim {
	const IDirect3DDeviceVtbl* lpVtbl;
	int refcount;
	DDrawSurfaceShim* target;  /* render-target surface bound to this device */
	D3DViewportShim* viewport; /* current viewport (from AddViewport) */
	D3DSceneSegment* segment;  /* deferred geometry for the current scene segment */
	int segment_pending;       /* a GPU pass is required at the next ordering boundary */
	AeronRectI segment_viewport;
	int segment_clear_depth;
	float segment_clear_depth_value;
	int scene_active; /* set between BeginScene and EndScene */

	/* Texture handle table (D3DRENDERSTATE_TEXTUREHANDLE indexes it; handle 0 =
	 * white fallback). IDirect3DTexture::GetHandle allocates entries and Load
	 * populates them; unresolved handles use the white fallback. */
	AeronTexture** handles;
	uint32_t handle_count;

	/* Render state + current texture persist across Execute calls, matching the
	 * IDirect3DDevice: std3D_SetRenderState delta-encodes (emits only the tokens
	 * that changed), so a buffer inherits whatever state earlier buffers left. */
	D3DRenderState render_state;
	uint32_t cur_texture_handle;
};

static const IDirect3DVtbl g_d3dVtbl;
static const IDirect3DDeviceVtbl g_d3dDeviceVtbl;

static int D3DCompat_StartSceneSegment(D3DDeviceShim* d);
static int D3DCompat_FlushSceneSegment(D3DDeviceShim* d);
static const IDirect3DViewportVtbl g_d3dViewportVtbl;
static const IDirect3DExecuteBufferVtbl g_d3dExecBufVtbl;

/* --- IDirect3DViewport --------------------------------------------------- */

static HRESULT XWA_DXAPI D3DViewport_QueryInterface(IDirect3DViewport* self, DxRefIid iid, void** out) {
	(void)self;
	(void)iid;
	if (out) {
		*out = NULL;
	}
	return DX_E_NOTIMPL;
}

static uint32_t XWA_DXAPI D3DViewport_AddRef(IDirect3DViewport* self) {
	return (uint32_t)++((D3DViewportShim*)self)->refcount;
}

static uint32_t XWA_DXAPI D3DViewport_Release(IDirect3DViewport* self) {
	D3DViewportShim* v = (D3DViewportShim*)self;
	if (--v->refcount > 0) {
		return (uint32_t)v->refcount;
	}
	free(v);
	return 0;
}

static HRESULT XWA_DXAPI D3DViewport_SetViewport(IDirect3DViewport* self, D3DVIEWPORT* vp) {
	D3DViewportShim* v = (D3DViewportShim*)self;
	if (!vp) {
		return DX_E_INVALIDARG;
	}
	v->x = (int)vp->dwX;
	v->y = (int)vp->dwY;
	v->width = (int)vp->dwWidth;
	v->height = (int)vp->dwHeight;
	v->min_z = vp->dvMinZ;
	v->max_z = vp->dvMaxZ;
	return DX_DD_OK;
}

static HRESULT XWA_DXAPI D3DViewport_SetBackground(IDirect3DViewport* self, D3DMATERIALHANDLE material) {
	(void)self;
	(void)material;
	return DX_DD_OK;
}

static const IDirect3DViewportVtbl g_d3dViewportVtbl = {
	.QueryInterface = D3DViewport_QueryInterface,
	.AddRef = D3DViewport_AddRef,
	.Release = D3DViewport_Release,
	.SetViewport = D3DViewport_SetViewport,
	.SetBackground = D3DViewport_SetBackground,
};

/* --- IDirect3DExecuteBuffer ---------------------------------------------- */

static HRESULT XWA_DXAPI D3DExecBuf_QueryInterface(IDirect3DExecuteBuffer* self, DxRefIid iid, void** out) {
	(void)self;
	(void)iid;
	if (out) {
		*out = NULL;
	}
	return DX_E_NOTIMPL;
}

static uint32_t XWA_DXAPI D3DExecBuf_AddRef(IDirect3DExecuteBuffer* self) {
	return (uint32_t)++((D3DExecBufShim*)self)->refcount;
}

static uint32_t XWA_DXAPI D3DExecBuf_Release(IDirect3DExecuteBuffer* self) {
	D3DExecBufShim* b = (D3DExecBufShim*)self;
	if (--b->refcount > 0) {
		return (uint32_t)b->refcount;
	}
	free(b->data);
	free(b);
	return 0;
}

static HRESULT XWA_DXAPI D3DExecBuf_Lock(IDirect3DExecuteBuffer* self, D3DEXECUTEBUFFERDESC* desc) {
	D3DExecBufShim* b = (D3DExecBufShim*)self;
	if (!desc) {
		return DX_E_INVALIDARG;
	}
	desc->lpData = b->data;
	desc->dwBufferSize = b->size;
	return DX_DD_OK;
}

static HRESULT XWA_DXAPI D3DExecBuf_Unlock(IDirect3DExecuteBuffer* self) {
	(void)self;
	return DX_DD_OK;
}

static HRESULT XWA_DXAPI D3DExecBuf_SetExecuteData(IDirect3DExecuteBuffer* self, D3DEXECUTEDATA* data) {
	D3DExecBufShim* b = (D3DExecBufShim*)self;
	if (!data) {
		return DX_E_INVALIDARG;
	}
	b->vertex_offset = data->dwVertexOffset;
	b->vertex_count = data->dwVertexCount;
	b->instruction_offset = data->dwInstructionOffset;
	b->instruction_length = data->dwInstructionLength;
	return DX_DD_OK;
}

static const IDirect3DExecuteBufferVtbl g_d3dExecBufVtbl = {
	.QueryInterface = D3DExecBuf_QueryInterface,
	.AddRef = D3DExecBuf_AddRef,
	.Release = D3DExecBuf_Release,
	.Lock = D3DExecBuf_Lock,
	.Unlock = D3DExecBuf_Unlock,
	.SetExecuteData = D3DExecBuf_SetExecuteData,
};

/* --- IDirect3DTexture ---------------------------------------------------- */

typedef struct D3DTextureShim {
	const IDirect3DTextureVtbl* lpVtbl;
	int refcount;
	DDrawSurfaceShim* surface; /* the surface this texture wraps */
	D3DDeviceShim* device;     /* device that issued the handle */
	uint32_t handle;           /* slot in device->handles */
	AeronTexture* texture;     /* GPU texture (created by Load) */
} D3DTextureShim;

static const IDirect3DTextureVtbl g_d3dTextureVtbl;

static HRESULT XWA_DXAPI D3DTexture_QueryInterface(IDirect3DTexture* self, DxRefIid iid, void** out) {
	(void)self;
	(void)iid;
	if (out) {
		*out = NULL;
	}
	return DX_E_NOTIMPL;
}

static uint32_t XWA_DXAPI D3DTexture_AddRef(IDirect3DTexture* self) {
	return (uint32_t)++((D3DTextureShim*)self)->refcount;
}

static uint32_t XWA_DXAPI D3DTexture_Release(IDirect3DTexture* self) {
	D3DTextureShim* t = (D3DTextureShim*)self;
	if (--t->refcount > 0) {
		return (uint32_t)t->refcount;
	}
	free(t);
	return 0;
}

/* Reserve a handle slot in the device table (0 stays the white fallback). */
static uint32_t D3DCompat_AllocHandle(D3DDeviceShim* d) {
	uint32_t handle = d->handle_count == 0 ? 1u : d->handle_count;
	AeronTexture** grown = (AeronTexture**)realloc(d->handles, (size_t)(handle + 1u) * sizeof(*grown));
	uint32_t i;

	if (!grown) {
		return 0;
	}
	for (i = d->handle_count; i <= handle; ++i) {
		grown[i] = NULL;
	}
	d->handles = grown;
	d->handle_count = handle + 1u;
	return handle;
}

static HRESULT XWA_DXAPI D3DTexture_GetHandle(IDirect3DTexture* self, IDirect3DDevice* device,
											  D3DTEXTUREHANDLE* handle) {
	D3DTextureShim* t = (D3DTextureShim*)self;
	uint32_t h;

	if (!handle || !device) {
		return DX_E_INVALIDARG;
	}
	t->device = (D3DDeviceShim*)device;
	h = D3DCompat_AllocHandle(t->device);
	if (h == 0) {
		return DX_E_FAIL;
	}
	t->handle = h;
	*handle = h;
	return DX_DD_OK;
}

/* Expand a masked channel to 8 bits; an absent mask (e.g. no alpha) reads as
 * fully opaque, matching RGB565/RGB (no-alpha) texture formats. */
static uint8_t D3DCompat_ExpandChannel(uint32_t value, uint32_t mask) {
	uint32_t shift = 0;
	uint32_t max_value;

	if (mask == 0) {
		return 255;
	}
	while (((mask >> shift) & 1u) == 0u) {
		++shift;
	}
	max_value = mask >> shift;
	return (uint8_t)((((value & mask) >> shift) * 255u + max_value / 2u) / max_value);
}

/* Unpack a source texture surface (PAL8 or a 16bpp masked format) to RGBA8. */
static int D3DCompat_UnpackToRgba8(DDrawSurfaceShim* src, uint8_t* out) {
	const DDPIXELFORMAT* pf = &src->pixel_format;
	uint8_t* pixels;
	int pitch;
	int x;
	int y;

	if (!src->cpu) {
		return 0;
	}
	pixels = (uint8_t*)AeronSurface_Lock(src->cpu, &pitch);
	if (!pixels) {
		return 0;
	}

	if (pf->dwFlags & DDPF_PALETTEINDEXED8) {
		for (y = 0; y < src->height; ++y) {
			const uint8_t* row = pixels + (size_t)y * pitch;
			for (x = 0; x < src->width; ++x) {
				uint32_t o = 4u * ((uint32_t)y * (uint32_t)src->width + (uint32_t)x);
				uint8_t idx = row[x];

				if (src->palette) {
					out[o + 0] = src->palette->entries[idx].r;
					out[o + 1] = src->palette->entries[idx].g;
					out[o + 2] = src->palette->entries[idx].b;
				} else {
					out[o + 0] = idx;
					out[o + 1] = idx;
					out[o + 2] = idx;
				}
				out[o + 3] = 255;
			}
		}
	} else {
		for (y = 0; y < src->height; ++y) {
			const uint16_t* row = (const uint16_t*)(pixels + (size_t)y * pitch);
			for (x = 0; x < src->width; ++x) {
				uint32_t o = 4u * ((uint32_t)y * (uint32_t)src->width + (uint32_t)x);
				uint16_t v = row[x];

				out[o + 0] = D3DCompat_ExpandChannel(v, pf->dwRBitMask);
				out[o + 1] = D3DCompat_ExpandChannel(v, pf->dwGBitMask);
				out[o + 2] = D3DCompat_ExpandChannel(v, pf->dwBBitMask);
				out[o + 3] = D3DCompat_ExpandChannel(v, pf->dwRGBAlphaBitMask);
			}
		}
	}

	AeronSurface_Unlock(src->cpu);
	return 1;
}

static HRESULT XWA_DXAPI D3DTexture_Load(IDirect3DTexture* self, IDirect3DTexture* source) {
	D3DTextureShim* dest = (D3DTextureShim*)self;
	D3DTextureShim* src = (D3DTextureShim*)source;
	DDrawSurfaceShim* level;
	AeronTextureUploadDesc* uploads;
	uint8_t** rgba_levels;
	AeronCommandBuffer* upload_cmd;
	int mip_count;
	int mip;
	int uploaded;

	if (!src || !src->surface || !src->surface->has_pixel_format || !dest->device) {
		return DX_E_INVALIDARG;
	}
	if (src->surface->width <= 0 || src->surface->height <= 0) {
		return DX_E_FAIL;
	}
	if (!D3DCompat_FlushSceneSegment(dest->device)) {
		return DX_E_FAIL;
	}

	/* Count mip levels in the source's attached chain (1 for a plain texture). */
	mip_count = 0;
	for (level = src->surface; level; level = level->attached) {
		++mip_count;
	}

	if (!dest->texture) {
		dest->texture = Aeron_CreateTexture(&(AeronTextureDesc) {
			.width = src->surface->width,
			.height = src->surface->height,
			.mip_count = mip_count,
			.format = AERON_TEXTURE_FORMAT_RGBA8_UNORM,
			.usage = AERON_TEXTURE_USAGE_SAMPLED | AERON_TEXTURE_USAGE_TRANSFER_DST,
		});
	}
	if (!dest->texture) {
		return DX_E_FAIL;
	}

	uploads = (AeronTextureUploadDesc*)calloc((size_t)mip_count, sizeof *uploads);
	rgba_levels = (uint8_t**)calloc((size_t)mip_count, sizeof *rgba_levels);
	if (!uploads || !rgba_levels) {
		free(uploads);
		free(rgba_levels);
		return DX_E_FAIL;
	}
	mip = 0;
	for (level = src->surface; level; level = level->attached, ++mip) {
		rgba_levels[mip] = (uint8_t*)malloc((size_t)level->width * (size_t)level->height * 4u);
		if (!rgba_levels[mip] || !D3DCompat_UnpackToRgba8(level, rgba_levels[mip])) {
			for (int i = 0; i <= mip; ++i)
				free(rgba_levels[i]);
			free(rgba_levels);
			free(uploads);
			return DX_E_FAIL;
		}
		uploads[mip] = (AeronTextureUploadDesc) {
			.texture = dest->texture,
			.mip_level = mip,
			.width = level->width,
			.height = level->height,
			.pixels = rgba_levels[mip],
			.pitch = level->width * 4,
			.pixel_format = AERON_PIXEL_FORMAT_RGBA8888,
			.color_space = AERON_COLOR_SPACE_SRGB,
			/* Cycle once for the complete mip-chain rewrite. */
			.cycle = mip == 0,
		};
	}
	upload_cmd = Aeron_AcquireCommandBuffer();
	uploaded = upload_cmd && Aeron_UploadTextureBatchCmd(upload_cmd, uploads, (uint32_t)mip_count);
	if (upload_cmd) {
		if (uploaded)
			uploaded = Aeron_SubmitCommandBuffer(upload_cmd);
		else
			Aeron_CancelCommandBuffer(upload_cmd);
	}
	for (mip = 0; mip < mip_count; ++mip)
		free(rgba_levels[mip]);
	free(rgba_levels);
	free(uploads);
	if (!uploaded) {
		Aeron_RequestFatalRendererError("Direct3D compatibility texture upload");
		return DX_E_FAIL;
	}

	if (dest->handle > 0 && dest->handle < dest->device->handle_count) {
		dest->device->handles[dest->handle] = dest->texture;
	}
	return DX_DD_OK;
}

static const IDirect3DTextureVtbl g_d3dTextureVtbl = {
	.QueryInterface = D3DTexture_QueryInterface,
	.AddRef = D3DTexture_AddRef,
	.Release = D3DTexture_Release,
	.GetHandle = D3DTexture_GetHandle,
	.Load = D3DTexture_Load,
};

/* --- execute-buffer interpreter ----------------------------------------- */

/* GPU vertex/uniform layout and the pipeline/sampler machinery. The render state
 * is folded directly from D3DRENDERSTATE_* tokens (see std3D_SetRenderState
 * @0x5955F0) here rather than through an intermediate flag encoding. */

typedef struct D3DGpuVertex {
	float screen[4];
	uint8_t color[4];
	uint8_t specular[4];
	float texcoord[2];
} D3DGpuVertex;

typedef struct D3DViewportUniform {
	float viewport[4];
	float depth_params[4];
} D3DViewportUniform;

typedef struct D3DFragmentUniform {
	float params[4];
} D3DFragmentUniform;

typedef struct D3DSceneDraw {
	D3DRenderState state;
	AeronGraphicsPipeline* pipeline;
	AeronSampler* sampler;
	AeronTexture* texture;
	D3DViewportUniform viewport;
	uint32_t first_index;
	uint32_t index_count;
	int32_t vertex_offset;
} D3DSceneDraw;

struct D3DSceneSegment {
	AeronBuffer* vertex_buffer;
	uint32_t vertex_buffer_size;
	AeronBuffer* index_buffer;
	uint32_t index_buffer_size;
	D3DGpuVertex* vertices;
	uint32_t vertex_count;
	uint32_t vertex_capacity;
	uint16_t* indices;
	uint32_t index_count;
	uint32_t index_capacity;
	D3DSceneDraw* draws;
	uint32_t draw_count;
	uint32_t draw_capacity;
};

/* DX5 D3DTEXTUREFILTER values (TEXTUREMAG 1..2, TEXTUREMIN 1..6). */
enum {
	D3D_FILTER_NEAREST = 1,
	D3D_FILTER_LINEAR = 2,
	D3D_FILTER_MIPNEAREST = 3,
	D3D_FILTER_MIPLINEAR = 4,
	D3D_FILTER_LINEARMIPNEAREST = 5,
	D3D_FILTER_LINEARMIPLINEAR = 6,
};

/* DX5 D3DTEXTUREADDRESS values used by std3D. */
enum { D3D_TADDRESS_WRAP = 1, D3D_TADDRESS_CLAMP = 3 };

typedef struct D3DPipelineKey {
	AeronTextureFormat color_format;
	AeronTextureFormat depth_format;
	int depth_test;
	int depth_write;
	AeronCompareOp depth_compare;
	int alpha_test;
	int blend_enabled;
	D3DTexBlendMode texture_blend;
} D3DPipelineKey;

typedef struct D3DPipelineCache {
	D3DPipelineKey key;
	AeronGraphicsPipeline* pipeline;
	struct D3DPipelineCache* next;
} D3DPipelineCache;

typedef struct D3DSamplerKey {
	int min_filter;
	int mag_filter;
	AeronAddressMode address_mode;
} D3DSamplerKey;

typedef struct D3DSamplerCache {
	D3DSamplerKey key;
	AeronSampler* sampler;
	struct D3DSamplerCache* next;
} D3DSamplerCache;

/* A run of triangles sharing one (texture handle, render state). */
typedef struct D3DRun {
	uint32_t texture_handle;
	D3DRenderState state;
	uint32_t first_index;
	uint32_t index_count;
} D3DRun;

static AeronShader* g_d3dVertexShader;
static AeronShader* g_d3dFragmentShader;
static D3DPipelineCache* g_d3dPipelineCache;
static D3DSamplerCache* g_d3dSamplerCache;
static uint16_t* g_d3dIndexScratch;
static uint32_t g_d3dIndexScratchCap;
static D3DRun* g_d3dRuns;
static uint32_t g_d3dRunCap;
static AeronTexture* g_d3dWhiteTexture;

static const D3DRenderState g_d3dDefaultRenderState = {
	.depth_compare = AERON_COMPARE_ALWAYS,
	.depth_write = 0,
	.alpha_test = 1,
	.blend_enabled = 0,
	.texture_blend = D3D_TEXBLEND_MODULATE,
	.min_filter = D3D_FILTER_NEAREST,
	.mag_filter = D3D_FILTER_NEAREST,
	.address_mode = AERON_ADDRESS_REPEAT,
};

static AeronCompareOp D3DCompat_MapCompare(uint32_t d3d_compare) {
	switch (d3d_compare) {
		case 2:
			return AERON_COMPARE_LESS;
		case 3:
			return AERON_COMPARE_EQUAL;
		case 4:
			return AERON_COMPARE_LESS_EQUAL;
		case 5:
			return AERON_COMPARE_GREATER;
		case 7:
			return AERON_COMPARE_GREATER_EQUAL;
		case 8:
		default:
			return AERON_COMPARE_ALWAYS;
	}
}

static int D3DCompat_DepthTestEnabled(const D3DRenderState* rs) {
	return rs->depth_compare != AERON_COMPARE_ALWAYS || rs->depth_write;
}

static int D3DCompat_EnsureShaders(void) {
	if (!g_d3dVertexShader) {
		g_d3dVertexShader = Aeron_CreateShader(&(AeronShaderDesc) {
			.name = "d3d_projected_triangle.vert",
			.stage = AERON_SHADER_STAGE_VERTEX,
			.uniform_buffer_count = 1,
		});
		if (!g_d3dVertexShader) {
			Aeron_LogError("xwa.d3d", "Failed to load XWA D3D projected triangle vertex shader");
			return 0;
		}
	}
	if (!g_d3dFragmentShader) {
		g_d3dFragmentShader = Aeron_CreateShader(&(AeronShaderDesc) {
			.name = "d3d_projected_triangle.frag",
			.stage = AERON_SHADER_STAGE_FRAGMENT,
			.sampler_count = 1,
			.uniform_buffer_count = 1,
		});
		if (!g_d3dFragmentShader) {
			Aeron_LogError("xwa.d3d", "Failed to load XWA D3D projected triangle fragment shader");
			return 0;
		}
	}
	return 1;
}

static AeronGraphicsPipeline* D3DCompat_GetPipeline(AeronTextureFormat color_format,
													AeronTextureFormat depth_format,
													const D3DRenderState* state) {
	const AeronVertexBufferLayoutDesc vertex_buffers[] = {
		{ .slot = 0, .stride = sizeof(D3DGpuVertex), .per_instance = 0 },
	};
	/* The D3DTLVERTEX specular colour is intentionally unbound: every recovered
	 * call site writes it as 0 and this shim ignores
	 * D3DRENDERSTATE_SPECULARENABLE, so nothing consumes it. The shader does not
	 * declare it and the remaining locations close up. The field stays in
	 * D3DGpuVertex to keep the stream a faithful D3DTLVERTEX mirror. */
	const AeronVertexAttributeDesc attributes[] = {
		{ .location = 0,
		  .buffer_slot = 0,
		  .format = AERON_VERTEX_FORMAT_FLOAT4,
		  .offset = offsetof(D3DGpuVertex, screen) },
		{ .location = 1,
		  .buffer_slot = 0,
		  .format = AERON_VERTEX_FORMAT_UBYTE4_NORM,
		  .offset = offsetof(D3DGpuVertex, color) },
		{ .location = 2,
		  .buffer_slot = 0,
		  .format = AERON_VERTEX_FORMAT_FLOAT2,
		  .offset = offsetof(D3DGpuVertex, texcoord) },
	};
	D3DPipelineKey key;
	D3DPipelineCache* entry;
	AeronGraphicsPipeline* pipeline;
	int depth_test;

	if (!D3DCompat_EnsureShaders()) {
		return NULL;
	}

	depth_test = D3DCompat_DepthTestEnabled(state);
	memset(&key, 0, sizeof(key));
	key.color_format = color_format;
	key.depth_format = depth_format;
	key.depth_test = depth_test;
	key.depth_write = state->depth_write;
	key.depth_compare = state->depth_compare;
	key.alpha_test = state->alpha_test;
	key.blend_enabled = state->blend_enabled;
	key.texture_blend = state->texture_blend;

	for (entry = g_d3dPipelineCache; entry; entry = entry->next) {
		if (memcmp(&entry->key, &key, sizeof(key)) == 0) {
			return entry->pipeline;
		}
	}

	pipeline = Aeron_CreateGraphicsPipeline(&(AeronGraphicsPipelineDesc) {
		.vertex_shader = g_d3dVertexShader,
		.fragment_shader = g_d3dFragmentShader,
		.primitive_type = AERON_PRIMITIVE_TRIANGLES,
		.cull_mode = AERON_CULL_NONE,
		.vertex_buffers = vertex_buffers,
		.vertex_buffer_count = 1,
		.attributes = attributes,
		.attribute_count = 3,
		.color_format = color_format,
		.depth_format = depth_format,
		.depth = { .depth_test = depth_test,
				   .depth_write = state->depth_write,
				   .compare = state->depth_compare },
		.blend = { .enabled = state->blend_enabled,
				   .src_color = state->blend_enabled ? AERON_BLEND_SRC_ALPHA : AERON_BLEND_ONE,
				   .dst_color = state->blend_enabled ? AERON_BLEND_ONE_MINUS_SRC_ALPHA : AERON_BLEND_ZERO,
				   .color_op = AERON_BLEND_OP_ADD,
				   .src_alpha = state->blend_enabled ? AERON_BLEND_SRC_ALPHA : AERON_BLEND_ONE,
				   .dst_alpha = state->blend_enabled ? AERON_BLEND_ONE_MINUS_SRC_ALPHA : AERON_BLEND_ZERO,
				   .alpha_op = AERON_BLEND_OP_ADD },
	});
	if (!pipeline) {
		Aeron_LogError("xwa.d3d", "Failed to create projected triangle pipeline");
		return NULL;
	}

	entry = (D3DPipelineCache*)calloc(1, sizeof(*entry));
	if (!entry) {
		Aeron_DestroyGraphicsPipeline(pipeline);
		return NULL;
	}
	entry->key = key;
	entry->pipeline = pipeline;
	entry->next = g_d3dPipelineCache;
	g_d3dPipelineCache = entry;
	return pipeline;
}

static AeronFilter D3DCompat_MagFilter(int filter) {
	return filter == D3D_FILTER_LINEAR ? AERON_FILTER_LINEAR : AERON_FILTER_NEAREST;
}

static AeronFilter D3DCompat_MinFilter(int filter) {
	switch (filter) {
		case D3D_FILTER_LINEAR:
		case D3D_FILTER_MIPLINEAR:
		case D3D_FILTER_LINEARMIPLINEAR:
			return AERON_FILTER_LINEAR;
		default:
			return AERON_FILTER_NEAREST;
	}
}

static AeronFilter D3DCompat_MipFilter(int filter) {
	switch (filter) {
		case D3D_FILTER_LINEARMIPNEAREST:
		case D3D_FILTER_LINEARMIPLINEAR:
			return AERON_FILTER_LINEAR;
		default:
			return AERON_FILTER_NEAREST;
	}
}

static int D3DCompat_FilterUsesMipmaps(int filter) {
	return filter == D3D_FILTER_MIPNEAREST || filter == D3D_FILTER_MIPLINEAR ||
		   filter == D3D_FILTER_LINEARMIPNEAREST || filter == D3D_FILTER_LINEARMIPLINEAR;
}

static AeronSampler* D3DCompat_GetSampler(const D3DRenderState* state) {
	D3DSamplerKey key;
	D3DSamplerCache* entry;
	AeronSampler* sampler;

	key.min_filter = state->min_filter;
	key.mag_filter = state->mag_filter;
	key.address_mode = state->address_mode;

	for (entry = g_d3dSamplerCache; entry; entry = entry->next) {
		if (entry->key.min_filter == key.min_filter && entry->key.mag_filter == key.mag_filter &&
			entry->key.address_mode == key.address_mode) {
			return entry->sampler;
		}
	}

	sampler = Aeron_CreateSampler(&(AeronSamplerDesc) {
		.min_filter = D3DCompat_MinFilter(key.min_filter),
		.mag_filter = D3DCompat_MagFilter(key.mag_filter),
		.mip_filter = D3DCompat_MipFilter(key.min_filter),
		.address_u = key.address_mode,
		.address_v = key.address_mode,
		.address_w = key.address_mode,
		.min_lod = 0.0f,
		.max_lod = D3DCompat_FilterUsesMipmaps(key.min_filter) ? 16.0f : 0.0f,
		.enable_anisotropy = 0,
		.max_anisotropy = 0.0f,
	});
	if (!sampler) {
		Aeron_LogError("xwa.d3d", "Failed to create std3D sampler");
		return NULL;
	}

	entry = (D3DSamplerCache*)calloc(1, sizeof(*entry));
	if (!entry) {
		Aeron_DestroySampler(sampler);
		return NULL;
	}
	entry->key = key;
	entry->sampler = sampler;
	entry->next = g_d3dSamplerCache;
	g_d3dSamplerCache = entry;
	return sampler;
}

static int D3DCompat_EnsureBuffer(AeronBuffer** buffer, uint32_t* current_size, uint32_t size, uint32_t usage,
								  const char* name) {
	uint32_t allocation_size;

	if (*buffer && *current_size >= size) {
		return 1;
	}
	allocation_size = *current_size > 4096u ? *current_size : 4096u;
	while (allocation_size < size) {
		if (allocation_size > UINT32_MAX / 2u) {
			allocation_size = size;
			break;
		}
		allocation_size *= 2u;
	}
	if (*buffer) {
		Aeron_DestroyBuffer(*buffer);
		*buffer = NULL;
		*current_size = 0;
	}
	*buffer = Aeron_CreateBuffer(&(AeronBufferDesc) {
		.size = allocation_size,
		.usage = usage,
		.memory_usage = AERON_MEMORY_USAGE_DYNAMIC,
		.debug_name = name,
	});
	if (!*buffer) {
		Aeron_LogError("xwa.d3d", "Failed to create %s buffer for %u bytes", name, allocation_size);
		return 0;
	}
	*current_size = allocation_size;
	return 1;
}

static int D3DCompat_EnsureBuffers(D3DSceneSegment* segment, uint32_t vertex_count, uint32_t index_count) {
	uint32_t vertex_bytes = vertex_count * (uint32_t)sizeof(D3DGpuVertex);
	uint32_t index_bytes = index_count * (uint32_t)sizeof(uint16_t);

	if (vertex_count == 0 || index_count == 0) {
		return 0;
	}
	if (vertex_bytes / sizeof(D3DGpuVertex) != vertex_count ||
		index_bytes / sizeof(uint16_t) != index_count) {
		return 0;
	}
	return D3DCompat_EnsureBuffer(&segment->vertex_buffer, &segment->vertex_buffer_size, vertex_bytes,
								  AERON_BUFFER_USAGE_VERTEX, "projected vertex") &&
		   D3DCompat_EnsureBuffer(&segment->index_buffer, &segment->index_buffer_size, index_bytes,
								  AERON_BUFFER_USAGE_INDEX, "projected index");
}

static uint32_t D3DCompat_GrownCapacity(uint32_t current, uint32_t required) {
	uint32_t capacity = current > 256u ? current : 256u;

	while (capacity < required) {
		if (capacity > UINT32_MAX / 2u) {
			return required;
		}
		capacity *= 2u;
	}
	return capacity;
}

static int D3DCompat_EnsureSegmentStorage(D3DSceneSegment* segment, uint32_t vertex_count,
										  uint32_t index_count, uint32_t draw_count) {
	if (segment->vertex_capacity < vertex_count) {
		uint32_t capacity = D3DCompat_GrownCapacity(segment->vertex_capacity, vertex_count);
		D3DGpuVertex* vertices =
			(D3DGpuVertex*)realloc(segment->vertices, (size_t)capacity * sizeof(*vertices));
		if (!vertices) {
			return 0;
		}
		segment->vertices = vertices;
		segment->vertex_capacity = capacity;
	}
	if (segment->index_capacity < index_count) {
		uint32_t capacity = D3DCompat_GrownCapacity(segment->index_capacity, index_count);
		uint16_t* indices = (uint16_t*)realloc(segment->indices, (size_t)capacity * sizeof(*indices));
		if (!indices) {
			return 0;
		}
		segment->indices = indices;
		segment->index_capacity = capacity;
	}
	if (segment->draw_capacity < draw_count) {
		uint32_t capacity = D3DCompat_GrownCapacity(segment->draw_capacity, draw_count);
		D3DSceneDraw* draws = (D3DSceneDraw*)realloc(segment->draws, (size_t)capacity * sizeof(*draws));
		if (!draws) {
			return 0;
		}
		segment->draws = draws;
		segment->draw_capacity = capacity;
	}
	return 1;
}

static void D3DCompat_ResetSceneSegment(D3DSceneSegment* segment) {
	segment->vertex_count = 0;
	segment->index_count = 0;
	segment->draw_count = 0;
}

static void D3DCompat_DestroySceneSegment(D3DSceneSegment* segment) {
	if (!segment) {
		return;
	}
	if (segment->vertex_buffer) {
		Aeron_DestroyBuffer(segment->vertex_buffer);
	}
	if (segment->index_buffer) {
		Aeron_DestroyBuffer(segment->index_buffer);
	}
	free(segment->vertices);
	free(segment->indices);
	free(segment->draws);
	free(segment);
}

static int D3DCompat_EnsureScratch(uint32_t index_count, uint32_t run_count) {
	if (g_d3dIndexScratchCap < index_count) {
		uint16_t* idx = (uint16_t*)realloc(g_d3dIndexScratch, (size_t)index_count * sizeof(*idx));
		if (!idx) {
			return 0;
		}
		g_d3dIndexScratch = idx;
		g_d3dIndexScratchCap = index_count;
	}
	if (g_d3dRunCap < run_count) {
		D3DRun* runs = (D3DRun*)realloc(g_d3dRuns, (size_t)run_count * sizeof(*runs));
		if (!runs) {
			return 0;
		}
		g_d3dRuns = runs;
		g_d3dRunCap = run_count;
	}
	return 1;
}

static int D3DCompat_EnsureWhiteTexture(void) {
	const uint8_t white_pixel[4] = { 255, 255, 255, 255 };

	if (g_d3dWhiteTexture) {
		return 1;
	}
	g_d3dWhiteTexture = Aeron_CreateTexture(&(AeronTextureDesc) {
		.width = 1,
		.height = 1,
		.mip_count = 1,
		.format = AERON_TEXTURE_FORMAT_RGBA8_UNORM,
		.usage = AERON_TEXTURE_USAGE_SAMPLED | AERON_TEXTURE_USAGE_TRANSFER_DST,
	});
	if (!g_d3dWhiteTexture) {
		Aeron_RequestFatalRendererError("classic white fallback texture creation");
		return 0;
	}
	if (!Aeron_UploadTextureData(&(AeronTextureUploadDesc) {
			.texture = g_d3dWhiteTexture,
			.mip_level = 0,
			.width = 1,
			.height = 1,
			.pixels = white_pixel,
			.pitch = 4,
			.pixel_format = AERON_PIXEL_FORMAT_RGBA8888,
			.color_space = AERON_COLOR_SPACE_SRGB,
			.cycle = 1,
			})) {
		Aeron_DestroyTexture(g_d3dWhiteTexture);
		g_d3dWhiteTexture = NULL;
		Aeron_RequestFatalRendererError("classic white fallback texture upload");
		return 0;
	}
	return 1;
}

static void D3DCompat_ConvertVertex(D3DGpuVertex* out, const D3DTLVERTEX* in) {
	out->screen[0] = in->sx;
	out->screen[1] = in->sy;
	out->screen[2] = in->sz;
	out->screen[3] = in->rhw;
	out->color[0] = (uint8_t)((in->color >> 16) & 0xffu);
	out->color[1] = (uint8_t)((in->color >> 8) & 0xffu);
	out->color[2] = (uint8_t)(in->color & 0xffu);
	out->color[3] = (uint8_t)((in->color >> 24) & 0xffu);
	out->specular[0] = (uint8_t)((in->specular >> 16) & 0xffu);
	out->specular[1] = (uint8_t)((in->specular >> 8) & 0xffu);
	out->specular[2] = (uint8_t)(in->specular & 0xffu);
	out->specular[3] = (uint8_t)((in->specular >> 24) & 0xffu);
	out->texcoord[0] = in->tu;
	out->texcoord[1] = in->tv;
}

/* Fold one D3DOP_STATERENDER {token, value} pair into the running render state
 * and current texture handle (Appendix B; std3D_SetRenderState @0x5955F0). */
static void D3DCompat_ApplyRenderState(D3DRenderState* rs, uint32_t* texture_handle, uint32_t token,
									   uint32_t value) {
	switch (token) {
		case D3DRENDERSTATE_TEXTUREHANDLE:
			*texture_handle = value;
			break;
		case D3DRENDERSTATE_TEXTUREADDRESS:
			rs->address_mode =
				value == D3D_TADDRESS_CLAMP ? AERON_ADDRESS_CLAMP_TO_EDGE : AERON_ADDRESS_REPEAT;
			break;
		case D3DRENDERSTATE_ZFUNC:
			rs->depth_compare = D3DCompat_MapCompare(value);
			break;
		case D3DRENDERSTATE_ZWRITEENABLE:
			rs->depth_write = value != 0;
			break;
		case D3DRENDERSTATE_ALPHATESTENABLE:
			rs->alpha_test = value != 0;
			break;
		case D3DRENDERSTATE_BLENDENABLE:
			rs->blend_enabled = value != 0;
			break;
		case D3DRENDERSTATE_TEXTUREMAPBLEND:
			rs->texture_blend = (D3DTexBlendMode)value;
			break;
		case D3DRENDERSTATE_TEXTUREMAG:
			rs->mag_filter = (int)value;
			break;
		case D3DRENDERSTATE_TEXTUREMIN:
			rs->min_filter = (value >= D3D_FILTER_NEAREST && value <= D3D_FILTER_LINEARMIPLINEAR)
								 ? (int)value
								 : D3D_FILTER_NEAREST;
			break;
		/* SRCBLEND/DESTBLEND are always SRCALPHA/INVSRCALPHA or ONE/ZERO, fully
		 * determined by ALPHABLENDENABLE; ALPHAFUNC is always NOTEQUAL (the alpha
		 * test the fragment shader implements); MONOENABLE is a no-op. */
		case D3DRENDERSTATE_SRCBLEND:
		case D3DRENDERSTATE_DESTBLEND:
		case D3DRENDERSTATE_ALPHAFUNC:
		case D3DRENDERSTATE_MONOENABLE:
			break;
		case D3DRENDERSTATE_FOGENABLE:
			if (value) {
				Aeron_LogWarn("xwa.d3d", "std3D fog is not implemented yet");
			}
			break;
		case D3DRENDERSTATE_FOGCOLOR:
		case D3DRENDERSTATE_FOGTABLEMODE:
		case D3DRENDERSTATE_FOGTABLESTART:
		case D3DRENDERSTATE_FOGTABLEEND:
			break;
		default:
			Aeron_LogWarn("xwa.d3d", "Unexpected D3DRENDERSTATE token %u in execute buffer", token);
			break;
	}
}

static AeronTexture* D3DCompat_TextureForHandle(D3DDeviceShim* d, uint32_t handle) {
	if (handle == 0 || handle >= d->handle_count || !d->handles[handle]) {
		return g_d3dWhiteTexture;
	}
	return d->handles[handle];
}

static void D3DCompat_GetViewportUniform(const D3DDeviceShim* d, D3DViewportUniform* uniform) {
	uniform->viewport[0] = d->viewport ? (float)d->viewport->x : 0.0f;
	uniform->viewport[1] = d->viewport ? (float)d->viewport->y : 0.0f;
	uniform->viewport[2] = d->viewport ? (float)d->viewport->width : (float)d->target->width;
	uniform->viewport[3] = d->viewport ? (float)d->viewport->height : (float)d->target->height;
	uniform->depth_params[0] = 1.0f;
	uniform->depth_params[1] = 0.0f;
	uniform->depth_params[2] = 0.0f;
	uniform->depth_params[3] = 0.0f;
}

/* Preserve the device state stream while classic flight geometry is discarded.
 * std3D delta-encodes render-state changes across Execute calls, so returning
 * success without consuming D3DOP_STATERENDER would make the first later
 * CLASSIC/SPLIT frame inherit stale depth, blend, filter, and texture state. */
static int D3DCompat_ExecuteStateOnly(D3DDeviceShim* d, const D3DExecBufShim* buf) {
	const uint8_t* base = (const uint8_t*)buf->data;
	const uint8_t* ip = base + buf->instruction_offset;
	const uint8_t* ip_end = ip + buf->instruction_length;
	D3DRenderState state = d->render_state;
	uint32_t texture_handle = d->cur_texture_handle;
	int exited = 0;

	while (ip + sizeof(D3DINSTRUCTION) <= ip_end && !exited) {
		const D3DINSTRUCTION* insn = (const D3DINSTRUCTION*)ip;
		const uint8_t* payload = ip + sizeof(D3DINSTRUCTION);
		const uint32_t payload_size = (uint32_t)insn->wCount * insn->bSize;
		uint32_t unit;

		if (payload_size > (uint32_t)(ip_end - payload)) {
			break;
		}
		switch (insn->bOpcode) {
			case D3DOP_STATERENDER:
				for (unit = 0; unit < insn->wCount; ++unit) {
					const D3DSTATE* s = (const D3DSTATE*)(payload + unit * sizeof(D3DSTATE));
					D3DCompat_ApplyRenderState(&state, &texture_handle, s->dwState, s->dwArg);
				}
				break;
			case D3DOP_PROCESSVERTICES:
			case D3DOP_TRIANGLE:
				break;
			case D3DOP_EXIT:
				exited = 1;
				break;
			default:
				Aeron_LogWarn("xwa.d3d", "Unexpected execute-buffer opcode %u", insn->bOpcode);
				break;
		}
		ip = payload + payload_size;
	}

	d->render_state = state;
	d->cur_texture_handle = texture_handle;
	return 1;
}

/* Interpret an execute buffer immediately, but defer its uploads and draw encoding
 * until the next Direct3D/DirectDraw ordering boundary. */
static int D3DCompat_ExecuteBuffer(D3DDeviceShim* d, D3DExecBufShim* buf) {
	D3DSceneSegment* segment;
	const D3DTLVERTEX* verts;
	const uint8_t* base;
	const uint8_t* ip;
	const uint8_t* ip_end;
	D3DRenderState state;
	uint32_t texture_handle;
	uint32_t run_count;
	uint32_t index_cursor;
	uint32_t vertex_base;
	uint32_t index_base;
	uint32_t draw_base;
	uint32_t i;
	D3DViewportUniform vp_uniform;
	int exited;

	if (!buf || !buf->data) {
		return 0;
	}
	if (buf->vertex_count > (uint32_t)UINT16_MAX + 1u) {
		return 0;
	}
	if (buf->instruction_offset > buf->size ||
		buf->instruction_length > buf->size - buf->instruction_offset) {
		return 0;
	}
	if (!d->scene_active) {
		return 0;
	}
	if (buf->vertex_count == 0 || DDrawCompat_IsClassicFlightRenderingSuppressed()) {
		return D3DCompat_ExecuteStateOnly(d, buf);
	}
	if (!D3DCompat_StartSceneSegment(d)) {
		return 0;
	}

	base = (const uint8_t*)buf->data;
	verts = (const D3DTLVERTEX*)base;
	ip = base + buf->instruction_offset;
	ip_end = ip + buf->instruction_length;

	/* Worst case: one run per triangle. Cap scratch to that. */
	if (!D3DCompat_EnsureScratch(buf->instruction_length, buf->instruction_length) ||
		!D3DCompat_EnsureWhiteTexture()) {
		return 0;
	}

	/* Inherit the device's persistent render state / texture; std3D only emits the
	 * tokens that changed since the previous Execute. */
	state = d->render_state;
	texture_handle = d->cur_texture_handle;
	run_count = 0;
	index_cursor = 0;
	exited = 0;

	while (ip + sizeof(D3DINSTRUCTION) <= ip_end && !exited) {
		const D3DINSTRUCTION* insn = (const D3DINSTRUCTION*)ip;
		const uint8_t* payload = ip + sizeof(D3DINSTRUCTION);
		uint32_t unit;

		if (payload + (uint32_t)insn->wCount * insn->bSize > ip_end) {
			break;
		}

		switch (insn->bOpcode) {
			case D3DOP_STATERENDER:
				for (unit = 0; unit < insn->wCount; ++unit) {
					const D3DSTATE* s = (const D3DSTATE*)(payload + unit * sizeof(D3DSTATE));
					D3DCompat_ApplyRenderState(&state, &texture_handle, s->dwState, s->dwArg);
				}
				break;
			case D3DOP_PROCESSVERTICES:
				/* TL vertices are already screen-space; nothing to transform. */
				break;
			case D3DOP_TRIANGLE:
				for (unit = 0; unit < insn->wCount; ++unit) {
					const D3DTRIANGLE* tri = (const D3DTRIANGLE*)(payload + unit * sizeof(D3DTRIANGLE));
					D3DRun* run;

					if (tri->v1 >= buf->vertex_count || tri->v2 >= buf->vertex_count ||
						tri->v3 >= buf->vertex_count) {
						continue;
					}
					/* Extend the current run if the (texture,state) is unchanged;
					 * otherwise start a new one. */
					if (run_count > 0 && g_d3dRuns[run_count - 1].texture_handle == texture_handle &&
						memcmp(&g_d3dRuns[run_count - 1].state, &state, sizeof(state)) == 0) {
						run = &g_d3dRuns[run_count - 1];
					} else {
						run = &g_d3dRuns[run_count++];
						run->texture_handle = texture_handle;
						run->state = state;
						run->first_index = index_cursor;
						run->index_count = 0;
					}
					g_d3dIndexScratch[index_cursor++] = tri->v1;
					g_d3dIndexScratch[index_cursor++] = tri->v2;
					g_d3dIndexScratch[index_cursor++] = tri->v3;
					run->index_count += 3;
				}
				break;
			case D3DOP_EXIT:
				exited = 1;
				break;
			default:
				Aeron_LogWarn("xwa.d3d", "Unexpected execute-buffer opcode %u", insn->bOpcode);
				break;
		}
		ip = payload + (uint32_t)insn->wCount * insn->bSize;
	}

	/* Persist the evolved state for the next Execute (state may change even when
	 * this buffer emits no triangles). */
	d->render_state = state;
	d->cur_texture_handle = texture_handle;

	if (run_count == 0 || index_cursor == 0) {
		return 1;
	}

	segment = d->segment;
	if (!segment || segment->vertex_count > (uint32_t)INT32_MAX ||
		buf->vertex_count > (uint32_t)INT32_MAX - segment->vertex_count ||
		index_cursor > UINT32_MAX - segment->index_count || run_count > UINT32_MAX - segment->draw_count) {
		return 0;
	}
	vertex_base = segment->vertex_count;
	index_base = segment->index_count;
	draw_base = segment->draw_count;
	if (vertex_base + buf->vertex_count > UINT32_MAX / (uint32_t)sizeof(D3DGpuVertex) ||
		index_base + index_cursor > UINT32_MAX / (uint32_t)sizeof(uint16_t)) {
		return 0;
	}
	if (!D3DCompat_EnsureSegmentStorage(segment, vertex_base + buf->vertex_count, index_base + index_cursor,
										draw_base + run_count)) {
		return 0;
	}

	for (i = 0; i < buf->vertex_count; ++i) {
		D3DCompat_ConvertVertex(&segment->vertices[vertex_base + i], &verts[i]);
	}
	memcpy(&segment->indices[index_base], g_d3dIndexScratch, (size_t)index_cursor * sizeof(uint16_t));
	D3DCompat_GetViewportUniform(d, &vp_uniform);

	for (i = 0; i < run_count; ++i) {
		const D3DRun* run = &g_d3dRuns[i];
		D3DSceneDraw* draw = &segment->draws[draw_base + i];

		draw->state = run->state;
		draw->pipeline = D3DCompat_GetPipeline(AERON_TEXTURE_FORMAT_RGBA8_UNORM,
											   AERON_TEXTURE_FORMAT_D16_UNORM, &run->state);
		draw->sampler = D3DCompat_GetSampler(&run->state);
		draw->texture = D3DCompat_TextureForHandle(d, run->texture_handle);
		if (!draw->pipeline || !draw->sampler || !draw->texture) {
			return 0;
		}
		draw->viewport = vp_uniform;
		draw->first_index = index_base + run->first_index;
		draw->index_count = run->index_count;
		draw->vertex_offset = (int32_t)vertex_base;
	}
	segment->vertex_count = vertex_base + buf->vertex_count;
	segment->index_count = index_base + index_cursor;
	segment->draw_count = draw_base + run_count;
	return 1;
}

/* --- IDirect3DDevice ----------------------------------------------------- */

static HRESULT XWA_DXAPI D3DDevice_QueryInterface(IDirect3DDevice* self, DxRefIid iid, void** out) {
	(void)self;
	(void)iid;
	if (out) {
		*out = NULL;
	}
	return DX_E_NOTIMPL;
}

static uint32_t XWA_DXAPI D3DDevice_AddRef(IDirect3DDevice* self) {
	return (uint32_t)++((D3DDeviceShim*)self)->refcount;
}

static uint32_t XWA_DXAPI D3DDevice_Release(IDirect3DDevice* self) {
	D3DDeviceShim* d = (D3DDeviceShim*)self;
	if (--d->refcount > 0) {
		return (uint32_t)d->refcount;
	}
	(void)D3DCompat_FlushSceneSegment(d);
	if (d->target && d->target->device == d) {
		d->target->device = NULL;
	}
	D3DCompat_DestroySceneSegment(d->segment);
	free(d->handles);
	free(d);
	return 0;
}

static HRESULT XWA_DXAPI D3DDevice_GetCaps(IDirect3DDevice* self, void* hw, void* hel) {
	(void)self;
	(void)hw;
	(void)hel;
	return DX_DD_OK;
}

static HRESULT XWA_DXAPI D3DDevice_CreateExecuteBuffer(IDirect3DDevice* self, D3DEXECUTEBUFFERDESC* desc,
													   IDirect3DExecuteBuffer** out, void* outer) {
	D3DExecBufShim* b;
	(void)self;
	(void)outer;

	if (!desc || !out) {
		return DX_E_INVALIDARG;
	}
	*out = NULL;
	b = (D3DExecBufShim*)calloc(1, sizeof(*b));
	if (!b) {
		return DX_E_FAIL;
	}
	b->size = desc->dwBufferSize;
	b->data = b->size ? malloc(b->size) : NULL;
	if (b->size && !b->data) {
		free(b);
		return DX_E_FAIL;
	}
	b->lpVtbl = &g_d3dExecBufVtbl;
	b->refcount = 1;
	*out = (IDirect3DExecuteBuffer*)b;
	return DX_DD_OK;
}

static HRESULT XWA_DXAPI D3DDevice_Execute(IDirect3DDevice* self, IDirect3DExecuteBuffer* buffer,
										   IDirect3DViewport* viewport, uint32_t flags) {
	D3DDeviceShim* d = (D3DDeviceShim*)self;
	(void)viewport;
	(void)flags;

	if (!buffer) {
		return DX_E_INVALIDARG;
	}
	if (!D3DCompat_ExecuteBuffer(d, (D3DExecBufShim*)buffer)) {
		Aeron_RequestFatalRendererError("Direct3D execute-buffer recording");
		return DX_E_FAIL;
	}
	return DX_DD_OK;
}

static HRESULT XWA_DXAPI D3DDevice_AddViewport(IDirect3DDevice* self, IDirect3DViewport* viewport) {
	D3DDeviceShim* d = (D3DDeviceShim*)self;
	d->viewport = (D3DViewportShim*)viewport;
	return DX_DD_OK;
}

static HRESULT XWA_DXAPI D3DDevice_DeleteViewport(IDirect3DDevice* self, IDirect3DViewport* viewport) {
	D3DDeviceShim* d = (D3DDeviceShim*)self;
	if (d->viewport == (D3DViewportShim*)viewport) {
		d->viewport = NULL;
	}
	return DX_DD_OK;
}

/* DX5 texture-format enumeration callback: __stdcall, (DDSURFACEDESC*, lParam),
 * returns D3DENUMRET_OK(1) to continue or 0 to stop (see std3D_EnumTextureFormats
 * @0x5995F0). */
typedef HRESULT(XWA_DXAPI* D3DEnumTextureFormatsCb)(DDSURFACEDESC*, void*);

static int D3DCompat_ReportTextureFormat(D3DEnumTextureFormatsCb cb, void* ctx, uint32_t flags, uint32_t bits,
										 uint32_t r, uint32_t g, uint32_t b, uint32_t a) {
	DDSURFACEDESC desc;

	memset(&desc, 0, sizeof(desc));
	desc.dwSize = 108;
	desc.dwFlags = DDSD_PIXELFORMAT | DDSD_CAPS;
	desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE;
	desc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
	desc.ddpfPixelFormat.dwFlags = flags;
	desc.ddpfPixelFormat.dwRGBBitCount = bits;
	desc.ddpfPixelFormat.dwRBitMask = r;
	desc.ddpfPixelFormat.dwGBitMask = g;
	desc.ddpfPixelFormat.dwBBitMask = b;
	desc.ddpfPixelFormat.dwRGBAlphaBitMask = a;
	return cb(&desc, ctx) != 0;
}

static HRESULT XWA_DXAPI D3DDevice_EnumTextureFormats(IDirect3DDevice* self, void* callback, void* ctx) {
	D3DEnumTextureFormatsCb cb = (D3DEnumTextureFormatsCb)callback;
	(void)self;

	if (!cb) {
		return DX_E_INVALIDARG;
	}
	/* RGB565, ARGB1555, ARGB4444, and 8-bit palettized -- the formats std3D
	 * searches for (g_cfRGB565 / g_cfRGBA1555 / g_cfRGBA4444 / g_cfPal8). The
	 * callback stops early if it returns 0. */
	if (!D3DCompat_ReportTextureFormat(cb, ctx, DDPF_RGB, 16, 0xF800, 0x07E0, 0x001F, 0) ||
		!D3DCompat_ReportTextureFormat(cb, ctx, DDPF_RGB | DDPF_ALPHAPIXELS, 16, 0x7C00, 0x03E0, 0x001F,
									   0x8000) ||
		!D3DCompat_ReportTextureFormat(cb, ctx, DDPF_RGB | DDPF_ALPHAPIXELS, 16, 0x0F00, 0x00F0, 0x000F,
									   0xF000) ||
		!D3DCompat_ReportTextureFormat(cb, ctx, DDPF_RGB | DDPF_PALETTEINDEXED8, 8, 0, 0, 0, 0)) {
		return DX_DD_OK;
	}
	return DX_DD_OK;
}

/* Captures the state that opening the original render pass observed. GPU commands
 * remain deferred until an ordering boundary, but CPU staging writeback and depth
 * clear consumption retain their BeginScene/first-Execute timing. */
static int D3DCompat_StartSceneSegment(D3DDeviceShim* d) {
	DDrawSurfaceShim* rt = d->target;

	if (!rt || !rt->rt) {
		return 0;
	}
	if (d->segment_pending) {
		return 1;
	}
	DDShim_WritebackRenderTarget(rt);

	if (d->viewport) {
		d->segment_viewport =
			(AeronRectI) { d->viewport->x, d->viewport->y, d->viewport->width, d->viewport->height };
	} else {
		d->segment_viewport = (AeronRectI) { 0, 0, rt->width, rt->height };
	}

	d->segment_clear_depth = rt->pending_depth_clear && rt->depth != NULL;
	d->segment_clear_depth_value = rt->pending_depth_clear_value;
	rt->pending_depth_clear = 0;
	d->segment_pending = 1;
	return 1;
}

static int D3DCompat_RecordSceneDraws(D3DDeviceShim* d, AeronRenderPass* pass) {
	D3DSceneSegment* segment = d->segment;
	AeronGraphicsPipeline* bound_pipeline = NULL;
	AeronSampler* bound_sampler = NULL;
	AeronTexture* bound_texture = NULL;
	D3DViewportUniform bound_viewport;
	D3DFragmentUniform bound_fragment;
	int have_viewport = 0;
	int have_fragment = 0;
	uint32_t i;

	if (segment->draw_count == 0) {
		return 1;
	}
	Aeron_BindVertexBuffer(pass, 0, segment->vertex_buffer, 0);
	Aeron_BindIndexBuffer(pass, segment->index_buffer, AERON_INDEX_FORMAT_UINT16, 0);

	for (i = 0; i < segment->draw_count; ++i) {
		const D3DSceneDraw* draw = &segment->draws[i];
		D3DFragmentUniform fragment;

		if (bound_pipeline != draw->pipeline) {
			Aeron_BindGraphicsPipeline(pass, draw->pipeline);
			bound_pipeline = draw->pipeline;
		}
		if (!have_viewport || memcmp(&bound_viewport, &draw->viewport, sizeof(bound_viewport)) != 0) {
			Aeron_BindUniformData(pass, AERON_SHADER_STAGE_VERTEX, 0, &draw->viewport,
								  sizeof(draw->viewport));
			bound_viewport = draw->viewport;
			have_viewport = 1;
		}
		if (bound_texture != draw->texture || bound_sampler != draw->sampler) {
			Aeron_BindTextureSampler(pass, AERON_SHADER_STAGE_FRAGMENT, 0, draw->texture, draw->sampler);
			bound_texture = draw->texture;
			bound_sampler = draw->sampler;
		}

		fragment.params[0] = (float)draw->state.texture_blend;
		fragment.params[1] = (float)draw->state.alpha_test;
		fragment.params[2] = 1.0f;
		fragment.params[3] = 1.0f;
		if (!have_fragment || memcmp(&bound_fragment, &fragment, sizeof(bound_fragment)) != 0) {
			Aeron_BindUniformData(pass, AERON_SHADER_STAGE_FRAGMENT, 0, &fragment, sizeof(fragment));
			bound_fragment = fragment;
			have_fragment = 1;
		}
		Aeron_DrawIndexed(pass, draw->index_count, draw->first_index, draw->vertex_offset);
	}
	return 1;
}

static int D3DCompat_FlushSceneSegment(D3DDeviceShim* d) {
	D3DSceneSegment* segment;
	AeronCommandBuffer* command_buffer;
	AeronRenderPass* pass;
	AeronBufferUploadDesc uploads[2];
	uint32_t upload_count;
	int ok;

	if (!d || !d->segment_pending) {
		return 1;
	}
	segment = d->segment;
	if (!segment || !d->target || !d->target->rt) {
		Aeron_RequestFatalRendererError("Direct3D scene-segment validation");
		return 0;
	}
	if (segment->draw_count > 0 &&
		!D3DCompat_EnsureBuffers(segment, segment->vertex_count, segment->index_count)) {
		Aeron_RequestFatalRendererError("Direct3D scene buffer creation");
		return 0;
	}

	command_buffer = Aeron_AcquireCommandBuffer();
	if (!command_buffer) {
		Aeron_RequestFatalRendererError("Direct3D command-buffer acquisition");
		return 0;
	}
	upload_count = 0;
	if (segment->draw_count > 0) {
		uploads[upload_count++] = (AeronBufferUploadDesc) {
			.buffer = segment->vertex_buffer,
			.offset = 0,
			.data = segment->vertices,
			.size = segment->vertex_count * (uint32_t)sizeof(D3DGpuVertex),
		};
		uploads[upload_count++] = (AeronBufferUploadDesc) {
			.buffer = segment->index_buffer,
			.offset = 0,
			.data = segment->indices,
			.size = segment->index_count * (uint32_t)sizeof(uint16_t),
		};
		if (!Aeron_UploadBufferBatchCmd(command_buffer, uploads, upload_count)) {
			Aeron_CancelCommandBuffer(command_buffer);
			Aeron_RequestFatalRendererError("Direct3D scene buffer upload");
			return 0;
		}
	}

	pass = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
		.color_target = d->target->rt,
		.depth_target = d->target->depth,
		.viewport = d->segment_viewport,
		.scissor = d->segment_viewport,
		.clear_color = 0,
		.clear_depth = d->segment_clear_depth,
		.clear_depth_value = d->segment_clear_depth_value,
		.command_buffer = command_buffer,
		.debug_label = "XWA Direct3D scene segment",
	});
	if (!pass) {
		Aeron_CancelCommandBuffer(command_buffer);
		Aeron_RequestFatalRendererError("Direct3D render-pass creation");
		return 0;
	}
	ok = D3DCompat_RecordSceneDraws(d, pass);
	Aeron_EndRenderPass(pass);
	if (!ok) {
		Aeron_CancelCommandBuffer(command_buffer);
		Aeron_RequestFatalRendererError("Direct3D scene draw recording");
		return 0;
	}
	ok = Aeron_SubmitCommandBuffer(command_buffer);
	if (!ok) {
		Aeron_RequestFatalRendererError("Direct3D scene submission");
		return 0;
	}

	D3DCompat_ResetSceneSegment(segment);
	d->segment_pending = 0;
	d->segment_clear_depth = 0;
	d->target->gpu_dirty = 1;
	return 1;
}

static HRESULT XWA_DXAPI D3DDevice_BeginScene(IDirect3DDevice* self) {
	D3DDeviceShim* d = (D3DDeviceShim*)self;

	if (!d->target || !d->target->rt || d->scene_active || d->segment_pending) {
		Aeron_RequestFatalRendererError("Direct3D scene begin");
		return DX_E_FAIL;
	}
	d->scene_active = 1;
	if (DDrawCompat_IsClassicFlightRenderingSuppressed()) {
		/* Retain the deferred clear for the first GPU pass after suppression. */
		return DX_DD_OK;
	}
	if (!D3DCompat_StartSceneSegment(d)) {
		d->scene_active = 0;
		Aeron_RequestFatalRendererError("Direct3D scene-segment start");
		return DX_E_FAIL;
	}
	return DX_DD_OK;
}

static HRESULT XWA_DXAPI D3DDevice_EndScene(IDirect3DDevice* self) {
	D3DDeviceShim* d = (D3DDeviceShim*)self;
	if (!d->scene_active) {
		Aeron_RequestFatalRendererError("Direct3D scene end");
		return DX_E_FAIL;
	}
	if (!D3DCompat_FlushSceneSegment(d)) {
		return DX_E_FAIL;
	}
	d->scene_active = 0;
	return DX_DD_OK;
}

/* Publishes queued draws before a DirectDraw operation that observes or modifies
 * the target. The scene stays active, so the next Execute starts a new segment. */
int D3DCompat_FlushRenderTargetPass(DDrawSurfaceShim* s) {
	D3DDeviceShim* d = s ? s->device : NULL;
	return D3DCompat_FlushSceneSegment(d);
}

static const IDirect3DDeviceVtbl g_d3dDeviceVtbl = {
	.QueryInterface = D3DDevice_QueryInterface,
	.AddRef = D3DDevice_AddRef,
	.Release = D3DDevice_Release,
	.GetCaps = D3DDevice_GetCaps,
	.CreateExecuteBuffer = D3DDevice_CreateExecuteBuffer,
	.Execute = D3DDevice_Execute,
	.AddViewport = D3DDevice_AddViewport,
	.DeleteViewport = D3DDevice_DeleteViewport,
	.EnumTextureFormats = D3DDevice_EnumTextureFormats,
	.BeginScene = D3DDevice_BeginScene,
	.EndScene = D3DDevice_EndScene,
};

/* --- IDirect3D ----------------------------------------------------------- */

static HRESULT XWA_DXAPI D3D_QueryInterface(IDirect3D* self, DxRefIid iid, void** out) {
	(void)self;
	(void)iid;
	if (out) {
		*out = NULL;
	}
	return DX_E_NOTIMPL;
}

static uint32_t XWA_DXAPI D3D_AddRef(IDirect3D* self) { return (uint32_t)++((D3DShim*)self)->refcount; }

static uint32_t XWA_DXAPI D3D_Release(IDirect3D* self) {
	D3DShim* d = (D3DShim*)self;
	if (--d->refcount > 0) {
		return (uint32_t)d->refcount;
	}
	free(d);
	return 0;
}

static HRESULT XWA_DXAPI D3D_CreateViewport(IDirect3D* self, IDirect3DViewport** out, void* outer) {
	D3DViewportShim* v;
	(void)self;
	(void)outer;

	if (!out) {
		return DX_E_INVALIDARG;
	}
	*out = NULL;
	v = (D3DViewportShim*)calloc(1, sizeof(*v));
	if (!v) {
		return DX_E_FAIL;
	}
	v->lpVtbl = &g_d3dViewportVtbl;
	v->refcount = 1;
	*out = (IDirect3DViewport*)v;
	return DX_DD_OK;
}

/* Enumerate the single synthesized RGB hardware device. The capability bits are
 * exactly those std3D_EnumDevicesCallback @0x5991CC reads to accept a device:
 * perspective + alpha + color-key textures, clamp addressing, linear + mipmap
 * filtering, alpha-blend shading, a 16bpp Z buffer, and all depth-compare funcs.
 * The device guid is the HAL guid the render surface's QueryInterface accepts. */
static HRESULT XWA_DXAPI D3D_EnumDevices(IDirect3D* self, D3DEnumDevicesCb cb, void* ctx) {
	D3DDEVICEDESC hw;
	DxGuid guid = IID_IDirect3DHALDevice_Compat;
	char name[] = "Aeron HAL";
	char desc[] = "Aeron Direct3D HAL device";
	(void)self;

	if (!cb) {
		return DX_E_INVALIDARG;
	}
	memset(&hw, 0, sizeof(hw));
	hw.dwSize = sizeof(hw);
	hw.dcmColorModel = 2; /* D3DCOLOR_RGB */
	hw.dpcTriCaps.dwSize = sizeof(D3DPRIMCAPS);
	hw.dpcTriCaps.dwTextureCaps = 0x0D;        /* PERSPECTIVE(1) | ALPHA(4) | TRANSPARENCY/colorkey(8) */
	hw.dpcTriCaps.dwTextureAddressCaps = 0x05; /* WRAP(1) | CLAMP(4) */
	hw.dpcTriCaps.dwTextureFilterCaps = 0x1A;  /* LINEAR(2) | MIPNEAREST(8) | MIPLINEAR(0x10) */
	hw.dpcTriCaps.dwShadeCaps = 0x4000;        /* alpha gouraud blend */
	hw.dpcTriCaps.dwTextureBlendCaps = 0x08;   /* alpha */
	hw.dpcTriCaps.dwZCmpCaps = 0xFF;           /* all compare funcs */
	hw.dwDeviceRenderBitDepth = 0x400;         /* DDBD_16 */
	hw.dwDeviceZBufferBitDepth = 0x400;        /* DDBD_16 */
	hw.dwMaxBufferSize = 0x10000;
	hw.dwMaxVertexCount = 8192;
	cb(&guid, desc, name, &hw, &hw, ctx);
	return DX_DD_OK;
}

static const IDirect3DVtbl g_d3dVtbl = {
	.QueryInterface = D3D_QueryInterface,
	.AddRef = D3D_AddRef,
	.Release = D3D_Release,
	.EnumDevices = D3D_EnumDevices,
	.CreateViewport = D3D_CreateViewport,
};

/* --- shim entry points (called from ddraw_compat.c QueryInterface) ------- */

IDirect3DTexture* D3DCompat_CreateTexture(DDrawSurfaceShim* surface) {
	D3DTextureShim* t = (D3DTextureShim*)calloc(1, sizeof(*t));
	if (!t) {
		return NULL;
	}
	t->lpVtbl = &g_d3dTextureVtbl;
	t->refcount = 1;
	t->surface = surface;
	return (IDirect3DTexture*)t;
}

IDirect3D* D3DCompat_CreateD3D(DDrawShim* dd) {
	D3DShim* d = (D3DShim*)calloc(1, sizeof(*d));
	if (!d) {
		return NULL;
	}
	d->lpVtbl = &g_d3dVtbl;
	d->refcount = 1;
	d->owner = dd;
	return (IDirect3D*)d;
}

/* Promote a DirectDraw surface to a render target (create its Aeron color+depth
 * targets) and bind a fresh IDirect3DDevice to it. */
IDirect3DDevice* D3DCompat_CreateDeviceForSurface(DDrawSurfaceShim* surface) {
	D3DDeviceShim* d;

	if (!surface) {
		return NULL;
	}
	if (!surface->rt) {
		surface->rt = Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) {
			.width = surface->width,
			.height = surface->height,
			.format = AERON_TEXTURE_FORMAT_RGBA8_UNORM,
		});
		if (!surface->rt) {
			return NULL;
		}
		/* Second half of the flip chain: present swaps rt<->rt_back so the composed
		 * frame stays intact while the next frame's background restore / 3D targets
		 * the other buffer. */
		surface->rt_back = Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) {
			.width = surface->width,
			.height = surface->height,
			.format = AERON_TEXTURE_FORMAT_RGBA8_UNORM,
		});
		if (!surface->rt_back) {
			Aeron_DestroyRenderTarget(surface->rt);
			surface->rt = NULL;
			return NULL;
		}
		surface->depth = Aeron_CreateDepthTarget(&(AeronDepthTargetDesc) {
			.width = surface->width,
			.height = surface->height,
			.format = AERON_TEXTURE_FORMAT_D16_UNORM,
		});
		if (!surface->depth) {
			Aeron_DestroyRenderTarget(surface->rt);
			Aeron_DestroyRenderTarget(surface->rt_back);
			surface->rt = NULL;
			surface->rt_back = NULL;
			return NULL;
		}
		surface->kind = DDSHIM_RENDER_TARGET;
	}

	d = (D3DDeviceShim*)calloc(1, sizeof(*d));
	if (!d) {
		return NULL;
	}
	d->lpVtbl = &g_d3dDeviceVtbl;
	d->refcount = 1;
	d->target = surface;
	d->segment = (D3DSceneSegment*)calloc(1, sizeof(*d->segment));
	if (!d->segment) {
		free(d);
		return NULL;
	}
	d->render_state = g_d3dDefaultRenderState;
	d->cur_texture_handle = 0;
	surface->device = d;
	return (IDirect3DDevice*)d;
}

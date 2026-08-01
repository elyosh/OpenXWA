#ifndef XWA_RUNTIME_SNAPSHOT_H
#define XWA_RUNTIME_SNAPSHOT_H

/*
 * XwaSnapshot — per-tick POD game-state snapshot for the remaster
 * driver.
 *
 * Double-buffered slots: emitters fill the write slot during
 * XwaPort_Tick; XwaSnapshot_Commit() flips it, after which
 * XwaSnapshot_Current()/Previous() expose stable read-only views for
 * the remaster driver's interpolation. Float-only 3D, no engine
 * pointers, stable ids, fixed caps with logged truncation.
 *
 * Determinism rule: emitters are read-only observers on the sim; no
 * sim or net code may branch on snapshot state (lockstep-safe).
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- scene kind ---------------------------------------------------- */

typedef enum XwaSceneKind {
	XWA_SCENE_NONE = 0,
	XWA_SCENE_FRONTEND,       /* concourse / menus / briefing / film room */
	XWA_SCENE_FLIGHT,         /* in-mission simulation (incl. hangar / DS) */
	XWA_SCENE_CUTSCENE,       /* SMUSH playback */
	XWA_SCENE_LOADING,        /* flight mission loading UI on attached frontend surfaces */
	XWA_SCENE_FRONTEND_MODAL, /* frontend UI temporarily attached to an active flight */
} XwaSceneKind;

/* ---- 2D draw records (frontend channel) ----------------------------
 * One record per FrontImage_Draw* / FrontendDraw_* call, in call
 * order. `z_order` is the tick-local monotonic emit counter so the
 * remaster driver can cross-merge channels; `target` routes records
 * drawn into a scratch surface (FrontendDraw_BeginExternalSurface)
 * away from the main screen. */

typedef enum XwaEmitTarget {
	XWA_EMIT_TARGET_MAIN = 0,      /* frontend back buffer */
	XWA_EMIT_TARGET_EXTERNAL = 1,  /* external/scratch surface (brief map etc.) */
	XWA_EMIT_TARGET_OFFSCREEN = 2, /* offscreen (persistent screen) surface */
} XwaEmitTarget;

/* Surface-level events, z-ordered with the draw records so the
 * reconstruction can mirror the engine's surface structure instead of
 * its (unhookable) pixel copies:
 *   OFFSCREEN_RESTORE : offscreen -> back full copy (per-present wipe
 *                       of transients: cursor, hover label, cels).
 *   BACKBUFFER_SAVE   : back -> offscreen full copy (promote the
 *                       composed frame to persistent).
 *   SCREEN_PUSH_SAVE  : screen-stack push saved `rect` of the
 *                       persistent surface (dialog about to open).
 *   SCREEN_POP_RESTORE: screen-stack pop restored that saved region.
 */
typedef enum XwaSurfaceEventKind {
	XWA_SURFACE_EVENT_OFFSCREEN_RESTORE = 0,
	XWA_SURFACE_EVENT_BACKBUFFER_SAVE = 1,
	XWA_SURFACE_EVENT_SCREEN_PUSH_SAVE = 2,
	XWA_SURFACE_EVENT_SCREEN_POP_RESTORE = 3,
	/* The classic frame was submitted for display at this z position.
	 * Restores after it belong to the NEXT frame (the engine cleans
	 * the back buffer post-flip); the reconstruction must show the
	 * state as of this event. Emitted by the DirectDraw shim. */
	XWA_SURFACE_EVENT_PRESENT = 4,
	/* External (scratch-surface) structure: CLEAR wipes the external
	 * RT (classic BriefingMap_ClearPreviewScratchRect); COMPOSITE_
	 * REVEAL copies the external RT's non-transparent texels over the
	 * main surface OUTSIDE the reveal rect — the L-shaped remainder of
	 * `rect` beyond aux0 (reveal width) x aux1 (reveal height) texels
	 * from its top-left (the briefing wireframe-over-model effect). */
	XWA_SURFACE_EVENT_EXTERNAL_CLEAR = 5,
	XWA_SURFACE_EVENT_EXTERNAL_COMPOSITE_REVEAL = 6,
	/* Full-surface colorfill clears (FrontendDisplay_Clear*); aux0 is
	 * the engine 16bpp fill color. */
	XWA_SURFACE_EVENT_BACKBUFFER_CLEAR = 7,
	XWA_SURFACE_EVENT_OFFSCREEN_CLEAR = 8,
} XwaSurfaceEventKind;

typedef struct XwaSurfaceEvent {
	uint32_t z_order;
	uint8_t kind;                     /* XwaSurfaceEventKind */
	int16_t aux0, aux1;               /* kind-specific (COMPOSITE_REVEAL: reveal w/h) */
	int16_t left, top, right, bottom; /* inclusive; push/pop rects */
} XwaSurfaceEvent;

typedef enum XwaDraw2DKind {
	XWA_DRAW2D_SPRITE = 0,                  /* FrontImage_DrawSprite (transparent) */
	XWA_DRAW2D_SPRITE_OPAQUE,               /* FrontImage_DrawSpriteOpaque */
	XWA_DRAW2D_SPRITE_TRANSLUCENT,          /* FrontImage_DrawSpriteTranslucent */
	XWA_DRAW2D_SPRITE_RECT,                 /* FrontImage_DrawSpriteRectTransparent */
	XWA_DRAW2D_SPRITE_RECT_TINTED,          /* FrontImage_DrawSpriteRectTinted */
	XWA_DRAW2D_SPRITE_RECT_ORIENTED,        /* FrontImage_DrawSpriteRectOriented */
	XWA_DRAW2D_SPRITE_RECT_TINTED_ORIENTED, /* ...RectTintedOriented */
	XWA_DRAW2D_ATLAS_SPRITE,                /* FrontImage_DrawAtlasSprite */
} XwaDraw2DKind;

#define XWA_SNAP_SPRITE_NAME_MAX 24
#define XWA_SNAP_FRONTEND_FILE_MAX 40
#define XWA_SNAP_FRONTEND_SOURCE_MAX 256
#define XWA_SNAP_SPRITE_FILE_MAX XWA_SNAP_FRONTEND_FILE_MAX

typedef struct XwaDraw2D {
	uint32_t z_order;
	uint8_t kind;   /* XwaDraw2DKind */
	uint8_t target; /* XwaEmitTarget */
	uint8_t has_src_rect;
	/* Sprite identity: catalog name for the name-keyed family;
	 * group/index for the atlas path (name empty). */
	char name[XWA_SNAP_SPRITE_NAME_MAX];
	/* Source-file identity resolved from the name at emit time
	 * ("<dir>/<base>" under FRONTRES, normalized lowercase, no
	 * extension — e.g. "family/familyroom"). The engine binds names to
	 * files at runtime ("background" is a different file per room), so
	 * only the file key addresses a baked modern asset. Empty when the
	 * name has no recorded binding. */
	char file[XWA_SNAP_SPRITE_FILE_MAX];
	/* Original source path captured with the binding. The HD fallback reads
	 * this file independently through Aeron VFS. */
	char source_file[XWA_SNAP_FRONTEND_SOURCE_MAX];
	/* Classic image dims (image[frame] for named sprites, DAT sprite
	 * w/h for the atlas kind). Geometry authority for the HD draw:
	 * dst extents and src-rect UVs derive from these, so modern assets
	 * of any resolution map correctly. */
	int16_t img_w, img_h;
	int16_t frame;                 /* ResourceDescriptor.currentFrame at draw */
	uint16_t resource_frame_count; /* complete named resource at draw time */
	int16_t atlas_group;
	int16_t atlas_index;
	/* Source sub-rect (catalog-image pixels) when has_src_rect. */
	int16_t src_left, src_top, src_right, src_bottom;
	/* Destination in frontend coords (640x480 frame). */
	int16_t dst_x, dst_y;
	uint32_t tint_color;        /* raw engine color arg (tinted kinds) */
	uint32_t opaque_fill_color; /* palette entry 0 for FrontImage_DrawSpriteOpaque */
	int32_t orientation_mode;   /* raw orientation arg (oriented kinds) */
	/* Active screen clip rect (inclusive, frontend coords) at emit
	 * time — FrontendDisplay_Get/SetScreenClipRect640x480 state. */
	int16_t clip_left, clip_top, clip_right, clip_bottom;
} XwaDraw2D;

typedef enum XwaPaintKind {
	XWA_PAINT_HLINE = 0,        /* FrontendDraw_HorizontalLineClipped */
	XWA_PAINT_VLINE,            /* FrontendDraw_VerticalLineClipped */
	XWA_PAINT_LINE,             /* FrontendDraw_Line */
	XWA_PAINT_LINE_AA,          /* FrontendDraw_LineAntialiased */
	XWA_PAINT_FILL_TRANSLUCENT, /* FrontendDraw_FillRectTranslucent */
	XWA_PAINT_FILL_RECT,        /* FrontendDraw_Rect (filled): opaque solid */
	XWA_PAINT_RECT_OUTLINE,     /* FrontendDraw_RectOutline / Rect (unfilled) */
	XWA_PAINT_PIXEL,            /* single raw-color pixel (concourse stars) */
} XwaPaintKind;

typedef struct XwaPaintCmd {
	uint32_t z_order;
	uint8_t kind;   /* XwaPaintKind */
	uint8_t target; /* XwaEmitTarget */
	/* Lines: (x0,y0)-(x1,y1). HLINE: x0..x1 at y0. VLINE: y0..y1 at
	 * x0. FILL_TRANSLUCENT: rect x0,y0..x1,y1 offset by (dx,dy). */
	int16_t x0, y0, x1, y1;
	int16_t dx, dy;
	uint32_t color; /* raw engine color arg */
	int16_t clip_left, clip_top, clip_right, clip_bottom;
} XwaPaintCmd;

/* One bitmap-font glyph draw from the FrontendText_Draw* family's
 * per-string loops. Glyph-level capture makes engine layout (wrapping,
 * centering, reveal, scroll clip) exact by construction; the remaster
 * driver substitutes an HD glyph of the same font size at the same
 * position. `color` is the resolved inline color (after 0x01 reset /
 * 0x02..0x07 color-code handling). */
typedef struct XwaGlyph2D {
	uint32_t z_order;
	uint8_t target; /* XwaEmitTarget */
	uint8_t ch;
	int16_t font_size; /* point-size key of g_fontBySize */
	int16_t x, y;      /* frontend coords (640x480 frame) */
	uint32_t color;    /* raw engine color arg */
	int16_t clip_left, clip_top, clip_right, clip_bottom;
} XwaGlyph2D;

/* One frontend 3D model preview (tech library / mission briefing).
 * The remaster driver renders it as a scene PiP over the drawlist.
 * Pose follows the engine's Q16 angle convention (full circle =
 * 65536); world coords are raw engine ints. */
typedef struct XwaModelPreview {
	uint32_t z_order;
	uint8_t target;            /* XwaEmitTarget */
	uint8_t wireframe;         /* RenderWireframeViewport path */
	uint16_t pitch, yaw, roll; /* Q16 angles */
	uint16_t angle_d;          /* ModelPreview_SetObjectAngleDDegrees */
	/* OPT file BASENAME (no directories — path components made long
	 * names truncate, e.g. CorellianTransport2Exterior.opt). */
	char opt_name[48];
	int32_t world_x, world_y, world_z;
	int32_t node_switch_index;
	int16_t dst_x, dst_y, dst_w, dst_h; /* clipped viewport rect */
	uint32_t line_color;                /* wireframe only */
	/* Engine-computed transform, captured AFTER FVIEW_SetObjectTransform
	 * + view rotation: eye = obj_basis(rows R0,R1,R2) * v_model +
	 * eye_delta; classic projection is screen = center + 512 * xy/z in
	 * viewport px (g_projScale). Model space is OPT-native (+Z up, -Y
	 * forward). Carrying the resolved basis avoids re-deriving the
	 * engine's Euler/angle_d conventions in the driver. */
	float obj_basis[9]; /* g_objViewMatF R0,R1,R2 row-major */
	float eye_delta[3]; /* g_modelPreviewViewDelta after view rotation */
	/* ModelPreview_LoadModel normalizes the loaded OPT's vertices in
	 * place: (500 / maxExtent) * {1.5, 1.8, 2.1} by aspect class. The
	 * driver's cooked mesh has RAW vertices, so it applies this. */
	float model_scale;
	/* This view's camera world->eye rotation (g_camMat Q15 rows R0,R1,R2
	 * scaled to float): eye = (R0.w, R1.w, R2.w). The preview is its own
	 * 3D view with its own camera (flight views carry the same matrix in
	 * flight_camera.rows); the driver rotates the WORLD-space dir_lights
	 * channel into eye space with it. */
	float cam_rows[9];
} XwaModelPreview;

/* ---- directional lights (shared 3D lighting state) ------------------
 * The engine's global FlightLight directional table, sampled at Commit.
 * The SAME table lights every 3D view — flight scenes (backdrop suns)
 * and the frontend model preview (its fixed studio light) — so this is
 * a top-level channel, not a per-record field. world_dir is the
 * surface->light unit direction in engine WORLD space (Q15 scaled to
 * float); shading in the original is sum(color * intensity * max(0,
 * N.L)) per vertex with overbright cross-channel bleed then clamp. */
typedef struct XwaDirLight {
	float world_dir[3]; /* surface -> light, unit */
	float intensity;
	float color[3]; /* r, g, b in [0,1] */
	/* ObjectTypeId of the source backdrop, or 0 for synthetic/studio
	 * lights. Sun types 521..530 are also the lens-flare sources. */
	uint16_t source_backdrop_model_type;
} XwaDirLight;

#define XWA_SNAP_MAX_DIR_LIGHTS 8

/* Engine glows are STATE-DERIVED: the HD driver rebuilds the classic
 * EngineGlow_BuildProjectedQuad geometry from the cooked glb's
 * engine-glow extras (position/axes/dims/colors per emitter) + the
 * per-object eg_* craft state below; no draw-time capture exists. */

/* ---- flight channel ------------------------------------------------
 * One record per live g_objectTable slot, captured once per host tick
 * after the flight task runs through a read-only walk.
 * Current and previous positions are exact raw int32 engine world units;
 * renderers subtract a view origin before converting them to float.
 * orientation ships BOTH the Q16 Euler angles and the mobile object's
 * cached Q15 basis rows (side, fwd, up) + the dirty flag, so the
 * remaster driver can pick per state. NOTE the engine's view compose
 * uses the (R0, R2, R1) row arrangement (FVIEW_ComputeObjectViewMatrix)
 * — that hazard lives in the driver's math, not in this data. */

/* CraftData carries 50 per-mesh slots. */
#define XWA_SNAP_MAX_MESH_SLOTS 50

/* Genus values the driver dispatches on (mirrors ModelGenusId — part
 * of the captured records' vocabulary, so drivers need no engine
 * headers). */
#define XWA_SNAP_GENUS_FIGHTER 0
#define XWA_SNAP_GENUS_TRANSPORT 1
#define XWA_SNAP_GENUS_UTILITY 2
#define XWA_SNAP_GENUS_FREIGHTER 3
#define XWA_SNAP_GENUS_STARSHIP 4
#define XWA_SNAP_GENUS_PLATFORM 5
#define XWA_SNAP_GENUS_PLAYER_PROJECTILE 6
#define XWA_SNAP_GENUS_NPC_PROJECTILE 7
#define XWA_SNAP_GENUS_MINE 8
#define XWA_SNAP_GENUS_SATELLITE_BUOY 9
#define XWA_SNAP_GENUS_ASTEROID 10
#define XWA_SNAP_GENUS_DEBRIS 11
#define XWA_SNAP_GENUS_EXPLOSION 13
#define XWA_SNAP_GENUS_LARGE_SCENERY 14
#define XWA_SNAP_GENUS_DS_TUNNEL 15
#define XWA_SNAP_GENUS_CONTAINER 17
#define XWA_SNAP_GENUS_PILOT_DROID 18
#define XWA_SNAP_GENUS_WEAPON_EMPLACEMENT 19
#define XWA_SNAP_GENUS_RUBBLE 20
#define XWA_SNAP_GENUS_SALVAGE_JUNK 21

/* Object-table slot classes (XwaFlightObject.slot_class). */
#define XWA_SNAP_SLOT_OTHER 0     /* static/region ranges outside main+transient */
#define XWA_SNAP_SLOT_MAIN 1      /* [g_regionMainObjectSlotStart, End) */
#define XWA_SNAP_SLOT_TRANSIENT 2 /* [g_localTransientSlotStart, End) */

/* Object-type values the drivers dispatch on (mirrors ObjectTypeId). */
#define XWA_SNAP_TYPE_BWING 4               /* OBJ_BWing (model-wide bridge compensation) */
#define XWA_SNAP_TYPE_FALCON2 59            /* OBJ_MilleniumFalcon2 (DS hangar light gate) */
#define XWA_SNAP_TYPE_SSD 140               /* OBJ_SuperStarDestroyer */
#define XWA_SNAP_TYPE_DS_REACTOR 324        /* OBJ_DSReactorCylinder (point-light gate) */
#define XWA_SNAP_TYPE_DEBRIS_CHUNK 222      /* OBJ_NoAsset_222 (hull piece; draws the SOURCE craft model) */
#define XWA_SNAP_TYPE_HANGAR_ROOF_CRANE 316 /* excluded from the classic hangar shadow pass */
#define XWA_SNAP_TYPE_DEBRIS_SPRITE_0 233   /* OBJ_DebrisTextureGroup4000 */
#define XWA_SNAP_TYPE_DEBRIS_SPRITE_3 236   /* OBJ_DebrisTextureGroup4003 */
#define XWA_SNAP_TYPE_LIGHTING_1000 237     /* OBJ_LightingEffectTextureGroup1000 (glow / flare sprites) */
#define XWA_SNAP_TYPE_HYPER_TUNNEL 238      /* OBJ_AnimationTextureGroup3051 */
#define XWA_SNAP_TYPE_EXPLOSION_2000 264    /* OBJ_ExplosionTextureGroup2000 */
#define XWA_SNAP_TYPE_EXPLOSION_2006 270    /* OBJ_ExplosionTextureGroup2006 (capital-ship blast) */
#define XWA_SNAP_TYPE_FLAME_2008 279        /* OBJ_AnimationTextureGroup2008 (wreck fire) */
/* Backdrop sun sprite range (lens-flare sources; mirrors
 * OBJ_BackdropTextureGroup9001..9010). */
#define XWA_SNAP_TYPE_BACKDROP_SUN_FIRST 521
#define XWA_SNAP_TYPE_BACKDROP_SUN_LAST 530

typedef struct XwaFlightObject {
	uint16_t signature;   /* ObjectRecord.objectSignature (stable id) */
	uint16_t slot;        /* object-table index */
	uint16_t object_type; /* ObjectTypeId (model index) */
	uint8_t genus;        /* ModelGenusId */
	uint8_t fg_idx;       /* flight-group index */
	uint8_t region;
	int8_t iff; /* from MobileObject (0 when no mobj) */
	uint8_t team;
	uint8_t state; /* MobileObject.state */
	uint8_t motion_flags;
	/* MobileObject.nodeSwitchIndex — the OPT variant selector (mission
	 * sets it from the flight group's `markings`); maps to the cooked
	 * glb's KHR material variant. */
	uint8_t node_switch;
	uint8_t has_mobj;     /* mobj-derived fields valid */
	uint8_t orient_dirty; /* MobileObject.orientMatrixDirty */
	int16_t source_obj;   /* MobileObject.sourceObjIdx */
	uint16_t speed;       /* MobileObject.speed (raw) */
	/* ---- billboard-law state (SceneBillboard_QueueObjectTextured
	 * inputs; the HD driver derives explosion/debris/spark sprites from
	 * these). */
	uint8_t slot_class;          /* XWA_SNAP_SLOT_* — the render walk's slot-
								  * range dispatch (flight_view.c): MAIN slots
								  * take the genus switch; TRANSIENT slots
								  * queue as billboards for any genus (angleD
								  * forced 0, GENUS_Debris hidden in external
								  * view); STATIC-genus slots take the
								  * RenderNonCraftSceneObject path. */
	uint8_t type_specific_0;     /* ObjectRecord.typeSpecificByte[0]: sprite
								  * animation frame (billboard types; 0 = not
								  * drawn); 2*componentIdx for debris chunks. */
	uint16_t type_specific_w;    /* ObjectRecord.typeSpecificWord: type-2006
								  * view-depth pull-in (draw-order bias). */
	uint16_t source_object_type; /* MobileObject.sourceObjectType (SSD blast
								  * rule; debris-chunk model resolution). */
	int32_t instance_extent;     /* MobileObject.instanceExtent (spawn size;
								  * preserve the full engine value — billboard
								  * size narrows only after its classic scaling) */
	/* Debris-chunk spin pivot (MobileObject.renderOffset* — the detached
	 * component's focus point in model space, set at detach from the
	 * mesh descriptor). */
	float render_offset[3];
	/* Exact engine coordinates. Renderers subtract their frame/view origin
	 * before converting to float. */
	int32_t world_pos[3];
	int32_t prev_world_pos[3];          /* MobileObject.prevWorld* (== world_pos when !has_mobj) */
	uint16_t yaw, pitch, roll, angle_d; /* Q16 angles */
	int16_t rows[9];                    /* cached side, fwd, up (Q15), row-major */
	/* Debris/breakup spin (FVIEW_calcrotateorient applies it after the
	 * Euler compose; the cached rows above already include it). */
	int16_t spin_angle; /* Q16; 0 = none */
	float spin_axis[3];
	int32_t player_owner;
	/* ---- v2: craft component pools (CraftData; valid when has_craft).
	 * componentState drives damage-texture selection per mesh,
	 * mesh_rotation the articulated meshes (turrets, rotaries),
	 * component_hp the blown-off threshold; sfoil_state the s-foil
	 * open/close animation. */
	uint8_t has_craft;
	uint8_t sfoil_state;
	uint16_t carried_object_slot;      /* CraftData.carriedObjectIndex */
	uint16_t carried_object_signature; /* stable identity, 0 when absent */
	uint16_t system_flags;             /* CraftData.systemFlags */
	uint16_t subsystem_damage;         /* CraftData.subsystemDamage */
	uint8_t component_state[XWA_SNAP_MAX_MESH_SLOTS];
	uint8_t mesh_rotation[XWA_SNAP_MAX_MESH_SLOTS];
	uint8_t component_hp[XWA_SNAP_MAX_MESH_SLOTS];
	/* ---- engine-glow state (EngineGlow_RenderObjectGlows inputs; the
	 * HD driver derives the glow fans from these + the cooked glb's
	 * engine-glow extras). objectKind 5/6 use the speed-boost scale
	 * formula (speed vs eg_max_speed); everything else the power-
	 * margin formula (16 - redirects, engineOutputScale, throttle). */
	uint8_t object_kind;        /* CraftData.objectKind */
	uint8_t eg_working;         /* CraftData.workingSubsystems != 0 */
	uint8_t eg_shield_redirect; /* CraftData.shieldRedirect */
	uint8_t eg_laser_redirect;  /* CraftData.laserRedirect */
	uint8_t eg_beam_level;      /* CraftData.beamLevel */
	uint16_t eg_throttle;       /* CraftData.throttleSpeed */
	uint16_t eg_output_scale;   /* CraftData.engineOutputScale */
	uint16_t eg_max_speed;      /* CraftData.aiFlight.maxSpeedCache */
	/* Damage knockouts: bit N = emitter N destroyed (classic per-model
	 * emitter order == the cooked glb's glow list order; emitters past
	 * 32 keep drawing). */
	uint32_t eg_knockout_mask;
} XwaFlightObject;

#define XWA_SNAP_MAX_FLIGHT_OBJECTS 1664
#define XWA_SNAP_MAX_FLIGHT_MAP_OBJECTS XWA_SNAP_MAX_FLIGHT_OBJECTS
#define XWA_SNAP_FLIGHT_MAP_LABEL_BYTES 65535

typedef enum XwaFlightMapRenderKind {
	XWA_FLIGHT_MAP_RENDER_CRAFT = 0,
	XWA_FLIGHT_MAP_RENDER_PROJECTILE = 1,
	XWA_FLIGHT_MAP_RENDER_SCENE_OBJECT = 2,
} XwaFlightMapRenderKind;

typedef enum XwaFlightMapCullKind {
	XWA_FLIGHT_MAP_CULL_BOUNDS = 0,
	XWA_FLIGHT_MAP_CULL_SPHERE = 1,
} XwaFlightMapCullKind;

typedef struct XwaFlightMapObject {
	uint16_t flight_object_index;
	uint16_t label_offset;
	int32_t max_bounds_extent;
	int32_t box_extent;
	int16_t move_x;
	int16_t move_y;
	uint16_t icon_id;
	uint16_t range_value;
	uint8_t effective_iff;
	uint8_t render_kind;
	uint8_t cull_kind;
	uint8_t label_visible;
	uint8_t movement_visible;
	uint8_t box_visible;
	uint8_t box_color_index;
} XwaFlightMapObject;

typedef struct XwaFlightMapState {
	uint8_t active;
	uint8_t has_order_endpoint;
	uint16_t object_count;
	uint16_t current_target_slot;
	uint16_t current_target_signature;
	uint16_t label_bytes;
	int32_t order_endpoint_world[3];
	XwaFlightMapObject objects[XWA_SNAP_MAX_FLIGHT_MAP_OBJECTS];
	char labels[XWA_SNAP_FLIGHT_MAP_LABEL_BYTES];
} XwaFlightMapState;

/* Flight camera, captured with the objects. `rows` are the active modern
 * render camera's world->eye basis rows in R0, R1, R2 storage order. */
typedef struct XwaFlightCamera {
	int32_t world_pos[3]; /* viewState.savedTarget* (camera anchor) */
	float rows[9];
	uint16_t view_pitch, view_yaw, view_roll, view_angle_d;
	int32_t focus_obj_idx;
	/* The local player's own object slot — the hangar anchors the
	 * cockpit to it (space anchors to focus_obj_idx). */
	int32_t player_obj_idx;
	/* Hangar planar-shadow receiver plane and the launch-reference
	 * object excluded from the classic caster walk. Valid in_hangar. */
	int32_t hangar_floor_z;
	int32_t hangar_launch_ref_obj_idx;
	/* ---- v2: view/scene modes the driver switches pipelines on. */
	uint8_t external;         /* viewState.externalCameraActive */
	uint8_t cockpit_visible;  /* player cockpit shown (own record below) */
	uint8_t hyperspace_phase; /* player.hyperspacePhase */
	uint8_t region;           /* player region index */
	uint8_t map_mode;         /* player.mapCameraState */
	uint8_t film_overlay;     /* film playback with overlay camera */
	uint8_t in_hangar;        /* g_inHangarReady (hangar scene phase) */
	/* Mission hangar kind == Death Star: type-2006 explosion billboards
	 * skip the 1024 projected-size clamp there (classic flush rule). */
	uint8_t ds_hangar;
	/* Death Star tunnel turbolaser beam light (the region's
	 * g_deathStarTunnelLaserRegions state while a beam is active —
	 * classic: green, intensity 100000, cull 0x4000). */
	uint8_t ds_beam_active;
	int32_t ds_beam_world_pos[3];
	/* ---- classic projection (resolution-dependent focal): screen =
	 * vp_center + proj_scale * xy/z, y down (TRANSFM2_ProjectScreen*).
	 * Screen-space records (billboards) unproject through these. */
	float proj_scale;      /* g_projScale (512 at 640x480) */
	uint16_t vp_w, vp_h;   /* g_flightVpWidth/Height */
	int16_t vp_center_x;   /* g_flightVpCenterX */
	int16_t vp_center_y;   /* g_flightVpCenterY */
	int16_t proj_offset_y; /* g_projOffsetY (per-craft gunsight Y offset) */
	/* Flight display mode (g_screenWidth/Height) — the classic 4:3
	 * frame size the classic renderer presents at; the driver's
	 * overlay rect uses it. */
	uint16_t screen_w, screen_h;
	/* g_flightBrightnessScaleQ8 — the classic display-brightness scale
	 * the default explosion point-light case derives from. */
	uint16_t brightness_q8;
} XwaFlightCamera;

/* Persistent hyperspace-effect STATE. The engine stores streak seeds in
 * camera-local XWA coordinates and rebuilds their quad corners every draw.
 * Capturing the seeds (but not screenX/screenY or generated vertices) keeps
 * the HD renderer aspect/FOV independent and preserves the launch's random
 * field without consuming either engine RNG. */
typedef struct XwaHyperspaceStreak {
	int32_t offset[3];
	int32_t half_width;
	uint16_t roll;
} XwaHyperspaceStreak;

#define XWA_SNAP_MAX_HYPERSPACE_STREAKS 1024

typedef enum XwaHyperspacePhase {
	XWA_HYPERSPACE_NONE = 0,
	XWA_HYPERSPACE_OUTBOUND = 2,
	XWA_HYPERSPACE_INBOUND = 3,
	XWA_HYPERSPACE_TUNNEL = 4,
} XwaHyperspacePhase;

typedef struct XwaHyperspaceState {
	uint8_t phase; /* XwaHyperspacePhase */
	uint32_t phase_elapsed_ticks;
	uint32_t tunnel_frame_q16;
} XwaHyperspaceState;

/* OPT mesh-type values the drivers dispatch on (mirrors MeshType —
 * carried per glb mesh slot as AeronMeshRot.mesh_type). */
#define XWA_SNAP_MESH_FUSELAGE 3
#define XWA_SNAP_MESH_BRIDGE 7
#define XWA_SNAP_MESH_ROTARY_GUN_TURRET 21
#define XWA_SNAP_MESH_ROTARY_LAUNCHER 22
#define XWA_SNAP_MESH_ROTARY_BEAM 24

/* The player cockpit — a real OPT model rendered from the interior.
 * Pure sim/view STATE, sampled at CaptureFlight (no draw hook). The
 * classic draws the player's cockpit OPT for one anchor object in
 * internal view, at that object's own orientation
 * (FVIEW_SetObjectTransform of its roll/pitch/yaw/angleD — the same
 * basis the driver derives for every flight object), positioned at
 * -(hardpoint_world + camera_pan/16) rotated into eye space. Anchor
 * and gates mirror the classic draw sites: the hangar anchors to the
 * PLAYER object and draws whenever the view is internal
 * (hangar.c render loop); flight anchors to the camera FOCUS object
 * and requires cockpit_visible plus the seat's availability flag
 * (FlightView_Render queue conditions). Gunner seat 2 additionally
 * flips the model about the hardpoint (basis * diag(1,-1,-1)). */
typedef struct XwaCockpit {
	/* Cockpit OPT basename from the loaded-handle registry: seat 0 =
	 * g_cockpitModel (includes the classic Tie/CombatSim fallback
	 * cockpits); turret seats = the seat's turret model
	 * (g_modelDefs turretModelIndex[seat-1]). Empty = none loaded. */
	char model_name[40];
	/* The own craft's "<name>Exterior.opt" variant (g_exteriorModel) —
	 * the classic swaps it in for the PLAYER object in external view
	 * (flight and hangar loops alike; the flyable OPT has no cockpit
	 * detail). Empty = not loaded (classic then draws the flyable
	 * OPT). */
	char exterior_name[40];
	uint8_t seat;             /* player currentSeatIdx (0 = pilot) */
	uint8_t look_available;   /* player cockpitLookAvailable (seat-0 gate) */
	uint8_t toggle_available; /* player cockpitToggleAvailable (turret gate) */
	/* Turret-seat aim (craft turretAim, seat-1 slot; 0 for the pilot
	 * seat). Classic rotates the turret cockpit's rotary meshes by
	 * these: gun/launcher types by aim_angle_a, the beam type by
	 * -aim_angle_b (Q16, about the mesh's own rotation node). */
	int16_t aim_angle_a;
	int16_t aim_angle_b;
	float hardpoint_world[3]; /* seat eye hardpoint (player.hardpointWorld*) */
	float camera_pan[3];      /* viewState.cameraPanDelta* (classic scales by 1/16) */
	/* Weapon hardpoint in the player CRAFT's model space
	 * (player.hardpointLocal*) — the classic anchors the weapon-fire
	 * pulse point lights here. */
	float hardpoint_local[3];
} XwaCockpit;

/* Local-player weapon-fire light pulses (g_localPlayerLightPulses):
 * time-animated point lights at the weapon hardpoint, toggled by the
 * firing code. The driver replays the classic cycle/fade envelope
 * from game_time_ms (deterministic). */
typedef struct XwaLightPulse {
	uint8_t enabled;
	int32_t start_time; /* g_gameTime base */
	float cycle_ticks;
	float inv_cycle_ticks;
	float fade_ticks;
	float intensity;
	float cull_radius;
	float color[3];
} XwaLightPulse;
#define XWA_SNAP_MAX_LIGHT_PULSES 6

/* Coordinate-strip cap: the engine clamps stripSegmentCount to 16,
 * then rounds it up to a multiple of the frame count (24, or
 * 4 * frames) — bounded by 4 * 8-frame groups = 32 segments. */
#define XWA_SNAP_MAX_STRIP_COORDS 33

/* One region backdrop (suns, planets, nebula sheets): a directional
 * sprite at infinity. Sampled at CaptureFlight from the region's
 * WorldRectRecord table — mission-static STATE (world direction,
 * strip geometry built once at load), not draw records. The driver
 * derives the hardware draw exactly as Backdrop_RenderCurrentRegion:
 * sides 0-3 and flags&2 render the world-space coordinate strip
 * (segment quads at strip_coords[i] +/- strip_half_height on world Z,
 * sliding U, frame advancing every strip_segments_per_frame segments
 * with the half-height rescaled by the frame-height ratio); sides 4/5
 * render one quad at world_dir/512 with half extents along world
 * X (height) / Y (width), extent = angular_scale * (classic_texdim *
 * 1.5) / 256, gated on the camera's forward-row Z sign (side 4 > 0,
 * side 5 < 0). Suns also feed dir_lights and lens flares. */
typedef struct XwaBackdrop {
	uint16_t model_type; /* ObjectTypeId (DAT texture group binding) */
	uint8_t frame;
	uint8_t flags; /* WorldRectRecord.flags (1 = fixed frame, 2 = strip) */
	uint8_t draw_flags;
	uint8_t side; /* WorldRectRecord.side (0-3 strip, 4/5 axis quad) */
	/* Classic per-record draw gates, sampled at capture: backdrops
	 * disabled with drawFlags set, or the Death Star tunnel beam
	 * sprite while no shot is active. */
	uint8_t hidden;
	float world_dir[3];     /* RAW Q20 direction (unnormalized) */
	uint16_t angular_scale; /* WorldRectRecord.angularScale */
	float color[3];
	float intensity;
	/* Coordinate-strip geometry (world space, engine magnitudes) —
	 * strip_segment_count 0 when the record has none (sides 4/5, or
	 * setup skipped it). */
	int32_t strip_half_height;
	uint8_t strip_segment_count;
	uint8_t strip_segments_per_frame;
	float strip_coords[XWA_SNAP_MAX_STRIP_COORDS][3];
} XwaBackdrop;

/* Receiver-local surface projections maintained by the classic glow-mark
 * pools. Most are short texture animations; blast marks are long-lived and
 * engine-knockout blast marks remain until repaired. Geometry is re-derived
 * by the HD driver from these projector parameters + cooked mesh vertices. */
typedef struct XwaGlowMark {
	uint16_t owner_slot;
	uint16_t owner_signature;
	uint8_t pool_kind; /* 0 animated (24-pool), 1 blast (32-pool) */
	uint8_t pool_index;
	uint8_t world_segment_mode;
	uint8_t persistent_until_cleared;
	uint32_t generation;
	uint16_t texture_model_type;
	uint16_t texture_frame; /* resolved DAT frame; 1-based */
	uint32_t age_ticks;     /* blast age; animated records use current frame */
	float center[3];
	float normal[3];
	float u_axis[3];
	float v_axis[3];
	float inv_scale_u;
	float inv_scale_v;
	float layer_uv_scale; /* classic per-frame dimension compensation */
	uint64_t mesh_mask;   /* engine/OPT mesh ordinals traversed at creation */
} XwaGlowMark;

/* ---- object trails -------------------------------------------------
 * Persistent renderer state captured after the classic flight frame has
 * advanced it. The HD renderer rebuilds the camera-facing ribbon for each
 * view; no projected coordinates or classic draw vertices cross this
 * boundary. Points are stored newest-to-oldest in one flat pool, with each
 * emitter owning a contiguous range. */
typedef struct XwaTrailPoint {
	int32_t world_pos[3];
	int32_t spawn_time_ms;
	float age_fade; /* classic normalized age accumulator */
	float tex_v;    /* unwrapped texture V */
} XwaTrailPoint;

typedef struct XwaTrailEmitter {
	uint16_t owner_slot;
	uint16_t owner_signature;
	uint8_t trail_kind;
	uint16_t texture_model_type;
	uint16_t texture_frame;
	uint32_t argb_color;
	float ribbon_width;
	float start_alpha_bias;
	float alpha_fade_start;
	float alpha_fade_rate;
	int32_t forward_offset;
	uint32_t first_point;
	uint16_t point_count;
} XwaTrailEmitter;

/* ---- particles -----------------------------------------------------
 * Persistent particle-system state flattened after the classic effects pass.
 * Local/object-attached coordinates are normalized to world-space points at
 * capture; the HD driver still owns camera-facing geometry and projection. */

typedef enum XwaParticleSourceKind {
	XWA_PARTICLE_SOURCE_WORLD = 0,
	XWA_PARTICLE_SOURCE_OBJECT = 1,
} XwaParticleSourceKind;

typedef enum XwaParticleBillboardMode {
	XWA_PARTICLE_BILLBOARD_FACING = 0,
	XWA_PARTICLE_BILLBOARD_STRETCHED = 1,
	/* Object-local stretched path: tail factor 0.1, 4x width and the
	 * classic per-axis minimum-width behavior. */
	XWA_PARTICLE_BILLBOARD_STRETCHED_LOCAL = 2,
} XwaParticleBillboardMode;

/* Exact large-scale world anchor plus a small float displacement. The HD
 * renderer subtracts its integer origin before converting the sum to float. */
typedef struct XwaPreciseWorldPoint {
	int32_t base[3];
	float offset[3];
} XwaPreciseWorldPoint;

typedef struct XwaParticle {
	uint32_t stable_id; /* modern allocation generation; never an engine pointer */
	XwaPreciseWorldPoint world_pos;
	XwaPreciseWorldPoint tail_world_pos; /* stretched modes only */
	uint32_t argb_color;                 /* packed legacy D3D AARRGGBB */
	float size_scale;                    /* particle.size * template.billboardScale */
	uint16_t texture_frame;              /* resolved classic frame, 1-based */
} XwaParticle;

typedef struct XwaParticleEffect {
	uint32_t stable_id;
	uint16_t owner_slot;      /* 0xffff for world-list effects */
	uint16_t owner_signature; /* 0 for world-list effects */
	uint8_t source_kind;      /* XwaParticleSourceKind */
	uint8_t region;
	uint8_t effect_type;
	uint8_t billboard_mode; /* XwaParticleBillboardMode */
	/* The classic dispatcher suppresses only the top-level object-local
	 * path for the local player in external/film views. Child effects are
	 * camera-facing and do not inherit this gate. */
	uint8_t hide_owner_external;
	uint8_t hide_owner_film;
	uint8_t point_light; /* intended effect-origin light; object effects only */
	uint16_t texture_model_type;
	uint32_t render_flags; /* legacy material/filter state */
	XwaPreciseWorldPoint emitter_world_pos;
	uint32_t first_particle;
	uint16_t particle_count;
} XwaParticleEffect;

/* ---- HUD channel --------------------------------------------------- */

#define XWA_SNAP_MAX_HUD_RADAR_BLIPS 96
#define XWA_SNAP_MAX_HUD_TARGET_BOXES 512
#define XWA_SNAP_MAX_HUD_GLYPHS 4096
#define XWA_SNAP_MAX_HUD_PANES 22

typedef enum XwaHudPaneId {
	XWA_HUD_PANE_NONE = 0,
	XWA_HUD_PANE_TOP_SPEED,
	XWA_HUD_PANE_TOP_THROTTLE,
	XWA_HUD_PANE_TOP_CRAFT_NAME,
	XWA_HUD_PANE_TOP_CLOCK,
	XWA_HUD_PANE_TOP_WEAPONS,
	XWA_HUD_PANE_TOP_COUNTERMEASURE,
	XWA_HUD_PANE_TOP_PROVING_STATUS,
	XWA_HUD_PANE_LEFT_SUBSYSTEM,
	XWA_HUD_PANE_RIGHT_SUBSYSTEM,
	XWA_HUD_PANE_CMD,
	XWA_HUD_PANE_RETICLE_COUNTS,
	XWA_HUD_PANE_MFD_LEFT_TITLE,
	XWA_HUD_PANE_MFD_LEFT_BODY,
	XWA_HUD_PANE_MFD_RIGHT_TITLE,
	XWA_HUD_PANE_MFD_RIGHT_BODY,
	XWA_HUD_PANE_MESSAGE_SYSTEM,
	XWA_HUD_PANE_MESSAGE_FLIGHT_GROUP,
	XWA_HUD_PANE_MESSAGE_READY,
	XWA_HUD_PANE_NETWORK,
	XWA_HUD_PANE_FILM_RECORDING,
	XWA_HUD_PANE_FPS,
} XwaHudPaneId;

typedef struct XwaHudGlyph {
	uint16_t pane;
	uint8_t ch;
	uint8_t font_tier;
	int16_t x, y;
	uint8_t scale;
	uint8_t classic_w;
	uint8_t reserved[2];
	uint32_t argb;
} XwaHudGlyph;

typedef struct XwaHudPane {
	uint16_t id;
	uint8_t visible;
	uint8_t reserved;
	uint32_t generation;
	int16_t origin_x, origin_y;
	uint16_t classic_w, classic_h;
} XwaHudPane;

typedef struct XwaHudInstruments {
	uint16_t player_object_type;
	uint16_t player_model_index;
	uint16_t throttle_speed;
	uint16_t speed;
	uint16_t engine_output_scale;
	int32_t hull_damage, hull_max;
	int32_t shield_front, shield_rear, shield_max;
	uint16_t subsystem_damage;
	uint16_t installed_features;
	uint16_t active_features;
	uint16_t system_flags;
	uint16_t working_subsystems;
	uint8_t laser_redirect;
	uint8_t shield_redirect;
	uint8_t beam_level;
	uint8_t beam_type;
	uint16_t beam_present;
	uint8_t beam_active;
	uint8_t cm_type;
	uint8_t cm_count;
	uint8_t shield_damage_flash;
	uint8_t hull_damage_flash;
	uint8_t last_shield_damage_side;
	uint8_t reserved_damage;
	uint8_t cannon_count;
	uint8_t laser_slot_count;
	uint8_t warhead_launcher_count;
	uint8_t energy_bank_laser_selector;
	uint8_t energy_bank_ion_selector;
	uint8_t laser_group_last_slot[3];
	uint8_t warhead_first_slot[2];
	uint8_t warhead_last_slot[2];
	uint8_t warhead_slot_count[2];
	uint16_t shield_silhouette_sprite;
	uint8_t laser_link_mode[6];
	uint8_t laser_link_next_slot[3];
	uint16_t laser_projectile_type[3];
	uint8_t weapon_type[16];
	uint8_t laser_charge[16];
	uint8_t warhead_count[16];
	uint16_t warhead_lock_ticks;
	uint8_t system_display_slot[11];
	uint16_t system_health[11];
	uint16_t system_timer[11];
} XwaHudInstruments;

typedef struct XwaHudTarget {
	uint16_t slot, signature;
	uint16_t selected_component;
	uint8_t valid;
	uint8_t padlock_active;
	uint16_t distance_whole, distance_frac;
	uint16_t shield_pct, system_pct, hull_pct;
	char name[30];
	char status[30];
} XwaHudTarget;

typedef struct XwaHudReticle {
	uint8_t visible;
	uint8_t weapon_mode;
	uint8_t selected_warhead;
	uint8_t missile_lock_state;
	uint8_t in_range;
	uint8_t laser_hardpoint_count;
	uint8_t warhead_hardpoint_count;
	uint8_t ready[16];
	uint8_t hardpoint_kind[16];
	int16_t laser_hardpoint_index[16];
	int16_t warhead_hardpoint_index[16];
	int16_t hardpoint_local[16][3];
	int16_t aim_offset[16][2];
	int16_t look_yaw, look_pitch;
	uint8_t seat;
	uint8_t turret_auto_fire;
	/* Mouse flight virtual-stick marker (position mode): held deflection in
	 * [-127, 127], drawn by the HD HUD relative to the reticle center. */
	uint8_t stick_marker;
	int8_t stick_marker_x;
	int8_t stick_marker_y;
} XwaHudReticle;

typedef struct XwaHudThreats {
	uint8_t laser;
	uint8_t turret;
	uint8_t beam;
	uint8_t missile;
	uint8_t flash_frame;
} XwaHudThreats;

typedef struct XwaHudRadarBlip {
	uint16_t slot, signature;
	uint8_t radar;
	uint8_t targeted;
	int16_t local_x, local_y;
	uint16_t color_index;
} XwaHudRadarBlip;

typedef enum XwaHudTargetBoxLayer {
	XWA_HUD_TARGET_BOX_BEFORE_FIXED = 0,
	XWA_HUD_TARGET_BOX_AFTER_FIXED = 1,
} XwaHudTargetBoxLayer;

typedef struct XwaHudTargetBox {
	uint16_t slot, signature;
	uint16_t component;
	uint8_t color_index;
	uint8_t selected;
	uint8_t layer;
	int32_t extent; /* resolved unprojected extent used by Targeting_DrawObjectBox */
} XwaHudTargetBox;

typedef struct XwaHudCrt {
	uint8_t visible;
	uint8_t self_view;
	uint8_t map_view;
	uint8_t component_marker_visible;
	uint16_t target_slot, target_signature;
	uint16_t selected_component;
	uint16_t projectile_exclude_slots[2];
	uint16_t classic_viewport_w;
	uint16_t classic_viewport_h;
	uint16_t proj_aspect_y_q16;
	int32_t proj_scale;
	int32_t component_focus[3];
	/* Target-to-camera fit distance before projection through the
	 * truncated Q15 camera row. */
	int32_t camera_distance;
	int32_t camera_back_step[3];
	int16_t camera_rows_q15[9];
} XwaHudCrt;

typedef enum XwaHudModeFlags {
	XWA_HUD_MODE_EXTERNAL_CAMERA = 1u << 0,
	XWA_HUD_MODE_MAP = 1u << 1,
	XWA_HUD_MODE_HANGAR_READY = 1u << 2,
	XWA_HUD_MODE_HANGAR_AUTOCAM = 1u << 3,
	XWA_HUD_MODE_HYPERSPACE = 1u << 4,
	XWA_HUD_MODE_REGION_SESSION = 1u << 5,
	XWA_HUD_MODE_MISSION_END = 1u << 6,
	XWA_HUD_MODE_FILM_PLAYBACK = 1u << 7,
	XWA_HUD_MODE_FILM_OVERLAY = 1u << 8,
	XWA_HUD_MODE_REPLAY_VIEW = 1u << 9,
	XWA_HUD_MODE_PROVING_GROUND = 1u << 10,
	XWA_HUD_MODE_MULTIPLAYER = 1u << 11,
	XWA_HUD_MODE_POWER_VR = 1u << 12,
	XWA_HUD_MODE_COCKPIT_VISIBLE = 1u << 13,
	XWA_HUD_MODE_PADLOCK = 1u << 14,
	XWA_HUD_MODE_FILM_RECORDING = 1u << 15,
} XwaHudModeFlags;

typedef struct XwaHudState {
	uint8_t valid;
	uint8_t hud_enabled;
	uint8_t classic_frame_valid;
	uint8_t film_mfd_visible;
	float classic_hud_scale; /* g_flightHudScaleFactor */
	uint32_t classic_frame_epoch;
	uint32_t mode_flags;
	uint16_t element_enabled_mask;
	uint16_t player_slot, player_signature;
	uint8_t mfd_enabled[3];
	uint8_t mfd_page[3];
	uint8_t mfd_active;
	uint8_t mfd_menu_row, mfd_menu_item;
	uint32_t hud_colors[7];
	XwaHudInstruments instruments;
	XwaHudTarget target;
	XwaHudReticle reticle;
	XwaHudThreats threats;
	XwaHudCrt crt;
	XwaHudPane panes[XWA_SNAP_MAX_HUD_PANES];
	uint16_t pane_count;
	XwaHudGlyph glyphs[XWA_SNAP_MAX_HUD_GLYPHS];
	uint16_t glyph_count;
	uint16_t glyph_dropped;
	uint16_t pane_scope_errors;
	uint16_t radar_classic_radius;
	uint8_t radar_target_marker_visible;
	uint8_t radar_target_marker_radar;
	int16_t radar_target_marker_local_x;
	int16_t radar_target_marker_local_y;
	XwaHudRadarBlip radar_blips[XWA_SNAP_MAX_HUD_RADAR_BLIPS];
	uint16_t radar_blip_count;
	XwaHudTargetBox target_boxes[XWA_SNAP_MAX_HUD_TARGET_BOXES];
	uint16_t target_box_count;
} XwaHudState;

/* Scene billboards (explosion/spark/debris/chaff sprites, wreck
 * flames) are STATE-DERIVED: the HD driver rebuilds the classic
 * SceneBillboard_QueueObjectTextured + flush laws from the per-object
 * billboard-law fields above (type_specific_*, instance_extent,
 * source_object_type) + the ModelTexFrame side accessor; no draw-time
 * capture exists. */

/* ---- caps ----------------------------------------------------------
 * Sized from observed frontend behavior (dirty-rect UI redraws a few
 * hundred sprites on busy concourse frames); truncation is counted
 * and logged once per tick. */
#define XWA_SNAP_MAX_DRAWS_2D 4096
/* The briefing ship-inspect draws the craft hologram as per-edge
 * FrontendDraw_Line calls — 20k+ per tick for large exteriors. */
#define XWA_SNAP_MAX_PAINT_CMDS 24576
#define XWA_SNAP_MAX_GLYPHS 8192
#define XWA_SNAP_MAX_MODEL_PREVIEWS 4
#define XWA_SNAP_MAX_SURFACE_EVENTS 64
#define XWA_SNAP_MAX_BACKDROPS 32  /* engine: 32 records per region */
#define XWA_SNAP_MAX_GLOW_MARKS 56 /* exact: 24 animated + 32 blast */
#define XWA_SNAP_MAX_TRAIL_EMITTERS 512
#define XWA_SNAP_MAX_TRAIL_POINTS 4096
#define XWA_SNAP_MAX_TRAIL_POINTS_PER_EMITTER 256
#define XWA_SNAP_MAX_PARTICLE_EFFECTS 36
#define XWA_SNAP_MAX_PARTICLES 3600
#define XWA_SNAP_MAX_OPT_ASSETS 1024
#define XWA_SNAP_OPT_NAME_MAX 40
#define XWA_SNAP_MAX_TEXTURE_ASSETS 600
#define XWA_SNAP_MAX_FRONTEND_FILES 1024
#define XWA_SNAP_MAX_SPRITE_GROUPS 200

/* One processed OPT handle currently owned by the classic engine. The
 * remaster mirrors this authoritative live set after each committed tick;
 * public_handle identifies the original allocation while name selects the
 * GLB / runtime-OPT HD counterpart. */
typedef struct XwaOptAsset {
	uint16_t public_handle;
	char name[XWA_SNAP_OPT_NAME_MAX]; /* basename, no extension */
} XwaOptAsset;

/* One sprite-textured model type whose classic TexLevel storage is live.
 * The remaster mirrors the corresponding baked atlas pages while this type
 * remains in the authoritative set. */
typedef struct XwaTextureAsset {
	uint16_t model_type;
} XwaTextureAsset;

/* One baked source-file identity currently owned by a classic named
 * frontend resource. frame_count covers frame-directory assets; single
 * images and packed atlases still carry their classic descriptor count. */
typedef struct XwaFrontendFileAsset {
	char file[XWA_SNAP_FRONTEND_FILE_MAX];
	char source_file[XWA_SNAP_FRONTEND_SOURCE_MAX];
	uint16_t frame_count;
} XwaFrontendFileAsset;

/* One DAT sprite group currently owned by the classic sprite subsystem. */
typedef struct XwaFrontendGroupAsset {
	int16_t group;
} XwaFrontendGroupAsset;

/* ---- snapshot ------------------------------------------------------ */

typedef struct XwaSnapshot {
	uint64_t tick_index; /* monotonic host tick counter */
	/* g_gameTime at commit — the SIM clock in ms (freezes on pause,
	 * scales with time compression, follows film replay). Drives all
	 * driver-side animation pacing and the lerp-vs-snap decision
	 * (equal across the pair = paused; non-monotonic = seek). The
	 * tick's sim delta is the difference across the held pair. */
	int32_t game_time_ms;
	XwaSceneKind scene_kind;

	XwaDraw2D draws_2d[XWA_SNAP_MAX_DRAWS_2D];
	uint32_t draw_2d_count;
	XwaPaintCmd paint_cmds[XWA_SNAP_MAX_PAINT_CMDS];
	uint32_t paint_cmd_count;
	XwaGlyph2D glyphs[XWA_SNAP_MAX_GLYPHS];
	uint32_t glyph_count;
	XwaModelPreview model_previews[XWA_SNAP_MAX_MODEL_PREVIEWS];
	uint32_t model_preview_count;
	XwaDirLight dir_lights[XWA_SNAP_MAX_DIR_LIGHTS];
	uint32_t dir_light_count;
	XwaSurfaceEvent surface_events[XWA_SNAP_MAX_SURFACE_EVENTS];
	uint32_t surface_event_count;
	/* g_offscreenRestoreEnabled at commit: MAIN-target records are
	 * per-frame transients when set; persistent when clear. */
	uint8_t offscreen_restore_enabled;
	/* Authoritative named-resource and DAT-group ownership. Assets referenced
	 * by this tick remain listed through this snapshot so draw records emitted
	 * before a same-tick free can still replay. */
	uint64_t frontend_asset_generation;
	XwaFrontendFileAsset frontend_files[XWA_SNAP_MAX_FRONTEND_FILES];
	uint32_t frontend_file_count;
	XwaFrontendGroupAsset frontend_groups[XWA_SNAP_MAX_SPRITE_GROUPS];
	uint32_t frontend_group_count;
	/* Bumps on every processed OPT load/free. Unlike render-time model
	 * requests, this is the classic engine's authoritative asset lifetime. */
	uint64_t opt_asset_generation;
	XwaOptAsset opt_assets[XWA_SNAP_MAX_OPT_ASSETS];
	uint32_t opt_asset_count;
	uint64_t texture_asset_generation;
	XwaTextureAsset texture_assets[XWA_SNAP_MAX_TEXTURE_ASSETS];
	uint32_t texture_asset_count;

	XwaFlightObject flight_objects[XWA_SNAP_MAX_FLIGHT_OBJECTS];
	uint32_t flight_object_count;
	XwaFlightMapState flight_map;
	XwaFlightCamera flight_camera;
	uint8_t flight_camera_valid;
	XwaHyperspaceState hyperspace;
	XwaHyperspaceStreak hyperspace_streaks[XWA_SNAP_MAX_HYPERSPACE_STREAKS];
	uint32_t hyperspace_streak_count;
	XwaCockpit cockpit;
	uint8_t cockpit_valid;
	XwaLightPulse light_pulses[XWA_SNAP_MAX_LIGHT_PULSES];
	uint8_t light_pulse_active;
	XwaBackdrop backdrops[XWA_SNAP_MAX_BACKDROPS];
	uint32_t backdrop_count;
	XwaGlowMark glow_marks[XWA_SNAP_MAX_GLOW_MARKS];
	uint32_t glow_mark_count;
	XwaTrailEmitter trail_emitters[XWA_SNAP_MAX_TRAIL_EMITTERS];
	uint32_t trail_emitter_count;
	XwaTrailPoint trail_points[XWA_SNAP_MAX_TRAIL_POINTS];
	uint32_t trail_point_count;
	XwaParticleEffect particle_effects[XWA_SNAP_MAX_PARTICLE_EFFECTS];
	uint32_t particle_effect_count;
	XwaParticle particles[XWA_SNAP_MAX_PARTICLES];
	uint32_t particle_count;
	XwaHudState hud;
	uint32_t dropped_records; /* emits lost to the caps this tick */

	/* Frontend cursor, sampled at commit (FRONTEND scenes). */
	uint8_t cursor_visible;
	int32_t cursor_x, cursor_y; /* hotspot-adjusted */
	char cursor_sprite[XWA_SNAP_SPRITE_NAME_MAX];
} XwaSnapshot;

/* ---- lifecycle (host loop) ----------------------------------------- */

/* Reset the write slot for a new tick (counters, z counter, target
 * tag; scene_kind persists until set again). */
void XwaSnapshot_BeginTick(void);

/* Flip the write slot into Current (previous Current becomes
 * Previous). Logs once per tick when records were dropped. */
void XwaSnapshot_Commit(void);

const XwaSnapshot* XwaSnapshot_Current(void);
const XwaSnapshot* XwaSnapshot_Previous(void);

/* ---- emitters (engine hooks; read-only observers) ------------------ */

void XwaSnapshot_SetSceneKind(XwaSceneKind kind);

/* FrontendDraw_Begin/EndExternalSurface routing tag. */
void XwaSnapshot_SetEmitTarget(XwaEmitTarget target);

/* Sprite family. `src` may be NULL (whole image); `frame` is the
 * resource's currentFrame at draw time (animation cel); img_w/img_h
 * are the classic image dims of that frame. `opaque_fill_color` is
 * palette entry 0 for the color-key-blind opaque blit, otherwise 0.
 * `orientation_mode` is the raw selector for the oriented kinds. */
void XwaSnapshot_EmitSprite(XwaDraw2DKind kind, const char* name, int frame, const int16_t src_ltrb[4],
							int dst_x, int dst_y, int img_w, int img_h, uint32_t tint_color,
							uint32_t opaque_fill_color, int32_t orientation_mode);
/* DAT atlas sprite. x/y is the FINAL anchored blit position (the
 * caller adds the payload anchor first — aeron atlas convention:
 * origins are baked into the draw position at emit time). */
void XwaSnapshot_EmitAtlasSprite(int group_id, int index, int x, int y, int img_w, int img_h);

/* Name -> source-file binding, recorded at resource registration
 * (FrontImage_RegisterResource* success). `fileName` is the engine's
 * raw path ("frontres\\family\\familyroom.bmp"); it is normalized to
 * the baked-asset key ("family/familyroom") and stamped into
 * subsequent sprite records for that name. */
void XwaSnapshot_NoteResourceBinding(const char* fileName, const char* name, int frame_count);
void XwaSnapshot_NoteResourceFree(const char* name);

void XwaSnapshot_EmitPaint(XwaPaintKind kind, int x0, int y0, int x1, int y1, int dx, int dy, uint32_t color);

/* One glyph from a FrontendText_Draw* string loop. */
void XwaSnapshot_EmitGlyph(int font_size, unsigned char ch, int x, int y, uint32_t color);

/* Surface-structure event (see XwaSurfaceEventKind). rect ignored for
 * the full-surface kinds. */
void XwaSnapshot_EmitSurfaceEvent(XwaSurfaceEventKind kind, int left, int top, int right, int bottom);

/* Variant with the kind-specific aux payload (COMPOSITE_REVEAL). */
void XwaSnapshot_EmitSurfaceEventAux(XwaSurfaceEventKind kind, int left, int top, int right, int bottom,
									 int aux0, int aux1);

/* One model-preview render (z_order / target filled by the helper). */
void XwaSnapshot_EmitModelPreview(const XwaModelPreview* preview);

/* Walk g_objectTable + the local player's view state into the flight
 * channel. Called from the port loop after XwaFlightTask_Tick (not an
 * in-engine hook); no-op outside flight scenes. */
void XwaSnapshot_CaptureFlight(void);

/* Hyperspace's classic renderer appends the NEXT 45 random seeds after
 * latching the count it draws this frame. This scalar capture slot preserves
 * that one-frame visibility latency without capturing any draw geometry. */
void XwaSnapshot_NoteHyperspaceVisibleStreakCount(uint32_t count);

/* XWA_MODERN capture slots populated while the original renderer resolves a
 * patch. They preserve parameters the persistent legacy block discards; they
 * never consume RNG or affect game/render branching. `patch` is an engine
 * ObjectMeshTextureLayerBlock, kept opaque at this API boundary. */
void XwaSnapshot_NoteGlowMarkAllocation(const void* patch, uint16_t texture_frame);
void XwaSnapshot_NoteGlowMarkProjector(const void* patch, float inv_scale_u, float inv_scale_v,
									   int world_segment_mode);
void XwaSnapshot_NoteGlowMarkMesh(const void* patch, unsigned int mesh_index);

/* Convert a raw engine 16bpp color argument (paint cmds, text colors)
 * to RGBA8 using the live 555/565 pixel-format flag. Source-color
 * conversion only — the HD render decodes no classic assets and
 * scrapes no framebuffer. */
void XwaSnapshotExport_ColorToRgba(uint32_t color, uint8_t out_rgba[4]);
uint32_t XwaSnapshotExport_FlightPaletteColor(uint16_t color_index);
uint8_t XwaSnapshotExport_FlightColorCodePaletteIndex(uint8_t color_code);
int XwaSnapshotExport_ComponentTargetGeometry(int object_type, int component, float out_local[3],
											  int* out_extent);

/* Engine model name for an ObjectTypeId — the
 * OPT basename of the model the engine LOADED for that type
 * (g_loadedModels handle -> load-time name registry; covers hangar
 * props and every non-craft loadable), falling back to the g_modelDefs
 * craft name. NULL when the type has no model. The driver keys cooked
 * .glb assets with it. */
const char* XwaSnapshotExport_ModelName(int object_type);

/* Engine-side OPT-loader observers. They update only snapshot-side state;
 * recovered simulation/render behavior never branches on the registry. */
void XwaSnapshot_NoteOptLoad(uint16_t public_handle, const char* file_name);
void XwaSnapshot_NoteOptFree(uint16_t public_handle);

/* Copy the authoritative live processed-OPT set into a snapshot slot. */
void XwaSnapshotExport_CaptureOptAssets(XwaSnapshot* snapshot);

/* Engine-side texture-model observers and committed live-set capture. */
void XwaSnapshot_NoteTextureLoad(uint16_t model_type);
void XwaSnapshot_NoteTextureFree(uint16_t model_type);
void XwaSnapshotExport_CaptureTextureAssets(XwaSnapshot* snapshot);

/* Engine-side frontend resource/group observers and committed residency set. */
void XwaSnapshot_NoteSpriteGroupLoad(int16_t group);
void XwaSnapshot_NoteSpriteGroupFree(int16_t group);
void XwaSnapshot_NoteSpriteGroupsReset(void);

/* The registry entry for a public OPT handle (e.g. g_cockpitModel),
 * or NULL when the handle is 0/unregistered. */
const char* XwaSnapshotExport_OptHandleName(uint16_t public_handle);

/* A sprite-textured model type's DAT binding.
 * Returns 0 when the type has none. Two classic binding kinds
 * (g_modelTypeTable flags bit 0x100):
 *   frames_mode 1: the WHOLE group loads as frames; a draw's frame N
 *     selects the group's Nth sprite in group order (1-based —
 *     FeDiskIo_SelectTextureFrame).
 *   frames_mode 0: the type binds ONE sprite: DAT id = sprite_id
 *     (the entry's frameCount field; SpriteTexture_ConvertById). */
int XwaSnapshotExport_ModelTextureBinding(int object_type, int* out_group, int* out_frames_mode,
										  int* out_sprite_id);

/* Side accessor for the billboard geometry laws: the frame's BASE
 * (level-0) classic texture dims and the model type's maxBoundsExtent.
 * Mirrors FeDiskIo_SelectTextureFrame's frame lookup without the mip
 * walk (the level shift compensation makes level 0 the geometry
 * authority: half extents are (dim * projected_size) >> 9 px at any
 * level). Returns 0 when the type's tex levels are not loaded or the
 * frame is out of range — the classic queue gate. NOTE the level's
 * argbColor is NOT exposed: it is draw-time scratch every
 * RenderQuad_DrawModelTexture call overwrites with its color arg (the
 * billboard flush passes -1 = opaque white; lens flares pass their
 * faint overrides). */
int XwaSnapshotExport_ModelTexFrame(int object_type, int frame, int* out_w, int* out_h, int* out_max_bounds);

/* Side accessor for the wreck-flame law (Damage_QueueCraftBillboards):
 * the model type's per-mesh OPT mesh types (MeshType values; flames
 * attach to MESH_Fuselage == 3 slots). Returns the mesh count (0 when
 * the type has no meshes), filling up to XWA_SNAP_MAX_MESH_SLOTS. */
int XwaSnapshotExport_ModelMeshTypes(int object_type, uint8_t out_types[XWA_SNAP_MAX_MESH_SLOTS]);

/* Side accessor: a model type's animation frame count (mission-static
 * table value; the explosion lens-flare window law reads it). 0 for
 * out-of-range types. */
int XwaSnapshotExport_ModelFrameCount(int object_type);

/* Side accessor for the point-light SOURCE laws
 * (FlightLight_AppendScenePointLightForObject): the classic per-type
 * color / intensity / cull-radius derivation, computed from captured
 * object fields (pure function — type tables + the classic switch).
 * Covers the explosion-genus family (sparks, explosions, chaff, DS2
 * blast — including the x4 multiplier and the brightness-based
 * default) and the projectile/special table (lasers, ions, warheads,
 * DS fixtures, the Falcon hangar light). Engine-glow lights are NOT
 * covered — the driver derives those from the cooked glb extras. The
 * caller applies the classic append GATES (genus/type list, hangar /
 * dir-light context) itself. Returns 0 when the type contributes no
 * light. */
int XwaSnapshotExport_PointLightForObject(int object_type, int genus, int type_specific_0,
										  int type_specific_w, int instance_extent, int brightness_q8,
										  float out_color[3], float* out_intensity, int* out_cull_radius);

#ifdef __cplusplus
}
#endif

#endif /* XWA_SNAPSHOT_H */

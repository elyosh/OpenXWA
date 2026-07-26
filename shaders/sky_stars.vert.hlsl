/*
 * XWA procedural starfield vertex shader.
 *
 * The scatter counterpart to the classic fullscreen starfield: instead of
 * shading every sky pixel to find the ~0.4% that are stars, this emits one
 * small screen-space quad per grid cell that actually hosts a visible
 * star, so the rasterizer only touches star pixels. The black background
 * comes from the scene's colour clear; star quads are additive over it.
 *
 * One non-indexed draw of 6·N²·6 vertices (6 verts / quad, one quad per
 * cube-face grid cell). Each vertex derives its cell from SV_VertexID,
 * hashes the same field as the classic gather shader (identical star
 * positions / twinkle), projects the star direction to clip space, and
 * offsets by its quad corner. Empty cells, winked-out stars, and stars
 * behind the camera collapse to a degenerate off-screen triangle.
 */

cbuffer SkyStarsVS : register(b0, space1)
{
    /* Cube-sampling basis → jittered clip space. Directions use w=0 so
     * camera translation is excluded. */
    row_major float4x4 cube_to_clip;
    /* x = viewport width px, y = viewport height px, z = grid N, w = density */
    float4             view;
    /* x = core half-extent px, y = classic-pixel pitch px, z = sim clock ms,
     * w = flare strength */
    float4             geom;
    /* x = exposure × brightness, y = quad margin px, z/w reserved */
    float4             tone;
};

static const int STAR_CYCLE_OFF[4] = { 0, 39, 103, 148 };
static const int STAR_CYCLE_LEN[4] = { 39, 64, 45, 57 };
static const int STAR_PAL[205] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
    0, 0, 0, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 6, 7, 8, 10, 12, 14, 16, 17,
    18, 19, 20, 20, 21, 21, 22, 22, 22, 23, 23, 23, 23, 22, 22, 22, 21, 21,
    20, 20, 19, 18, 17, 16, 14, 12, 10, 8, 7, 6, 5, 4, 4, 3, 3, 2, 2, 1, 1, 1,
    0, 0, 0,
    0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 23, 23, 22, 20, 18, 16,
    14, 12, 10, 8, 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 2, 1, 0, 0, 0, 0, 0,
    0, 0, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
    19, 20, 21, 22, 23, 23, 23, 23, 23, 20, 22, 18, 21, 18, 20, 19, 16, 18,
    16, 17, 16, 15, 13, 14, 12, 12, 11, 10, 8, 5, 2, 0, 0, 0, 0, 0, 0, 0
};

uint star_hash(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

uint cell_seed(int face, int cx, int cy)
{
    uint k = (uint)(cx + 4096) * 73856093u ^ (uint)(cy + 4096) * 19349663u ^
             ((uint)face + 1u) * 83492791u;
    return star_hash(k);
}

float hash01(uint h) { return (float)(h >> 8) * (1.0f / 16777216.0f); }

float3 dir_from_face(int face, float2 uv)
{
    if (face == 0) return float3(1.0f, -uv.y, -uv.x);
    if (face == 1) return float3(-1.0f, -uv.y, uv.x);
    if (face == 2) return float3(uv.x, 1.0f, uv.y);
    if (face == 3) return float3(uv.x, -1.0f, -uv.y);
    if (face == 4) return float3(uv.x, -uv.y, 1.0f);
    return float3(-uv.x, -uv.y, -1.0f);
}

float3 star_ramp(int L, int jitter)
{
    return pow(max(float3(L / 31.0f, (2.0f * L) / 63.0f, (L + jitter) / 31.0f), 0.0f), 2.2f);
}

struct VSOut
{
    float4                   position : SV_Position;
    float2                   localPx  : TEXCOORD0; /* pixel offset from star centre */
    nointerpolation float3   coreCol  : TEXCOORD1;
    nointerpolation float3   flareCol : TEXCOORD2; /* 0 = no flare this star */
    nointerpolation float3   diagCol  : TEXCOORD3; /* even-parity dim diagonal */
    nointerpolation float    parity   : TEXCOORD4; /* 1 = plus (odd), 0 = L (even) */
};

VSOut main(uint vid : SV_VertexID)
{
    /* Quad corners (triangle list): 0,1,2, 0,2,3 over (-1,-1)(1,-1)(1,1)(-1,1). */
    static const uint CORNER[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
    uint cell = vid / 6u;
    uint k = CORNER[vid % 6u];
    float2 quad = float2((k == 1u || k == 2u) ? 1.0f : -1.0f, (k == 2u || k == 3u) ? 1.0f : -1.0f);

    int gridN = (int)view.z;
    int perFace = gridN * gridN;
    int face = (int)(cell / (uint)perFace);
    int within = (int)(cell % (uint)perFace);
    int cx = within % gridN;
    int cy = within / gridN;

    VSOut o;
    o.position = float4(2.0f, 2.0f, 2.0f, 1.0f); /* default: culled, off-screen */
    o.localPx = float2(0.0f, 0.0f);
    o.coreCol = float3(0.0f, 0.0f, 0.0f);
    o.flareCol = float3(0.0f, 0.0f, 0.0f);
    o.diagCol = float3(0.0f, 0.0f, 0.0f);
    o.parity = 0.0f;

    uint h = cell_seed(face, cx, cy);
    if (hash01(h) >= view.w)
    {
        return o; /* empty cell */
    }

    h = star_hash(h);
    float sx = hash01(h);
    h = star_hash(h);
    float sy = hash01(h);
    h = star_hash(h);
    uint ci = h & 3u;
    h = star_hash(h);
    uint attrib = h;

    int cnt = STAR_CYCLE_LEN[ci];
    int phase = (int)(attrib % (uint)cnt);
    int jitter = (int)((attrib >> 13) & 7u);
    int step = (int)(geom.z / 60.0f);
    int idx = (step + phase) % cnt;
    int n = STAR_PAL[STAR_CYCLE_OFF[ci] + idx];
    if (n <= 0)
    {
        return o; /* winked out this frame */
    }

    /* Star direction (cube space) → clip space. */
    float2 uv = ((float2(cx, cy) + float2(sx, sy)) / view.z) * 2.0f - 1.0f;
    float3 sd = normalize(dir_from_face(face, uv));
    float4 clip = mul(cube_to_clip, float4(sd, 0.0f));
    if (clip.w <= 1e-4f)
    {
        return o; /* behind the camera */
    }

    float coreHalf = geom.x;
    float pitch = geom.y;
    float quadHalfPx = coreHalf + pitch + tone.y; /* cover core + flare + AA */
    float2 localPx = quad * quadHalfPx;
    /* localPx.y is screen-down; NDC y is up, so flip. */
    float2 clipOff = localPx * float2(2.0f / view.x, -2.0f / view.y) * clip.w;

    o.position = float4(clip.xy + clipOff, 0.0f, clip.w); /* reverse-Z far plane */
    o.localPx = localPx;

    float expB = tone.x;
    o.coreCol = star_ramp(n, jitter) * expB;
    if (geom.w > 0.0f && n > 16)
    {
        int nf = n & 7;
        float fscale = expB * geom.w;
        o.flareCol = star_ramp(nf, 0) * fscale;
        o.diagCol = star_ramp(max(nf - 1, 0), 0) * fscale;
        o.parity = (((attrib >> 20) & 1u) != 0u) ? 1.0f : 0.0f;
    }
    return o;
}

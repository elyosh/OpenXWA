/*
 * XWA procedural starfield fragment shader.
 *
 * The scatter fragment stage: runs only over the small quads the vertex
 * shader emits around each visible star (not the whole sky). Each star's
 * colours + flare parity are computed per-quad in the vertex shader; here
 * we just evaluate box coverage of the pixel against the star centre and
 * its flare neighbours, and output the additive contribution.
 *
 * Faithful to the classic FlightStarfield_Render footprint: the core is
 * one classic pixel (a square box, not a round disc), and bright stars
 * (blue channel > 16, resolved in the VS) gain DrawBrightStarFlare's exact
 * ±1 classic-pixel pattern — a 4-way plus for odd-index stars, an L (left,
 * up, dim up-left) for even. Output alpha is 0 so additive blending leaves
 * the destination alpha (from the colour clear) untouched.
 */

cbuffer SkyStarsPS : register(b0, space3)
{
    /* x = core half-extent px, y = classic-pixel pitch px (flare offset),
     * z = dot antialias feather px, w = reserved. */
    float4 shape;
};

struct VSOut
{
    float4                 position : SV_Position;
    float2                 localPx  : TEXCOORD0;
    nointerpolation float3 coreCol  : TEXCOORD1;
    nointerpolation float3 flareCol : TEXCOORD2;
    nointerpolation float3 diagCol  : TEXCOORD3;
    nointerpolation float  parity   : TEXCOORD4;
};

/* Coverage of a one-classic-pixel box centred at `o` (screen pixels). Full
 * inside (half - feather), antialiased to 0 at half. */
float box1d(float p, float half, float feather)
{
    return 1.0f - smoothstep(half - feather, half, abs(p));
}
float box_at(float2 p, float2 o, float half, float feather)
{
    return box1d(p.x - o.x, half, feather) * box1d(p.y - o.y, half, feather);
}

float4 main(VSOut i) : SV_Target
{
    float coreHalf = shape.x;
    float pitch = shape.y;
    float feather = shape.z;
    float2 p = i.localPx;

    float3 acc = i.coreCol * box_at(p, float2(0.0f, 0.0f), coreHalf, feather);

    /* Bright-star flare (flareCol is 0 when inactive, so this is a no-op
     * for dim stars). Odd-index → 4-way plus; even-index → L. */
    if (i.parity > 0.5f)
    {
        acc += i.flareCol * (box_at(p, float2(pitch, 0.0f), coreHalf, feather) +
                             box_at(p, float2(-pitch, 0.0f), coreHalf, feather) +
                             box_at(p, float2(0.0f, pitch), coreHalf, feather) +
                             box_at(p, float2(0.0f, -pitch), coreHalf, feather));
    }
    else
    {
        acc += i.flareCol * (box_at(p, float2(-pitch, 0.0f), coreHalf, feather) +
                             box_at(p, float2(0.0f, -pitch), coreHalf, feather));
        acc += i.diagCol * box_at(p, float2(-pitch, -pitch), coreHalf, feather);
    }

    return float4(acc, 0.0f); /* additive; preserve destination alpha */
}

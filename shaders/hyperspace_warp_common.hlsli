/* Shared by the hyperspace scene warp shaders (docs/hd_hyperspace_jump.md,
 * Scene warp). */

#ifndef XWA_HYPERSPACE_WARP_COMMON_INCLUDED
#define XWA_HYPERSPACE_WARP_COMMON_INCLUDED

/* Sky colours are divided by the classic radiance and clamped to this before
 * they are raised to the highlight power, so the powers stay within FP16
 * range. */
static const float HYPER_WARP_SKY_CLAMP = 30.0f;

/* How much of the scene is left at `px` by the black reveal: 1 untouched, 0
 * black. `vanishing` is xy the vanishing point in px, z how much the view
 * faces the direction of travel (0 to 1), w 1 / the distance from the
 * vanishing point to the farthest screen corner. `reveal` is x the progress
 * (0 to 1) and y the width of the soft edge, in shares of that distance.
 *
 * The black opens from the vanishing point. In views that don't face the
 * direction of travel the scene fades evenly instead. */
float warp_reveal(float2 px, float4 vanishing, float4 reveal)
{
	const float progress = reveal.x;
	const float softness = reveal.y;
	const float distance = length(px - vanishing.xy) * vanishing.w;
	const float edge = progress * (1.0f + softness);
	const float radial = smoothstep(edge - softness, edge, distance);
	const float even = 1.0f - smoothstep(0.0f, 1.0f, progress);
	return lerp(even, radial, vanishing.z);
}

#endif

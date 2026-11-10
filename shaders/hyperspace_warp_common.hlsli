/* Shared by the hyperspace scene warp shaders (docs/hd_hyperspace_jump.md,
 * Scene warp). */

#ifndef XWA_HYPERSPACE_WARP_COMMON_INCLUDED
#define XWA_HYPERSPACE_WARP_COMMON_INCLUDED

/* Sky colours are divided by the classic radiance and clamped to this before
 * they are raised to the highlight power, so the powers stay within FP16
 * range. */
static const float HYPER_WARP_SKY_CLAMP = 30.0f;

/* How much of the scene the black reveal leaves at `distance` from the
 * vanishing point: 1 untouched, 0 black. The distance is a share of the
 * distance from the vanishing point to the farthest screen corner.
 * `vanishing` is xy the vanishing point in px, z how much the view faces the
 * direction of travel (0 to 1), w 1 / the distance to the farthest corner.
 * `reveal` is x the progress (0 to 1) and y the width of the soft edge, in
 * shares of that distance.
 *
 * The black opens from the vanishing point. In views that don't face the
 * direction of travel the scene fades evenly instead. */
float reveal_visibility_at_distance(float distance, float4 vanishing, float4 reveal)
{
	const float progress = reveal.x;
	const float softness = reveal.y;
	const float edge = progress * (1.0f + softness);
	const float radial = smoothstep(edge - softness, edge, distance);
	const float even = 1.0f - smoothstep(0.0f, 1.0f, progress);
	return lerp(even, radial, vanishing.z);
}

/* The same at `px`, in px. */
float reveal_visibility_at_px(float2 px, float4 vanishing, float4 reveal)
{
	return reveal_visibility_at_distance(length(px - vanishing.xy) * vanishing.w, vanishing, reveal);
}

/* The sky passes raise colours to the highlight power and the composite
 * takes that root. The default power, 2, takes no pow. */
float3 apply_highlight_power(float3 color, float power)
{
	return power == 2.0f ? color * color : pow(color, power);
}

float3 remove_highlight_power(float3 color, float power)
{
	return power == 2.0f ? sqrt(color) : pow(color, 1.0f / power);
}

#endif

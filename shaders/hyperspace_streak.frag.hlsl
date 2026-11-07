/*
 * Hyperspace jump streaks (docs/hd_hyperspace_jump.md).
 *
 * Each pixel finds the nearest point of the streak and shapes the streak
 * there:
 *   - the core is Gaussian across the streak, and its width follows true
 *     perspective (wider where the streak is nearer the camera) and the
 *     distance from the vanishing point;
 *   - a soft glow skirt surrounds the core;
 *   - brightness and colour go from the tail's to the head's along the
 *     streak's 3D length;
 *   - each end fades out over a share of that 3D length, so the fades look
 *     long near the screen's edge and short near the centre. Ends never
 *     narrow, so they don't read as drawn shapes.
 * A streak only a few widths long keeps a round shape and the head's look,
 * so a star that hasn't moved yet is a soft round point. Output alpha is 0
 * so additive blending keeps the destination alpha.
 */

#include "hyperspace_streak_common.hlsli"

cbuffer HyperspaceStreakPS : register(b0, space3)
{
	/* x glow strength, y glow width in core widths, z tail brightness as a
	 * share of the head's, w largest widening from perspective and distance
	 * to the vanishing point together. */
	float4 glow;
	/* xy vanishing point in px, z radial widening, w 1 / screen half-height. */
	float4 vanishing;
	/* rgb colour at the tail. */
	float4 tail_color;
};

/* 0 at the tip of an end, 1 where its fade is over. */
float taper_ramp(float distance_from_end, float taper_length)
{
	return taper_length > 1.0e-3f ? saturate(distance_from_end / taper_length) : 1.0f;
}

float4 main(HyperStreakVaryings input) : SV_Target0
{
	const float2 tail = input.ends.xy;
	const float2 segment = input.ends.zw - tail;
	const float length_sq = dot(segment, segment);
	const float2 offset = input.position.xy - tail;
	const float t = length_sq > 1.0e-6f ? saturate(dot(offset, segment) / length_sq) : 0.0f;
	const float2 away = offset - segment * t;

	/* Where the nearest point is on the whole streak: on screen, and on its
	 * 3D length. 1 / depth is linear on screen; the 3D share follows from it. */
	const float full_length = input.path.y;
	const float screen_share =
		full_length > 1.0e-3f ? saturate((t * sqrt(length_sq) + input.path.x) / full_length) : 0.0f;
	const float inverse_depth = lerp(input.path.z, input.path.w, screen_share);
	const float share =
		inverse_depth > 0.0f ? saturate(screen_share * input.path.w / inverse_depth) : screen_share;

	/* The brightness follows perspective alone. */
	const float gain = streak_gain(input.star.y, inverse_depth, glow.w);
	float sigma = streak_sigma(input.star.x, gain, tail + segment * t, vanishing, glow.w);
	float intensity = input.star.z * sqrt(gain);
	if (sigma < HYPER_STREAK_MIN_SIGMA)
	{
		intensity *= sigma / HYPER_STREAK_MIN_SIGMA;
		sigma = HYPER_STREAK_MIN_SIGMA;
	}

	/* Short streaks keep their round shape and the head's look. */
	const float shaped = saturate(full_length / (HYPER_STREAK_CORE_EXTENT * sigma));
	const float tail_fade = smoothstep(0.0f, 1.0f, taper_ramp(share, input.tapers.x));
	const float head_fade = smoothstep(0.0f, 1.0f, taper_ramp(1.0f - share, input.tapers.y));
	const float fade = lerp(1.0f, tail_fade * head_fade, shaped);
	const float gradient = lerp(1.0f, share, shaped);
	const float3 color = lerp(tail_color.rgb, input.color, gradient) * lerp(saturate(glow.z), 1.0f, gradient);

	const float distance_sq = dot(away, away);
	const float core = exp(-0.5f * distance_sq / (sigma * sigma));
	const float glow_radius = HYPER_STREAK_HALF_WIDTH * glow.y * sigma;
	/* The skirt falls off from the core, without a plateau. */
	const float skirt_ramp = 1.0f - smoothstep(0.0f, glow_radius, sqrt(distance_sq));
	const float skirt = saturate(glow.x) * skirt_ramp * skirt_ramp;
	return float4(color * (intensity * fade * max(core, skirt)), 0.0f);
}

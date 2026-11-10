/* Shared by the hyperspace jump streak shaders (docs/hd_hyperspace_jump.md). */

#ifndef XWA_HYPERSPACE_STREAK_COMMON_INCLUDED
#define XWA_HYPERSPACE_STREAK_COMMON_INCLUDED

struct HyperStreakVaryings
{
	float4 position : SV_Position;
	/* The streak's ends after clipping to the screen: tail xy, head xy, px. */
	nointerpolation float4 ends : TEXCOORD0;
	/* The whole streak before clipping to the screen: x distance in px from
	 * its tail to the clipped tail, y its length in px, z/w 1 / depth at its
	 * tail and head. */
	nointerpolation float4 path : TEXCOORD1;
	/* x the star's core width as a standard deviation in px, before
	 * perspective; y the star's distance at the start of the jump; z its
	 * intensity; w unused. */
	nointerpolation float4 star : TEXCOORD2;
	/* Tail and head taper lengths, as shares of the streak's 3D length. */
	nointerpolation float2 tapers : TEXCOORD3;
	/* Colour at the head. */
	nointerpolation float3 color : TEXCOORD4;
};

/* Smallest Gaussian standard deviation drawn, in px. Thinner streaks keep
 * their energy by getting dimmer instead. */
static const float HYPER_STREAK_MIN_SIGMA = 0.6f;
/* The core reaches this many standard deviations from the streak. */
static const float HYPER_STREAK_CORE_EXTENT = 4.0f;
/* Half the full width at half maximum, in standard deviations: glow widths
 * are counted in core widths. */
static const float HYPER_STREAK_HALF_WIDTH = 1.1774f;
/* Distances from the vanishing point beyond this many screen half-heights
 * don't widen a streak more. */
static const float HYPER_STREAK_MAX_RADIAL = 2.5f;

/* How much nearer a point of the streak is than its star was at the start,
 * from 1 up to `widening`. 1 / depth changes linearly along the streak on
 * screen, so this is true perspective. */
float streak_gain(float star_distance, float inverse_depth, float widening)
{
	return clamp(star_distance * inverse_depth, 1.0f, max(widening, 1.0f));
}

/* The core's standard deviation at `px`. `vanishing` is xy the vanishing
 * point in px, z the radial widening, w 1 / screen half-height. Perspective
 * and radial widening together stay within `widening`. */
float streak_sigma(float base_sigma, float gain, float2 px, float4 vanishing, float widening)
{
	const float radial = min(length(px - vanishing.xy) * vanishing.w, HYPER_STREAK_MAX_RADIAL);
	return base_sigma * min(gain * (1.0f + vanishing.z * radial), max(widening, 1.0f));
}

/* How far the quad must reach from the streak for a core of `sigma`, with a
 * glow `glow_width` core widths wide. */
float streak_extent(float sigma, float glow_width)
{
	return max(HYPER_STREAK_CORE_EXTENT, HYPER_STREAK_HALF_WIDTH * glow_width) * sigma;
}

#endif

/*
 * Hyperspace jump streaks (docs/hd_hyperspace_jump.md).
 *
 * One non-indexed draw of 6 vertices per star. Each vertex rebuilds its
 * star from a hash of the star index, moves it toward the camera, projects
 * the streak's two ends, clips them, and places one corner of a
 * screen-space quad around the streak. The fragment shader shapes the
 * streak from the two ends, which it gets without interpolation. Hidden
 * stars collapse to a degenerate triangle off screen.
 *
 * View space follows the tunnel shader: x right, y down, z forward.
 */

#include "hyperspace_streak_common.hlsli"

cbuffer HyperspaceStreakVS : register(b0, space1)
{
	/* x/y render size in px, z px per 1080p px, w near plane in view space. */
	float4 view;
	/* x/y tan(horizontal/vertical half-FOV), z/w projection offsets. */
	float4 projection;
	/* The ship's right, up and forward axes in view space. */
	float4 ship_right;
	float4 ship_up;
	float4 ship_forward;
	/* x head travel, y tail travel as a share of the head's, z share of
	 * visible stars, w width factor. */
	float4 motion;
	/* x intensity, y tint amount, z streak width in 1080p px,
	 * w largest widening from perspective and distance to the vanishing
	 * point together. */
	float4 appearance;
	/* rgb tint. */
	float4 tint;
	/* x brightest-to-faintest ratio, y how much brightness widens a star,
	 * z random width spread (a ratio), w random taper spread (0 to 1). */
	float4 variation;
	/* x tail taper, y head taper, as shares of the streak's 3D length. */
	float4 taper;
	/* xy vanishing point in px, z radial widening, w 1 / screen half-height. */
	float4 vanishing;
	/* x glow strength, y glow width in core widths. */
	float4 glow;
	/* x star count, y seed. */
	uint4 config;
};

static const float HYPER_STREAK_TWO_PI = 6.28318530718f;

uint streak_hash(uint x)
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

float streak_unit(inout uint state)
{
	state = streak_hash(state);
	return (float)(state >> 8) * (1.0f / 16777216.0f);
}

float3 ship_to_view(float3 p)
{
	return ship_right.xyz * p.x + ship_up.xyz * p.y + ship_forward.xyz * p.z;
}

float2 view_to_px(float3 p)
{
	float2 ndc = float2(p.x / (p.z * projection.x) + projection.z,
						projection.w - p.y / (p.z * projection.y));
	return float2((ndc.x * 0.5f + 0.5f) * view.x, (0.5f - ndc.y * 0.5f) * view.y);
}

/* Star colour from a temperature in 0..1: warm, white, then blue-white. */
float3 star_color(float temperature)
{
	static const float3 warm = float3(1.0f, 0.78f, 0.55f);
	static const float3 white = float3(1.0f, 0.97f, 0.94f);
	static const float3 blue = float3(0.68f, 0.80f, 1.0f);
	return temperature < 0.5f ? lerp(warm, white, temperature * 2.0f)
							  : lerp(white, blue, temperature * 2.0f - 1.0f);
}

HyperStreakVaryings main(uint vertex_id : SV_VertexID)
{
	HyperStreakVaryings output;
	output.position = float4(2.0f, 2.0f, 0.0f, 1.0f);
	output.ends = 0.0f;
	output.path = 0.0f;
	output.star = 0.0f;
	output.tapers = 0.0f;
	output.color = 0.0f;

	const uint star = vertex_id / 6u;
	if (star >= config.x)
		return output;

	uint state = streak_hash(star * 0x9e3779b9u ^ config.y);
	const float cos_polar = streak_unit(state) * 2.0f - 1.0f;
	const float azimuth = streak_unit(state) * HYPER_STREAK_TWO_PI;
	const float distance = exp(streak_unit(state) * log(6.0f));
	const float onset = streak_unit(state);
	const float brightness_roll = streak_unit(state);
	const float bright_roll = streak_unit(state);
	const float warm_roll = streak_unit(state);
	const float temperature_roll = streak_unit(state);
	const float width_roll = streak_unit(state);
	const float tail_taper_roll = streak_unit(state);
	const float head_taper_roll = streak_unit(state);

	/* Stars show in order of their delay as the visible share grows. */
	const float visible = smoothstep(onset - 0.08f, onset, motion.z);
	if (visible <= 0.0f)
		return output;

	float sin_azimuth, cos_azimuth;
	sincos(azimuth, sin_azimuth, cos_azimuth);
	const float sin_polar = sqrt(max(1.0f - cos_polar * cos_polar, 0.0f));
	const float3 star_ship = float3(sin_polar * cos_azimuth, sin_polar * sin_azimuth, cos_polar) * distance;

	const float head_travel = motion.x;
	const float tail_travel = motion.x * motion.y;
	float3 tail = ship_to_view(star_ship - float3(0.0f, 0.0f, tail_travel));
	float3 head = ship_to_view(star_ship - float3(0.0f, 0.0f, head_travel));

	/* Clip against the near plane: a star that passes the camera becomes a
	 * line running off screen. */
	const float near_z = view.w;
	if (tail.z < near_z && head.z < near_z)
		return output;
	if (tail.z < near_z)
		tail = lerp(tail, head, (near_z - tail.z) / (head.z - tail.z));
	if (head.z < near_z)
		head = lerp(head, tail, (near_z - head.z) / (tail.z - head.z));
	const float tail_inverse_depth = 1.0f / tail.z;
	const float head_inverse_depth = 1.0f / head.z;

	/* Brightness spreads on a log scale around a median of 0.25, skewed so
	 * most stars are faint: the faintest are 1/ratio^0.25 of the median and
	 * the brightest ratio^0.75 times it. A few stars are brighter still. */
	const float relative_brightness =
		pow(max(variation.x, 1.0f), brightness_roll * brightness_roll - 0.25f);
	float brightness = 0.25f * relative_brightness;
	if (bright_roll < 0.04f)
		brightness *= 2.5f;
	const float intensity = appearance.x * brightness * visible;

	/* Brighter stars are wider, and each star has a random width factor
	 * between 1/spread and spread. */
	const float width_factor = pow(relative_brightness, variation.y) *
							   pow(max(variation.z, 1.0f), width_roll * 2.0f - 1.0f);
	/* Full width at half maximum to standard deviation. */
	const float base_sigma = appearance.z * view.z * motion.w * width_factor / 2.3548f;

	const float2 tail_px = view_to_px(tail);
	const float2 head_px = view_to_px(head);
	const float full_length = length(head_px - tail_px);

	/* Clip the segment to the viewport plus the widest the quad can get
	 * (Liang-Barsky), so streaks running off screen don't make huge quads. */
	const float widest_sigma = max(base_sigma * max(appearance.w, 1.0f), HYPER_STREAK_MIN_SIGMA);
	const float margin = streak_extent(widest_sigma, glow.y);
	const float2 box_min = float2(-margin, -margin);
	const float2 box_max = view.xy + margin;
	const float2 delta = head_px - tail_px;
	float enter = 0.0f;
	float leave = 1.0f;
	[unroll]
	for (int axis = 0; axis < 2; ++axis)
	{
		const float d = delta[axis];
		const float lo = box_min[axis] - tail_px[axis];
		const float hi = box_max[axis] - tail_px[axis];
		if (abs(d) < 1.0e-6f)
		{
			if (lo > 0.0f || hi < 0.0f)
				return output;
		}
		else
		{
			const float t0 = lo / d;
			const float t1 = hi / d;
			enter = max(enter, min(t0, t1));
			leave = min(leave, max(t0, t1));
		}
	}
	if (enter > leave)
		return output;
	const float2 clipped_tail = tail_px + delta * enter;
	const float2 clipped_head = tail_px + delta * leave;

	/* The quad's reach at each clipped end, from the core width there. */
	const float tail_gain =
		streak_gain(distance, lerp(tail_inverse_depth, head_inverse_depth, enter), appearance.w);
	const float head_gain =
		streak_gain(distance, lerp(tail_inverse_depth, head_inverse_depth, leave), appearance.w);
	const float tail_extent = streak_extent(
		max(streak_sigma(base_sigma, tail_gain, clipped_tail, vanishing, appearance.w), HYPER_STREAK_MIN_SIGMA),
		glow.y);
	const float head_extent = streak_extent(
		max(streak_sigma(base_sigma, head_gain, clipped_head, vanishing, appearance.w), HYPER_STREAK_MIN_SIGMA),
		glow.y);

	/* Trapezoid around the streak: corners 0,1 at the tail, 2,3 at the head. */
	static const uint CORNER[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
	const uint corner = CORNER[vertex_id % 6u];
	const float2 segment = clipped_head - clipped_tail;
	const float segment_length = length(segment);
	const float2 along = segment_length > 1.0e-4f ? segment / segment_length : float2(1.0f, 0.0f);
	const float2 across = float2(-along.y, along.x);
	float2 px;
	if (corner == 0u)
		px = clipped_tail - along * tail_extent - across * tail_extent;
	else if (corner == 1u)
		px = clipped_tail - along * tail_extent + across * tail_extent;
	else if (corner == 2u)
		px = clipped_head + along * head_extent + across * head_extent;
	else
		px = clipped_head + along * head_extent - across * head_extent;

	/* Most stars are blue-white, about one in ten is warm. */
	const float temperature = warm_roll < 0.1f ? temperature_roll * 0.4f : 0.4f + 0.6f * temperature_roll;
	/* Each star's tapers vary by up to the taper spread either way. */
	const float spread = saturate(variation.w);
	output.position = float4(px.x / view.x * 2.0f - 1.0f, 1.0f - px.y / view.y * 2.0f, 0.0f, 1.0f);
	output.ends = float4(clipped_tail, clipped_head);
	output.path = float4(full_length * enter, full_length, tail_inverse_depth, head_inverse_depth);
	output.star = float4(base_sigma, distance, intensity, 0.0f);
	output.tapers = float2(saturate(taper.x * lerp(1.0f - spread, 1.0f + spread, tail_taper_roll)),
						   saturate(taper.y * lerp(1.0f - spread, 1.0f + spread, head_taper_roll)));
	output.color = lerp(star_color(temperature), tint.rgb, saturate(appearance.y));
	return output;
}

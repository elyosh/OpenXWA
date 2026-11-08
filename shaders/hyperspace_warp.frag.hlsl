/*
 * The hyperspace scene warp's composite (docs/hd_hyperspace_jump.md, Scene
 * warp). For each pixel `q` of the output:
 *
 *   - Keep mask: where the scene is nearer than the keep distance (the
 *     cockpit), `q` keeps the scene's colour and nothing is drawn over it.
 *   - World: the world moves a distance `s` toward the camera along the
 *     ship's forward axis `f`. Points move on lines through the vanishing
 *     point, so the content that reaches `q` comes from the line behind it.
 *     In a forward exit the world comes in toward the camera from behind
 *     its rest position: `s` is negative, and the content comes from the
 *     other side of `q`.
 *     The walk samples that line, on a log scale of the distance from `q`.
 *     A tap's point, rebuilt from its depth, crosses `q` when the world has
 *     moved z * t / |u| (t the tap's distance from `q` and u the direction of
 *     movement at `q`, both in tangent units). The tap covers `q` when that
 *     happens during the smear, from the smear's old end to `s`. The nearest
 *     covering surface wins; taps close to it in depth are averaged.
 *   - Background: the warped sky from the sky passes, or, where they found
 *     no sky, the first sky pixel of the walk.
 *   - The black reveal darkens the world's taps at their sources, as the sky
 *     passes do for the sky.
 *   - Layer: the streaks and the tunnel's fades, drawn at render resolution
 *     with premultiplied alpha, go over the result.
 *
 * View space follows the streak shader: x right, y down, z forward.
 */

#include "hyperspace_warp_common.hlsli"

Texture2D<float4> g_scene : register(t0, space2);
SamplerState g_scene_sampler : register(s0, space2);
Texture2D<float4> g_depth : register(t1, space2);
SamplerState g_depth_sampler : register(s1, space2);
Texture2D<float4> g_sky : register(t2, space2);
SamplerState g_sky_sampler : register(s2, space2);
Texture2D<float4> g_layer : register(t3, space2);
SamplerState g_layer_sampler : register(s3, space2);

cbuffer HyperspaceWarp : register(b0, space3)
{
	/* xy output size in px, zw 1 / that size. */
	float4 size;
	/* xy tan(horizontal/vertical half-FOV), zw projection offsets. */
	float4 projection;
	/* xyz the ship's forward axis in view space. */
	float4 forward;
	/* xy vanishing point in px, z how much the view faces the direction of
	 * travel, w 1 / distance to the farthest screen corner. */
	float4 vanishing;
	/* x how far the world has moved (s, negative behind its rest position),
	 * y s at the smear's old end, z near plane, w depth at the keep distance
	 * (nearer is larger). All in view units. */
	float4 motion;
	/* x reveal progress, y reveal softness, z 1 when the warp runs, w sky
	 * highlight power. */
	float4 reveal;
	/* x taps, y classic radiance, z debug view, w the side of its rest
	 * position the world is on: 1 nearer the camera, -1 farther (forward
	 * exit). */
	float4 config;
	/* xy render size in px, zw 1 / that size. */
	float4 render;
};

static const uint WARP_VIEW_KEEP_MASK = 1u;
static const uint WARP_VIEW_SKY = 2u;
static const uint WARP_VIEW_REVEAL = 3u;

/* Taps within this share of the nearest covering tap's depth are part of
 * the same surface. */
static const float WARP_SURFACE_BAND = 0.15f;

float scene_depth(float2 uv)
{
	return g_depth.SampleLevel(g_depth_sampler, uv, 0.0f).r;
}

/* The pixel's position in tangent units (x / z, y / z of its view ray). */
float2 to_tangent(float2 px)
{
	const float2 ndc = float2(px.x * size.z * 2.0f - 1.0f, 1.0f - px.y * size.w * 2.0f);
	return float2((ndc.x - projection.z) * projection.x, (projection.w - ndc.y) * projection.y);
}

/* How far a ray from `origin` along `direction` stays on screen, in px. */
float screen_exit(float2 origin, float2 direction)
{
	float distance = 1.0e9f;
	[unroll]
	for (int axis = 0; axis < 2; ++axis)
	{
		if (direction[axis] > 1.0e-6f)
			distance = min(distance, (size[axis] - origin[axis]) / direction[axis]);
		else if (direction[axis] < -1.0e-6f)
			distance = min(distance, -origin[axis] / direction[axis]);
	}
	return max(distance, 0.0f);
}

/* The warped sky at `uv`, in scene colour, and whether there was any. */
float4 warped_sky(float2 uv)
{
	const float4 sky = g_sky.SampleLevel(g_sky_sampler, uv, 0.0f);
	if (sky.a < 0.02f)
		return 0.0f;
	const float3 color = pow(max(sky.rgb / sky.a, 0.0f), 1.0f / reveal.w) * config.y;
	return float4(color, 1.0f);
}

float4 main(float4 position : SV_Position) : SV_Target0
{
	const float2 q = position.xy;
	const float2 uv = q * size.zw;
	const float4 scene = g_scene.SampleLevel(g_scene_sampler, uv, 0.0f);
	const uint view = (uint)config.z;

	/* Keep mask: the nearest depth of the 2x2 render texels around q. */
	const float2 half_texel = 0.5f * render.zw;
	const float keep_depth = max(max(scene_depth(uv + float2(-half_texel.x, -half_texel.y)),
									 scene_depth(uv + float2(half_texel.x, -half_texel.y))),
								 max(scene_depth(uv + float2(-half_texel.x, half_texel.y)),
									 scene_depth(uv + float2(half_texel.x, half_texel.y))));
	const bool keep = keep_depth >= motion.w;
	if (view == WARP_VIEW_KEEP_MASK)
		return float4(keep ? lerp(scene.rgb, float3(config.y, 0.0f, 0.0f), 0.6f) : scene.rgb, scene.a);
	if (view == WARP_VIEW_REVEAL)
		return float4(warp_reveal(q, vanishing, reveal) * config.y * float3(1.0f, 1.0f, 1.0f), scene.a);
	if (keep)
		return scene;

	float3 warped = scene.rgb;
	if (reveal.z > 0.5f && reveal.x >= 1.0f)
	{
		/* The reveal is complete: everything outside the keep mask is
		 * black. */
		warped = 0.0f;
		if (view == WARP_VIEW_SKY)
			return float4(warped, scene.a);
	}
	else if (reveal.z > 0.5f)
	{
		/* Points are moved along +u at q, in tangent units, or along -u when
		 * the world is farther than its rest position. The walk goes the
		 * other way. Distances below are counted along the move. */
		const float3 f = forward.xyz;
		const float direction = config.w < 0.0f ? -1.0f : 1.0f;
		const float2 u = to_tangent(q) * f.z - f.xy;
		const float u_length = max(length(u), 1.0e-6f);
		const float2 back = -direction * u / u_length;
		/* Tangent units per px: the same on both axes. */
		const float tangent_per_px = 2.0f * projection.y * size.w;
		float walk = screen_exit(q, back);
		if (f.z * direction > 1.0e-4f)
			walk = min(walk, length(vanishing.xy - q));

		const float s = motion.x * direction;
		const float s_old = motion.y * direction;
		const float s_min = min(s, s_old);
		const float s_max = max(s, s_old);
		const float near_z = motion.z;
		const int taps = (int)config.x;
		const float jitter = frac(52.9829189f * frac(dot(q, float2(0.06711056f, 0.00583715f))));

		float3 surface = 0.0f;
		float surface_weight = 0.0f;
		float coverage = 0.0f;
		float nearest = 1.0e30f;
		float3 first_sky = 0.0f;
		bool sky_found = false;
		[loop]
		for (int i = 0; i < taps; ++i)
		{
			/* Tap 0 is q itself; the others spread from 1 px to the walk's
			 * end on a log scale. */
			float t = 0.0f;
			if (i > 0)
			{
				if (walk < 1.0f)
					break;
				t = exp(log(walk) * ((float)(i - 1) + jitter) / (float)max(taps - 1, 1));
			}
			const float2 px = q + back * t;
			const float2 tap_uv = px * size.zw;
			const float depth = scene_depth(tap_uv);
			if (depth <= 0.0f)
			{
				if (!sky_found)
				{
					first_sky = g_scene.SampleLevel(g_scene_sampler, tap_uv, 0.0f).rgb *
								warp_reveal(px, vanishing, reveal);
					sky_found = true;
				}
				continue;
			}
			if (depth >= motion.w)
				continue;
			/* The world movement at which the tap's point crosses q, give or
			 * take half a pixel. */
			const float z = near_z / depth;
			const float t_tangent = t * tangent_per_px;
			const float half_px = 0.5f * tangent_per_px;
			const float s_low = z * max(t_tangent - half_px, 0.0f) / u_length;
			const float s_high = z * (t_tangent + half_px) / u_length;
			if (s_high < s_min || s_low > s_max)
				continue;
			const float s_cross = clamp(z * t_tangent / u_length, s_min, s_max);
			const float moved_z = z - direction * s_cross * f.z;
			if (moved_z <= near_z)
				continue;
			/* The smear fades out over its older half. The weight is taken
			 * at the end of the pixel's footprint nearest the smear's head:
			 * while the world has moved less than a pixel, the head is
			 * inside the footprint and the pixel keeps its own content. */
			const float s_head = s > s_old ? min(s_high, s) : max(s_low, s);
			const float along = s != s_old ? saturate((s_head - s_old) / (s - s_old)) : 1.0f;
			const float weight = smoothstep(0.0f, 0.5f, along);
			const float3 color =
				g_scene.SampleLevel(g_scene_sampler, tap_uv, 0.0f).rgb * warp_reveal(px, vanishing, reveal);
			if (moved_z < nearest * (1.0f - WARP_SURFACE_BAND))
			{
				nearest = moved_z;
				surface = color * weight;
				surface_weight = weight;
				coverage = weight;
			}
			else if (moved_z <= nearest * (1.0f + WARP_SURFACE_BAND))
			{
				surface += color * weight;
				surface_weight += weight;
				coverage = max(coverage, weight);
			}
		}

		const float4 sky = warped_sky(uv);
		const float3 background = sky.a > 0.0f ? sky.rgb : first_sky;
		if (view == WARP_VIEW_SKY)
			return float4(background, scene.a);
		const float3 world = surface_weight > 0.0f ? surface / surface_weight : 0.0f;
		warped = lerp(background, world, coverage);
	}
	else if (view == WARP_VIEW_SKY)
	{
		return float4(scene_depth(uv) > 0.0f ? float3(0.0f, 0.0f, 0.0f) : scene.rgb, scene.a);
	}

	const float4 layer = g_layer.SampleLevel(g_layer_sampler, uv, 0.0f);
	return float4(layer.rgb + warped * (1.0f - layer.a), scene.a);
}

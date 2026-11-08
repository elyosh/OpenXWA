/*
 * One pass of the hyperspace sky warp (docs/hd_hyperspace_jump.md, Sky
 * movement).
 *
 * The sky zooms about the vanishing point and smears along the zoom. A zoom
 * about one point is a blur along the log of the distance to that point, so
 * it splits into passes: each pass averages `taps` samples at log zooms
 * `first + step * j`, and three passes chained give taps^3 evenly spaced
 * samples. Each pass zooms its input by exp(-log zoom) about the vanishing
 * point.
 *
 * The first pass reads the scene. Pixels with geometry depth are holes with
 * weight 0; sky pixels get the black reveal, then their colour, divided by
 * the classic radiance and clamped, is raised to the highlight power. The
 * output is (sum of weighted colours, sum of weights) / taps, so later
 * passes average both and the composite divides by the weight.
 *
 * In a forward exit the sky is shrunk: the first pass reads outside the
 * screen. It then reads the region's sky cube, which has no holes, and
 * blends to the scene as the zoom returns to 1. The scene's share fades out
 * near the screen's edges, so they don't show. Later passes only read
 * toward the vanishing point, so they stay on screen.
 *
 * The last pass weights its samples so the smear fades toward its old end.
 */

#include "hyperspace_warp_common.hlsli"

Texture2D<float4> g_source : register(t0, space2);
SamplerState g_source_sampler : register(s0, space2);
Texture2D<float4> g_depth : register(t1, space2);
SamplerState g_depth_sampler : register(s1, space2);
TextureCube<float4> g_sky_cube : register(t2, space2);
SamplerState g_sky_cube_sampler : register(s2, space2);
TextureCube<float4> g_overlay_cube : register(t3, space2);
SamplerState g_overlay_cube_sampler : register(s3, space2);

cbuffer HyperspaceWarpSky : register(b0, space3)
{
	/* xy output size in px, zw 1 / that size. */
	float4 size;
	/* xy vanishing point in px, z how much the view faces the direction of
	 * travel, w 1 / distance to the farthest screen corner. */
	float4 vanishing;
	/* x reveal progress, y reveal softness. */
	float4 reveal;
	/* x log zoom of the first sample, y log zoom between samples, z samples,
	 * w the pass: 0 first, 1 middle, 2 last. */
	float4 zoom;
	/* x highlight power, y 1 / classic radiance. */
	float4 sky;
	/* xy tan(horizontal/vertical half-FOV), zw projection offsets. */
	float4 projection;
	/* x sky cube scale, y overlay scale (0 without an overlay), z unused,
	 * w the scene's share: 1 reads only the scene and not the cubes. */
	float4 cube;
	/* x sky cube texels per output px at zoom 1, y its highest level, z/w
	 * the same for the overlay. */
	float4 cube_level;
	/* Rows of the view-to-cube matrices of the sky and the overlay. */
	float4 sky_rows[3];
	float4 overlay_rows[3];
};

/* Width of the fade of the scene's share at the screen's edges, in screen
 * heights. */
static const float WARP_SKY_EDGE_FADE = 0.05f;

/* The sky in the direction of `px`, which can be outside the screen, from the
 * cubes. `zoom` is the sample's log zoom, which sets the level to read. */
float3 cube_sky(float2 px, float zoom)
{
	const float2 ndc = float2(px.x * size.z * 2.0f - 1.0f, 1.0f - px.y * size.w * 2.0f);
	const float3 view = float3((ndc.x - projection.z) * projection.x, (projection.w - ndc.y) * projection.y, 1.0f);
	/* A shrunk sky packs more texels into each pixel. */
	const float minify = exp(-zoom);
	const float3 sky_dir = float3(dot(sky_rows[0].xyz, view), dot(sky_rows[1].xyz, view), dot(sky_rows[2].xyz, view));
	const float sky_level = clamp(log2(max(cube_level.x * minify, 1.0f)), 0.0f, cube_level.y);
	float3 color = g_sky_cube.SampleLevel(g_sky_cube_sampler, sky_dir, sky_level).rgb * cube.x;
	if (cube.y > 0.0f)
	{
		const float3 overlay_dir = float3(dot(overlay_rows[0].xyz, view), dot(overlay_rows[1].xyz, view),
										  dot(overlay_rows[2].xyz, view));
		const float overlay_level = clamp(log2(max(cube_level.z * minify, 1.0f)), 0.0f, cube_level.w);
		const float4 overlay = g_overlay_cube.SampleLevel(g_overlay_cube_sampler, overlay_dir, overlay_level);
		color = overlay.rgb * cube.y + color * (1.0f - overlay.a);
	}
	return color;
}

float4 main(float4 position : SV_Position) : SV_Target0
{
	const float2 from_vanishing = position.xy - vanishing.xy;
	const int taps = (int)zoom.z;
	const bool first = zoom.w < 0.5f;
	const bool last = zoom.w > 1.5f;
	const bool scene_only = cube.w >= 1.0f;
	float4 sum = 0.0f;
	float weight_sum = 0.0f;
	[loop]
	for (int j = 0; j < taps; ++j)
	{
		const float sample_zoom = zoom.x + zoom.y * (float)j;
		const float2 px = vanishing.xy + from_vanishing * exp(-sample_zoom);
		const float2 uv = px * size.zw;
		/* The smear fades out over its older half. */
		const float weight = last ? smoothstep(0.0f, 0.5f, ((float)j + 0.5f) / (float)taps) : 1.0f;
		weight_sum += weight;
		const bool inside = all(uv >= 0.0f) && all(uv <= 1.0f);
		/* Outside the screen only the cubes have the sky. */
		if (!inside && (scene_only || !first))
			continue;
		float4 sample_value;
		if (first)
		{
			/* The scene's share of the sample: none in its holes and outside
			 * the screen. */
			float scene_share = 0.0f;
			if (inside && g_depth.SampleLevel(g_depth_sampler, uv, 0.0f).r <= 0.0f)
			{
				scene_share = 1.0f;
				if (!scene_only)
				{
					const float2 edge = min(px, size.xy - px) * size.w;
					scene_share = cube.w * smoothstep(0.0f, WARP_SKY_EDGE_FADE, min(edge.x, edge.y));
				}
			}
			if (scene_only && scene_share <= 0.0f)
				continue;
			float3 color = 0.0f;
			if (scene_share > 0.0f)
				color = g_source.SampleLevel(g_source_sampler, uv, 0.0f).rgb * scene_share;
			if (scene_share < 1.0f)
				color += cube_sky(px, sample_zoom) * (1.0f - scene_share);
			color = min(color * sky.y, HYPER_WARP_SKY_CLAMP) * warp_reveal(px, vanishing, reveal);
			sample_value = float4(pow(max(color, 0.0f), sky.x), 1.0f);
		}
		else
		{
			sample_value = g_source.SampleLevel(g_source_sampler, uv, 0.0f);
		}
		sum += sample_value * weight;
	}
	return sum / max(weight_sum, 1.0e-6f);
}

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
 * The last pass weights its samples so the smear fades toward its old end.
 */

#include "hyperspace_warp_common.hlsli"

Texture2D<float4> g_source : register(t0, space2);
SamplerState g_source_sampler : register(s0, space2);
Texture2D<float4> g_depth : register(t1, space2);
SamplerState g_depth_sampler : register(s1, space2);

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
};

float4 main(float4 position : SV_Position) : SV_Target0
{
	const float2 from_vanishing = position.xy - vanishing.xy;
	const int taps = (int)zoom.z;
	const bool first = zoom.w < 0.5f;
	const bool last = zoom.w > 1.5f;
	float4 sum = 0.0f;
	float weight_sum = 0.0f;
	[loop]
	for (int j = 0; j < taps; ++j)
	{
		const float2 px = vanishing.xy + from_vanishing * exp(-(zoom.x + zoom.y * (float)j));
		const float2 uv = px * size.zw;
		/* The smear fades out over its older half. */
		const float weight = last ? smoothstep(0.0f, 0.5f, ((float)j + 0.5f) / (float)taps) : 1.0f;
		weight_sum += weight;
		if (any(uv < 0.0f) || any(uv > 1.0f))
			continue;
		float4 sample_value;
		if (first)
		{
			if (g_depth.SampleLevel(g_depth_sampler, uv, 0.0f).r > 0.0f)
				continue;
			float3 color = g_source.SampleLevel(g_source_sampler, uv, 0.0f).rgb;
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

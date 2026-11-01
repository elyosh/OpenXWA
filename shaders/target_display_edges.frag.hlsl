/* The target display's edges (target_display style `edges`). The input is
 * the tonemapped display, linear with premultiplied alpha. The filter works
 * on sRGB values, and the result goes back to linear:
 *   - each of the 3x3 texels around the pixel gives the luminance
 *     c = dot(luminance, contrast * alpha * srgb);
 *   - G is the length of the Sobel gradient of c;
 *   - the edge (G * C, G), clamped, is blended over the display with its
 *     alpha G, and the display's alpha becomes G^2, so it only shows its
 *     edges;
 *   - a 6-pixel border stays empty, and the display is empty while blanked. */

Texture2D<float4> g_display : register(t0, space2);
SamplerState      g_sampler : register(s0, space2);

cbuffer TargetDisplayEdges : register(b0, space3) {
	float4 edge_color; /* rgb: the edge colour, sRGB values; a: 0 blanks the display */
	float4 luminance;  /* rgb: the luminance weights; w: the contrast */
	float4 size;       /* xy: the display's size in texels; zw: 1 / size */
};

float3 LinearToSrgb(float3 value) {
	return lerp(1.055 * pow(value, 1.0 / 2.4) - 0.055, value * 12.92, step(value, 0.0031308));
}

float3 SrgbToLinear(float3 value) {
	return lerp(pow((value + 0.055) / 1.055, 2.4), value / 12.92, step(value, 0.04045));
}

/* The texel's colour in sRGB without its alpha, and its alpha. */
float4 StraightSrgb(float2 texel) {
	float4 color = g_display.SampleLevel(g_sampler, (texel + 0.5) * size.zw, 0.0);
	float3 rgb   = color.a > 0.0 ? saturate(color.rgb / color.a) : float3(0.0, 0.0, 0.0);
	return float4(LinearToSrgb(rgb), saturate(color.a));
}

float4 main(float4 position : SV_Position) : SV_Target0 {
	float2 texel = floor(position.xy);
	if (edge_color.a < 0.5 || any(texel < 6.0) || any(texel >= size.xy - 6.0))
		return float4(0.0, 0.0, 0.0, 0.0);

	float c[9];
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			float4 color = StraightSrgb(texel + float2(j - 1, i - 1));
			c[3 * i + j] = dot(luminance.rgb, luminance.w * color.a * color.rgb);
		}
	}
	float lx = 2.0 * (c[7] - c[1]) + c[6] + c[8] - c[2] - c[0];
	float ly = 2.0 * (c[3] - c[5]) + c[6] + c[0] - c[2] - c[8];
	float g  = sqrt(lx * lx + ly * ly);

	float  edge_alpha = saturate(g);
	float3 edge_rgb   = saturate(g * edge_color.rgb);
	float3 display    = StraightSrgb(texel).rgb;
	float3 rgb        = edge_rgb * edge_alpha + display * (1.0 - edge_alpha);
	float  alpha      = edge_alpha * edge_alpha;
	return float4(SrgbToLinear(saturate(rgb)) * alpha, alpha);
}

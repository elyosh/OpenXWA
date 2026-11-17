/*
 * Draws the HUD's panel image into a headset's eye image.
 *
 * The fixed group of the HUD is drawn once into a flat image. This shader
 * shows it on a flat panel in the cockpit: one matrix takes an eye pixel to
 * a point of the HUD image (src/xwa_remaster/hud_reproject_math.h).
 *
 * The HUD image has premultiplied alpha, and so has the output.
 */

Texture2D<float4> g_hud : register(t0, space2);
SamplerState g_hud_sampler : register(s0, space2);

cbuffer HudReproject : register(b0, space3)
{
	/* The rows of the matrix: (x, y, w) = M * (eye_ndc_x, eye_ndc_y, 1). */
	float4 row_x;
	float4 row_y;
	float4 row_w;
	/* xy 2 / the eye image's size in px, z the HUD's opacity. */
	float4 params;
	/* xy the eye image's upper left corner in its target, in px. */
	float4 origin;
};

float4 main(float4 position : SV_Position) : SV_Target0
{
	float2 pixel = position.xy - origin.xy;
	float3 eye = float3(pixel.x * params.x - 1.0, 1.0 - pixel.y * params.y, 1.0);
	float w = dot(row_w.xyz, eye);
	/* The HUD image is behind the eye. */
	if (w <= 0.0)
		discard;
	float2 hud = float2(dot(row_x.xyz, eye), dot(row_y.xyz, eye)) / w;
	if (abs(hud.x) > 1.0 || abs(hud.y) > 1.0)
		discard;
	float2 uv = float2(hud.x * 0.5 + 0.5, 0.5 - hud.y * 0.5);
	return g_hud.Sample(g_hud_sampler, uv) * params.z;
}

/* XWA hyperspace tunnel/flash textured geometry. */

Texture2D<float4> g_texture : register(t0, space2);
SamplerState      g_sampler : register(s0, space2);

float4 main(float4 position : SV_Position, float2 texcoord : TEXCOORD0, float4 color : TEXCOORD1)
	: SV_Target0 {
	float unusedValue = position.x * 0.0;
	return g_texture.Sample(g_sampler, texcoord) * color + float4(unusedValue, 0.0, 0.0, 0.0);
}

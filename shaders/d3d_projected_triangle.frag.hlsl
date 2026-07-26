/* XWA Direct3D v1 transformed/projected fragment compatibility path. */

Texture2D<float4> g_texture : register(t0, space2);
SamplerState      g_sampler : register(s0, space2);

cbuffer FragmentUniform : register(b0, space3) { float4 params; };

/* Must read every user interpolant from d3d_projected_triangle.vert; see the
 * signature-packing note there. */
float4 main(float4 color : TEXCOORD0, float2 texcoord : TEXCOORD1) : SV_Target0 {
	float4 tex = g_texture.Sample(g_sampler, texcoord);
	float4 output_color;
	float3 tex_gamma;
	float3 output_gamma;

	if (params.z != 0.0) {
		tex_gamma = saturate(tex.rgb);
	} else {
		tex_gamma = pow(saturate(tex.rgb), 1.0 / 2.2);
	}

	if (params.x == 1.0) {
		output_color.rgb = params.w != 0.0 ? tex_gamma : pow(tex_gamma, 2.2);
		output_color.a   = tex.a;
	} else {
		output_gamma     = tex_gamma * saturate(color.rgb);
		output_color.rgb = params.w != 0.0 ? saturate(output_gamma) : pow(saturate(output_gamma), 2.2);
		output_color.a   = tex.a * color.a;
		if (params.x == 4.0) {
			output_color.a = tex.a * color.a;
		}
	}

	if (params.y != 0.0 && output_color.a == 0.0) {
		discard;
	}

	return output_color;
}

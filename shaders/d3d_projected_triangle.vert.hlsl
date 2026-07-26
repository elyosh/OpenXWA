/* XWA Direct3D v1 transformed/projected vertex compatibility path. */

cbuffer ViewportUniform : register(b0, space1) {
	float4 viewport;
	float4 depth_params;
};

/* Mirrors D3DGpuVertex in d3d_compat.c, minus the D3DTLVERTEX specular colour:
 * the recovered renderer always writes it as 0 and the shim ignores
 * D3DRENDERSTATE_SPECULARENABLE, so that field is carried in the vertex stream
 * but not bound as an attribute. Semantic indices are dense because the
 * attribute locations in d3d_compat.c must match them. */
struct VertexInput {
	float4 screen : TEXCOORD0;
	float4 color : TEXCOORD1;
	float2 texcoord : TEXCOORD2;
};

/* Every user interpolant must be read by the fragment stage. ShaderCross packs
 * each stage's DXIL registers independently after its SPIR-V round trip, so an
 * ignored interpolant shifts the ones after it and makes D3D12 pipeline
 * creation fail with E_INVALIDARG. */
struct VertexOutput {
	float4 position : SV_Position;
	float4 color : TEXCOORD0;
	float2 texcoord : TEXCOORD1;
};

VertexOutput main(VertexInput input) {
	VertexOutput output;
	float        w;
	float        x_ndc;
	float        y_ndc;
	float        z_ndc;

	w     = input.screen.w != 0.0 ? 1.0 / input.screen.w : 1.0;
	x_ndc = ((input.screen.x - viewport.x) / viewport.z) * 2.0 - 1.0;
	y_ndc = 1.0 - ((input.screen.y - viewport.y) / viewport.w) * 2.0;
	z_ndc = input.screen.z * depth_params.x + depth_params.y;

	output.position = float4(x_ndc * w, y_ndc * w, z_ndc * w, w);
	output.color    = input.color;
	output.texcoord = input.texcoord;
	return output;
}

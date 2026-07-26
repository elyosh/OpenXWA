/* XWA hyperspace tunnel/flash textured geometry. */

struct VertexInput {
	float3 position : TEXCOORD0;
	float2 texcoord : TEXCOORD1;
	float4 color : TEXCOORD2;
};

struct VertexOutput {
	float4 position : SV_Position;
	float2 texcoord : TEXCOORD0;
	float4 color : TEXCOORD1;
};

VertexOutput main(VertexInput input) {
	VertexOutput output;

	output.position = float4(input.position, 1.0);
	output.texcoord = input.texcoord;
	output.color    = input.color;
	return output;
}

/* Fullscreen triangle for analytic hyperspace backgrounds. */

struct VertexOutput {
	float4 position : SV_Position;
};

VertexOutput main(uint vertex_id : SV_VertexID) {
	static const float2 positions[3] = {
		float2(-1.0f, -1.0f),
		float2(-1.0f, 3.0f),
		float2(3.0f, -1.0f),
	};

	VertexOutput output;
	output.position = float4(positions[vertex_id], 0.0f, 1.0f);
	return output;
}

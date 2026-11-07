/* Fullscreen triangle for the analytic hyperspace tunnel. */

cbuffer HyperspaceTunnelVS : register(b0, space1) {
	/* x depth of the triangle: 0 in the tunnel scene, the depth plane in
	 * front of the world during the jump's fades. */
	float4 depth;
};

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
	output.position = float4(positions[vertex_id], depth.x, 1.0f);
	return output;
}

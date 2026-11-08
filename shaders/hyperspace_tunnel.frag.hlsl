/* Analytic hyperspace tunnel. During the fades of the jump
 * (docs/hd_hyperspace_jump.md) it is drawn with partial opacity over the
 * black clear, before the additive streaks. */

#define XWA_HYPERSPACE_FRAGMENT
#include "hyperspace_tunnel_field.hlsli"

float4 main(float4 position : SV_Position) : SV_Target0 {
	float2 screen = position.xy / max(view.xy, float2(1.0f, 1.0f));
	float2 ndc = float2(screen.x * 2.0f - 1.0f, 1.0f - screen.y * 2.0f);
	float3 ray = float3((ndc.x - projection.z) * projection.x,
					   -(ndc.y - projection.w) * projection.y, 1.0f);

	float opacity = saturate(appearance.z);
	return float4(hyperspace_tunnel_radiance(ray) * opacity, opacity);
}

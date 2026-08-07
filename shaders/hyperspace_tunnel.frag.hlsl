/* Analytic backgrounds for XWA's hyperspace entry, transfer, and exit phases. */

cbuffer HyperspacePS : register(b0, space3) {
	/* x/y render size, z/w transition-flare center in screen UV. */
	float4 view;
	/* x/y tan(horizontal/vertical half-FOV), z/w projection offsets. */
	float4 projection;
	/* x simulation time, y axial speed, z angular speed, w simplex frequency scale. */
	float4 motion;
	/* x overall HDR brightness, y highlight strength, z transition alpha,
	 * w nonzero for the transition flare. */
	float4 appearance;
	/* x focal length, y axial twist, z cap radius, w cap falloff. */
	float4 geometry;
	float4 tunnel_right;
	float4 tunnel_up;
	float4 tunnel_forward;
	float4 dark_color;
	float4 body_color;
	float4 highlight_color;
	float4 cap_color;
};

static const float TWO_PI = 6.28318530718f;
/* MIT noise core adapted from shadertoy.com/view/Wtd3Wr by keiranhalcyon31 */
float3 hash33(float3 p) {
	p = frac(p * float3(0.1031f, 0.11369f, 0.13787f));
	p += dot(p, p.yxz + 19.19f);
	return -1.0f + 2.0f * frac(float3((p.x + p.y) * p.z, (p.x + p.z) * p.y, (p.y + p.z) * p.x));
}

float simplex_noise(float3 p) {
	static const float K1 = 1.0f / 3.0f;
	static const float K2 = 1.0f / 6.0f;
	float3 cell = floor(p + (p.x + p.y + p.z) * K1);
	float3 d0 = p - (cell - (cell.x + cell.y + cell.z) * K2);
	float3 order = step(float3(0.0f, 0.0f, 0.0f), d0 - d0.yzx);
	float3 i1 = order * (1.0f - order.zxy);
	float3 i2 = 1.0f - order.zxy * (1.0f - order);
	float3 d1 = d0 - (i1 - K2);
	float3 d2 = d0 - (i2 - 2.0f * K2);
	float3 d3 = d0 - (1.0f - 3.0f * K2);
	float4 falloff = max(0.6f - float4(dot(d0, d0), dot(d1, d1), dot(d2, d2), dot(d3, d3)), 0.0f);
	float4 contribution = falloff * falloff * falloff * falloff *
		float4(dot(d0, hash33(cell)), dot(d1, hash33(cell + i1)), dot(d2, hash33(cell + i2)),
			   dot(d3, hash33(cell + 1.0f)));
	return 31.316f * (contribution.x + contribution.y + contribution.z + contribution.w);
}

float fbm3(float3 p, float frequency_scale) {
	float sum = 0.0f;
	float scale = 13.0f * frequency_scale;
	float amplitude = 0.75f;

	[unroll]
	for (int octave = 0; octave < 5; ++octave) {
		sum += simplex_noise(p * scale) * amplitude;
		scale *= 2.0f;
		amplitude *= 0.5f;
	}
	return min(sum, 1.0f);
}

float4 main(float4 position : SV_Position) : SV_Target0 {
	float2 screen = position.xy / max(view.xy, float2(1.0f, 1.0f));
	if (appearance.w > 0.5f) {
		float2 offset = screen - view.zw;
		offset.x *= view.x / max(view.y, 1.0f);
		float radius_sq = dot(offset, offset);
		float halo = exp2(-3.0f * radius_sq);
		float core = exp2(-32.0f * radius_sq);
		float profile = saturate(0.2f + 0.55f * halo + 0.25f * core);
		float opacity = saturate(appearance.z) * profile;
		float3 color = cap_color.rgb * (appearance.x * appearance.y * opacity);
		return float4(color, opacity);
	}

	float2 ndc = float2(screen.x * 2.0f - 1.0f, 1.0f - screen.y * 2.0f);
	float3 ray = float3((ndc.x - projection.z) * projection.x,
					   -(ndc.y - projection.w) * projection.y, 1.0f);

	/* Resolve the ray around the player craft's forward axis so the reference
	 * polar mapping remains correct in forward, side, and rear views. */
	float3 axis = normalize(tunnel_forward.xyz);
	float axial_rate = dot(ray, axis);
	float3 radial_ray = ray - axis * axial_rate;
	float radial_length = length(radial_ray);
	float radial_rate = max(radial_length, 5.0e-5f);
	float projection_scale = max(projection.y, 1.0e-4f);
	float3 radial_direction = radial_ray / radial_rate;
	float angle = atan2(dot(radial_direction, tunnel_up.xyz),
						dot(radial_direction, tunnel_right.xyz));

	/* Match the reference's polar mapping while retaining XWA's configurable
	 * rates and the ship-relative tunnel axis. */
	float focal_depth = 0.1725f / max(geometry.x, 1.0e-4f);
	float polar_y = axial_rate * projection_scale / radial_rate * focal_depth + motion.x * motion.y * 0.25f;
	float angular_fraction = frac((angle + motion.x * motion.z) / TWO_PI);
	float2 polar = float2(angular_fraction + polar_y * geometry.y, polar_y) * float2(1.0f, 0.2f);
	float3 noise_coord = float3(polar, motion.x * 0.15f);
	float seam_blend = smoothstep(0.0f, 1.0f, angular_fraction);
	float broad = lerp(fbm3(noise_coord + float3(1.0f, 0.0f, 0.0f), motion.w),
					   fbm3(noise_coord, motion.w), seam_blend);
	broad = saturate(0.45f + 0.55f * broad);

	float3 color = lerp(dark_color.rgb, body_color.rgb, broad);
	color += highlight_color.rgb * (smoothstep(0.55f, 1.0f, broad) * 0.65f * appearance.y);
	color = saturate(color);

	/* Soft additive core based on the MIT shader at shadertoy.com/view/MttSz2.
	 * Axis distance keeps the halo aligned in side and rear views. */
	float axis_distance = radial_length / (max(abs(axial_rate), 1.0e-4f) * projection_scale);
	float cap_light = exp(-max(axis_distance - geometry.z, 0.0f) * geometry.w);
	color += cap_color.rgb * (cap_light * appearance.y);
	color *= appearance.x;

	return float4(color, 1.0f);
}

#ifndef XWA_HYPERSPACE_TUNNEL_FIELD_INCLUDED
#define XWA_HYPERSPACE_TUNNEL_FIELD_INCLUDED

#ifndef XWA_HYPERSPACE_UNIFORM_BINDING
#define XWA_HYPERSPACE_UNIFORM_BINDING register(b0, space3)
#endif

cbuffer HyperspacePS : XWA_HYPERSPACE_UNIFORM_BINDING {
	/* x/y render size, z/w unused. */
	float4 view;
	/* x/y tan(horizontal/vertical half-FOV), z/w projection offsets. */
	float4 projection;
	/* x simulation time, y axial speed, z angular speed, w simplex frequency scale. */
	float4 motion;
	/* x overall HDR brightness, y highlight strength, z opacity during the
	 * jump's fades, w unused. */
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
	/* The front of a look whose colours spread from the far end of the tunnel
	 * toward the camera. XwaRemasterHyperspaceFront explains the depth.
	 * x: the front's position, in log2 of the depth. Deeper walls have the
	 * look's colours.
	 * y: half the edge's thickness.
	 * z: how far the edge's bumps reach.
	 * w: 1 when the front is on, else 0. */
	float4 front;
	/* x: the size of the edge's noise.
	 * y: how much the edge curls.
	 * z: how much of the look's colour the cap has.
	 * w: unused. */
	float4 front_shape;
	float4 look_dark_color;
	float4 look_body_color;
	float4 look_highlight_color;
	float4 look_cap_color;
	/* The colour of the front's edge, in rgb. */
	float4 front_edge_color;
};

static const float HYPER_TWO_PI = 6.28318530718f;

/* MIT noise core adapted from shadertoy.com/view/Wtd3Wr by keiranhalcyon31. */
float3 hyper_hash33(float3 p) {
	p = frac(p * float3(0.1031f, 0.11369f, 0.13787f));
	p += dot(p, p.yxz + 19.19f);
	return -1.0f + 2.0f * frac(float3((p.x + p.y) * p.z, (p.x + p.z) * p.y, (p.y + p.z) * p.x));
}

float hyper_simplex_noise(float3 p) {
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
		float4(dot(d0, hyper_hash33(cell)), dot(d1, hyper_hash33(cell + i1)),
			   dot(d2, hyper_hash33(cell + i2)), dot(d3, hyper_hash33(cell + 1.0f)));
	return 31.316f * (contribution.x + contribution.y + contribution.z + contribution.w);
}

float hyper_fbm3(float3 p, float frequency_scale) {
	float sum = 0.0f;
	float scale = 13.0f * frequency_scale;
	float amplitude = 0.75f;

	[unroll]
	for (int octave = 0; octave < 5; ++octave) {
		sum += hyper_simplex_noise(p * scale) * amplitude;
		scale *= 2.0f;
		amplitude *= 0.5f;
	}
	return min(sum, 1.0f);
}

/* Three octaves of simplex noise, from about -1 to 1. */
float hyper_front_noise(float3 p) {
	return hyper_simplex_noise(p) * 0.6f + hyper_simplex_noise(p * 2.03f) * 0.3f +
		   hyper_simplex_noise(p * 4.01f) * 0.15f;
}

/* How far a point of the walls is behind the front, in log2 of the tunnel
 * depth: positive where the look's colours have arrived. The noise is laid
 * out by the angle around the tunnel (its cosine and sine, so it has no seam)
 * and the logarithm of the depth. On screen these change at about the same
 * rate, so the noise makes round lobes and curling fingers rather than
 * spikes. It drifts slowly toward the camera. */
float hyper_front_distance(float depth, float angle) {
	float log_depth = log2(max(depth, 1.0f / 64.0f));
	float3 wall = float3(cos(angle), sin(angle), log_depth * 0.6931472f - motion.x * 0.4f) * front_shape.x;
	float3 warp = float3(hyper_simplex_noise(wall + float3(5.2f, 1.3f, 0.0f)),
						 hyper_simplex_noise(wall + float3(1.7f, 9.2f, 0.0f)),
						 hyper_simplex_noise(wall + float3(8.3f, 2.8f, 0.0f)));
	float reach = hyper_front_noise(wall + warp * front_shape.y);
#ifndef XWA_HYPERSPACE_FRAGMENT
	/* The lighting cube also sees the walls beside and behind the camera.
	   They light most of the cockpit, but they are shallower than any wall
	   on screen, so the front would only reach them when it ends. Here the
	   depths from 2^-6 to 2^6 are squeezed into 2^-1 to 2^6: the walls at
	   60 degrees from the tunnel's axis turn about halfway through the
	   spread, and those beside and behind the camera when the screen's
	   corners do. The cockpit's light then warms up as the red grows. */
	if (log_depth < 6.0f) {
		log_depth = -1.0f + (log_depth + 6.0f) * (7.0f / 12.0f);
	}
#endif
	return log_depth - front.x + reach * front.z;
}

/* Evaluate the tunnel's linear HDR radiance for an arbitrary view-space ray. */
float3 hyperspace_tunnel_radiance(float3 ray) {
	float3 axis = normalize(tunnel_forward.xyz);
	float axial_rate = dot(ray, axis);
	float3 radial_ray = ray - axis * axial_rate;
	float radial_length = length(radial_ray);
	float radial_rate = max(radial_length, 5.0e-5f);
	float projection_scale = max(projection.y, 1.0e-4f);
	float3 radial_direction = radial_ray / radial_rate;
	float angle = atan2(dot(radial_direction, tunnel_up.xyz),
						dot(radial_direction, tunnel_right.xyz));

	float focal_depth = 0.1725f / max(geometry.x, 1.0e-4f);
	float polar_y = axial_rate * projection_scale / radial_rate * focal_depth + motion.x * motion.y * 0.25f;
	float angular_fraction = frac((angle + motion.x * motion.z) / HYPER_TWO_PI);
	float2 polar = float2(angular_fraction + polar_y * geometry.y, polar_y) * float2(1.0f, 0.2f);
	float3 noise_coord = float3(polar, motion.x * 0.15f);
	float seam_blend = smoothstep(0.0f, 1.0f, angular_fraction);
	float broad = lerp(hyper_fbm3(noise_coord + float3(1.0f, 0.0f, 0.0f), motion.w),
					   hyper_fbm3(noise_coord, motion.w), seam_blend);
	broad = saturate(0.45f + 0.55f * broad);

	float highlight = smoothstep(0.55f, 1.0f, broad) * 0.65f * appearance.y;
	float3 color = saturate(lerp(dark_color.rgb, body_color.rgb, broad) + highlight_color.rgb * highlight);
	float3 cap = cap_color.rgb;

	if (front.w > 0.0f) {
		/* Behind the front the walls have the look's colours, and a band of
		   the edge colour glows along the front itself. The two sets of
		   colours only mix inside this band, where the edge colour mostly
		   hides them. */
		float depth = axial_rate * projection_scale / radial_rate;
		float behind = hyper_front_distance(depth, angle);
		float width = front.y;
#ifdef XWA_HYPERSPACE_FRAGMENT
		/* Near the end of the tunnel the depth changes fast across a pixel;
		   the edge stays at least as wide as a pixel so that it doesn't
		   shimmer. */
		width = max(width, fwidth(behind) * 1.5f);
#endif
		float reached = smoothstep(-width, width, behind);
		float3 look = saturate(lerp(look_dark_color.rgb, look_body_color.rgb, broad) +
							   look_highlight_color.rgb * highlight);
		float edge = saturate(1.0f - abs(behind) / (width * 1.5f));
		color = lerp(lerp(color, look, reached), front_edge_color.rgb, edge * edge * 0.8f);
		cap = lerp(cap, look_cap_color.rgb, front_shape.z);
	}

	float axis_distance = radial_length / (max(abs(axial_rate), 1.0e-4f) * projection_scale);
	float cap_light = exp(-max(axis_distance - geometry.z, 0.0f) * geometry.w);
	color += cap * (cap_light * appearance.y);
	return color * appearance.x;
}

#endif

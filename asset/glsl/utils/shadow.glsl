
const vec2 CASCADE_OFFSETS[4] = vec2[](
    vec2(0, 0), vec2(0.5, 0), vec2(0, 0.5), vec2(0.5, 0.5)
);


float how_much_not_in_shadow_pcf_bilinear(const vec3 world_pos, const mat4 light_mat, sampler2D depth_map) {
    const vec4 frag_pos_in_dlight = light_mat * vec4(world_pos, 1);
    const vec3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    if (proj_coords.z > 1.0)
        return 1.0;
    if (proj_coords.z < 0.0)
        return 1.0;
    if (proj_coords.x > 1.0)
        return 1.0;
    if (proj_coords.x < -1.0)
        return 1.0;
    if (proj_coords.y > 1.0)
        return 1.0;
    if (proj_coords.y < -1.0)
        return 1.0;

    const vec2 sample_coord = proj_coords.xy * 0.5 + 0.5;
    const float current_depth = min(proj_coords.z, 0.99999);

    const vec2 texture_size = textureSize(depth_map, 0);
    const vec2 coord_frac = fract(sample_coord.xy * texture_size);

    const bool lit00 = current_depth < textureOffset(depth_map, sample_coord, ivec2(0, 0)).r;
    const bool lit01 = current_depth < textureOffset(depth_map, sample_coord, ivec2(0, 1)).r;
    const bool lit10 = current_depth < textureOffset(depth_map, sample_coord, ivec2(1, 0)).r;
    const bool lit11 = current_depth < textureOffset(depth_map, sample_coord, ivec2(1, 1)).r;

    const float lit_y0    = mix( lit00 ? 0.0 : 1.0,  lit10 ? 0.0 : 1.0, coord_frac.x);
    const float lit_y1    = mix( lit01 ? 0.0 : 1.0,  lit11 ? 0.0 : 1.0, coord_frac.x);
    const float lit_total = mix(lit_y0, lit_y1, coord_frac.y);

    return lit_total;
}


float how_much_not_in_cascade_shadow_bilinear(
    const vec3 world_pos,
    const vec2 offset,
    const mat4 light_mat,
    sampler2D depth_map
) {
    const vec4 frag_pos_in_dlight = light_mat * vec4(world_pos, 1);
    const vec3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    const vec2 sample_coord = (proj_coords.xy * 0.25 + 0.25) + offset;
    const float current_depth = min(proj_coords.z, 0.99999);

    const vec2 texture_size = textureSize(depth_map, 0);
    const vec2 coord_frac = fract(sample_coord.xy * texture_size);

    const bool lit00 = current_depth > textureOffset(depth_map, sample_coord, ivec2(0, 0)).r;
    const bool lit01 = current_depth > textureOffset(depth_map, sample_coord, ivec2(0, 1)).r;
    const bool lit10 = current_depth > textureOffset(depth_map, sample_coord, ivec2(1, 0)).r;
    const bool lit11 = current_depth > textureOffset(depth_map, sample_coord, ivec2(1, 1)).r;

    const float lit_y0    = mix( lit00 ? 0.0 : 1.0,  lit10 ? 0.0 : 1.0, coord_frac.x);
    const float lit_y1    = mix( lit01 ? 0.0 : 1.0,  lit11 ? 0.0 : 1.0, coord_frac.x);
    const float lit_total = mix(lit_y0, lit_y1, coord_frac.y);

    return lit_total;
}


float how_much_not_in_cascade_shadow_pcf(
    const vec3 world_pos,
    const vec2 offset,
    const mat4 light_mat,
    sampler2D depth_map
) {
    const vec4 frag_pos_in_dlight = light_mat * vec4(world_pos, 1);
    const vec3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;if (proj_coords.z > 1.0)
        return 1.0;
    if (proj_coords.z < 0.0)
        return 1.0;
    if (proj_coords.x > 1.0)
        return 1.0;
    if (proj_coords.x < -1.0)
        return 1.0;
    if (proj_coords.y > 1.0)
        return 1.0;
    if (proj_coords.y < -1.0)
        return 1.0;

    const vec2 sample_coord = (proj_coords.xy * 0.25 + 0.25) + offset;
    const float current_depth = min(proj_coords.z, 0.99999);

    const vec2 texture_size = textureSize(depth_map, 0);
    const vec2 texel_size = 1.0 / texture_size;
    const vec2 coord_frac = fract(sample_coord.xy * texture_size);

    uint total_iterations = 0;
    uint total_lit = 0;
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            const vec2 c = sample_coord + vec2(i, j) * texel_size;
            const float depth = texture(depth_map, c).r;
            total_iterations += 1;
            if (current_depth > depth)
                total_lit += 1;
        }
    }

    return float(total_lit) / float(total_iterations);
}


float how_much_not_in_cascade_shadow(
    const vec3 world_pos,
    const vec2 offset,
    const mat4 light_mat,
    sampler2D depth_map
) {
    return how_much_not_in_cascade_shadow_pcf(
        world_pos, offset, light_mat, depth_map
    );
}

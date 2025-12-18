import "../module/dither";
import "../module/konst";
import "../module/lighting";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
}


struct U_CompoSlightMain {
    float4x4 proj_;
    float4x4 proj_inv_;
    float4x4 view_;
    float4x4 view_inv_;
    float4 fog_color_density_;
    float mie_anisotropy_;
}


layout(set = 0, binding = 0) Sampler2D u_depth_map;
layout(set = 0, binding = 1) Sampler2D u_albedo_map;
layout(set = 0, binding = 2) Sampler2D u_normal_map;
layout(set = 0, binding = 3) Sampler2D u_material_map;
layout(set = 0, binding = 4) ConstantBuffer<U_CompoSlightMain> u_main;
layout(set = 1, binding = 0) Sampler2DShadow u_shadow_map;

[push_constant]
cbuffer U_CompoSlightPushConst {
    float4x4 light_mat_;
    float4 pos_n_inner_angle_;
    float4 dir_n_outer_angle_;
    float4 color_n_max_dist_;
}
u_pc;


float3 make_shadow_texco(const float3 frag_pos_v) {
    const float4 frag_pos_in_dlight = mul(u_pc.light_mat_, float4(frag_pos_v, 1));
    const float3 proj_coords = frag_pos_in_dlight.xyz / frag_pos_in_dlight.w;
    const float2 sample_coord = proj_coords.xy * 0.5 + 0.5;
    return float3(sample_coord, proj_coords.z);
}

[shader("vertex")]
VSOutput vert_main(int vtxid: SV_VertexID) {
    VSOutput output;
    output.pos_ = float4(FULLSCREEN_POS[vtxid], 0, 1);
    output.texco_ = FULLSCREEN_UV[vtxid];
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input, float4 screen_pos: SV_Position) {
    const float depth_texel = u_depth_map.Sample(input.texco_).r;
    const float4 albedo_texel = u_albedo_map.Sample(input.texco_);
    const float4 normal_texel = u_normal_map.Sample(input.texco_);
    const float4 material_texel = u_material_map.Sample(input.texco_);

    const float3 frag_pos = calc_frag_pos(depth_texel, input.texco_, u_main.proj_inv_);
    const float3 albedo = albedo_texel.rgb;
    const float3 normal = normalize(normal_texel.xyz * 2 - 1);
    const float roughness = material_texel.y;
    const float metallic = material_texel.z;

    let view_inv3 = float3x3(u_main.view_inv_);

    const float3 world_pos = mul(u_main.view_inv_, float4(frag_pos, 1)).xyz;
    const float3 world_normal = mul(view_inv3, normal);
    const float3 view_direc = normalize(frag_pos);
    const float3 world_direc = mul(view_inv3, view_direc);
    const float3 F0 = lerp(float3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);
    const float3 reflect_direc = reflect(view_direc, normal);
    const float3 world_reflect = mul(view_inv3, reflect_direc);
    float4 f_color = float4(0, 0, 0, 1);

    // Volumetric scattering
    {
        const int SAMPLE_COUNT = 5;
        const float INTENSITY = 0.2;

        const float light_factor =
            INTENSITY *
            phase_mie(dot(view_direc, u_pc.dir_n_outer_angle_.xyz), u_main.mie_anisotropy_) /
            float(SAMPLE_COUNT);
        const float3 vec_step = frag_pos / float(-SAMPLE_COUNT - 1);
        const float dither_value = get_dither_value(screen_pos.xy);

        for (int i = 0; i < SAMPLE_COUNT; ++i) {
            const float sample_factor = float(i + 0.5) * dither_value;
            const float3 sample_pos = frag_pos + vec_step * sample_factor;
            const float sample_dist = length(sample_pos);
            const float3 texco = make_shadow_texco(sample_pos);
            const float lit = u_shadow_map.SampleCmp(texco.xy, texco.z);

            const float attenuation = calc_slight_attenuation(
                                          sample_pos,
                                          u_pc.pos_n_inner_angle_.xyz,
                                          -u_pc.dir_n_outer_angle_.xyz,
                                          u_pc.pos_n_inner_angle_.w,
                                          u_pc.dir_n_outer_angle_.w
                                      ) *
                                      calc_attenuation(sample_dist, u_pc.color_n_max_dist_.w);

            f_color.xyz += u_pc.color_n_max_dist_.xyz * (light_factor * lit * attenuation);
        }
    }

    // Spotlight
    {
        const float3 light_pos = u_pc.pos_n_inner_angle_.xyz;
        const float3 to_light = normalize(light_pos - frag_pos);
        const float3 to_light_dir = u_pc.dir_n_outer_angle_.xyz;

        const float attenuation = calc_slight_attenuation(
                                      frag_pos,
                                      light_pos,
                                      -to_light_dir,
                                      u_pc.pos_n_inner_angle_.w,
                                      u_pc.dir_n_outer_angle_.w
                                  ) *
                                  calc_attenuation(frag_distance, u_pc.color_n_max_dist_.w);

        const float3 texco = make_shadow_texco(frag_pos);
        const float lit = u_shadow_map.SampleCmp(texco.xy, texco.z);

        f_color.xyz += calc_pbr_illumination(
                           roughness,
                           metallic,
                           albedo,
                           normal,
                           F0,
                           -view_direc,
                           to_light,
                           u_pc.color_n_max_dist_.xyz
                       ) *
                       attenuation * lit;
    }

    return f_color;
}

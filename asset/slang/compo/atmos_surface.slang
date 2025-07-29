import "../module/atmos";
import "../module/konst";
import "../module/lighting";
import "../module/shadow";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};


struct U_CompoAtmosSurfMain {
    float4x4 proj;
    float4x4 proj_inv;
    float4x4 view;
    float4x4 view_inv;
    float4 view_pos_w;
    float4 fog_color_density;
    float mie_anisotropy;
};

struct U_CompoDlightShadowMap {
    float4x4 light_mats[4];
    float4 cascade_depths;
    float4 dlight_color;
    float4 dlight_dir;
};


layout(set = 0, binding = 0) Sampler2D u_depth_map;
layout(set = 0, binding = 1) Sampler2D u_albedo_map;
layout(set = 0, binding = 2) Sampler2D u_normal_map;
layout(set = 0, binding = 3) Sampler2D u_material_map;
layout(set = 0, binding = 4) Sampler2D u_trans_lut;
layout(set = 0, binding = 5) Sampler2D u_multi_scat;
layout(set = 0, binding = 6) Sampler2D u_sky_view_lut;
layout(set = 0, binding = 7) Sampler3D u_cam_scat_vol;
layout(set = 0, binding = 8) ConstantBuffer<U_CompoAtmosSurfMain> u_main;

layout(set = 1, binding = 0) Sampler2DShadow u_shadow_map;
layout(set = 1, binding = 1) ConstantBuffer<U_CompoDlightShadowMap> ubuf_sh;


float3 get_transmittance(float3 frag_pos_w) {
    const AtmosphereParameters atmos_params = GetAtmosphereParameters();
    const float planet_radius = atmos_params.BottomRadius * 1000;
    const float3 frag_pos_e = frag_pos_w + float3(0, planet_radius, 0);
    const float frag_height_e = length(frag_pos_e);
    const float3 frag_up_dir_e = normalize(frag_pos_e);
    const float3 dlight_dir_w = normalize(mul(float3x3(u_main.view_inv), ubuf_sh.dlight_dir.xyz));
    const float view_zenith_cos_angle = dot(dlight_dir_w, frag_up_dir_e);
    const float2 lut_trans_uv = LutTransmittanceParamsToUv(
        atmos_params, frag_height_e * M_TO_KM, view_zenith_cos_angle
    );
    return u_trans_lut.SampleLevel(lut_trans_uv, 0).xyz;
}


[shader("vertex")]
VSOutput vert_main(int vtx_id: SV_VertexID) {
    VSOutput output;
    output.pos_ = float4(FULLSCREEN_POS[vtx_id], 0, 1);
    output.texco_ = FULLSCREEN_UV[vtx_id];
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) {
    const float depth_texel = u_depth_map.Sample(input.texco_).r;
    const float4 albedo_texel = u_albedo_map.Sample(input.texco_);
    const float4 normal_texel = u_normal_map.Sample(input.texco_);
    const float4 material_texel = u_material_map.Sample(input.texco_);

    const float3 frag_pos_v = calc_frag_pos(depth_texel, input.texco_, u_main.proj_inv);
    const float3 frag_pos_w = mul(u_main.view_inv, float4(frag_pos_v, 1)).xyz;

    float4 output = float4(0);

    // Aerial perspective
    {
        const float t_depth = length(frag_pos_w - u_main.view_pos_w.xyz);
        float slice = AerialPerspectiveDepthToSlice(t_depth);
        float weight = 1;
        if (slice < 0.5) {
            // We multiply by weight to fade to 0 at depth 0. That works for luminance and opacity.
            weight = clamp(slice * 2, 0, 1);
            slice = 0.5;
        }
        const float w = sqrt(slice * AP_SLICE_COUNT_RCP);  // squared distribution
        const float4 cam_scat_texel = u_cam_scat_vol.SampleLevel(float3(input.texco_, w), 0);
        output = cam_scat_texel * weight;
    }

    // Directional light
    {
        const float3 albedo = albedo_texel.rgb;
        const float3 normal_v = normalize(normal_texel.xyz * 2 - 1);
        const float roughness = material_texel.y;
        const float metallic = material_texel.z;
        const float3 F0 = lerp(float3(0.04), albedo, metallic);
        const float3 view_dir_v = normalize(frag_pos_v);

        const float3 transmittance = get_transmittance(frag_pos_w);
        const uint selected_dlight = select_cascade(depth_texel, ubuf_sh.cascade_depths);

        float lit = 1;
        const float3 texco = make_shadow_texco(
            frag_pos_v, ubuf_sh.light_mats[selected_dlight], selected_dlight
        );
        if (check_shadow_texco_range(texco))
            lit = u_shadow_map.SampleCmp(texco.xy, texco.z);

        output.xyz += lit * transmittance *
                      calc_pbr_illumination(
                          roughness,
                          metallic,
                          albedo,
                          normal_v,
                          F0,
                          -view_dir_v,
                          ubuf_sh.dlight_dir.xyz,
                          float3(1)
                      );
    }

    return output;
}

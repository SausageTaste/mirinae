import "../module/atmos";
import "../module/lighting";
import "../module/shadow";
import "../module/skinned";


struct VSInput {
    float3 pos_ : POSITION;
    float3 normal_ : NORMAL;
    float3 tangent_;
    float2 texcoord_ : TEXCOORD0;
    float4 j_weights_;
    int4 j_ids_;
};

struct VSOutput {
    float3x3 tbn_;
    float4 pos_ : SV_POSITION;
    float3 frag_pos_v_;
    float2 texco_ : TEXCOORD0;
};


struct U_GbufActorSkinned {
    float4x4 j_tforms_[MAX_JOINTS];
    float4x4 view_model_;
    float4x4 pvm_;
};

struct U_GbufModel {
    float roughness;
    float metallic;
};

struct U_TranspSkinnedFrame {
    float4x4 view;
    float4x4 view_inv;
    float4x4 proj;
    float4x4 proj_inv;

    // Directional light
    float4x4 dlight_mats[4];
    float4 dlight_dir;
    float4 dlight_color;
    float4 dlight_cascade_depths;

    float mie_anisotropy;
};


layout(set = 0, binding = 0) ConstantBuffer<U_TranspSkinnedFrame> u_main;
layout(set = 0, binding = 1) Sampler2DShadow u_dlight_shadow_maps[1];
layout(set = 0, binding = 2) Sampler2DShadow u_slight_shadow_maps[1];
layout(set = 0, binding = 3) SamplerCube u_env_diffuse;
layout(set = 0, binding = 4) SamplerCube u_env_specular;
layout(set = 0, binding = 5) Sampler2D u_env_lut;
layout(set = 0, binding = 6) Sampler2D u_trans_lut;
layout(set = 0, binding = 7) Sampler3D u_cam_scat_vol;
layout(set = 1, binding = 0) ConstantBuffer<U_GbufModel> u_model;
layout(set = 1, binding = 1) Sampler2D u_albedo_map;
layout(set = 1, binding = 2) Sampler2D u_normal_map;
layout(set = 1, binding = 3) Sampler2D u_orm_map;
layout(set = 2, binding = 0) ConstantBuffer<U_GbufActorSkinned> u_gbuf_actor;


[shader("vertex")]
VSOutput vert_main(VSInput input) {
    let joint_mat = make_joint_transform(input.j_ids_, input.j_weights_, u_gbuf_actor.j_tforms_);
    let after_joint = mul(joint_mat, float4(input.pos_, 1));
    let vmj3 = mul(float3x3(u_gbuf_actor.view_model_), float3x3(joint_mat));

    VSOutput output;
    output.tbn_ = make_tbn_mat(input.normal_, input.tangent_, vmj3);
    output.pos_ = mul(u_gbuf_actor.pvm_, after_joint);
    output.frag_pos_v_ = mul(u_gbuf_actor.view_model_, after_joint).xyz;
    output.texco_ = input.texcoord_;
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input, float4 screen_pos: SV_Position) : SV_Target {
    float4 output = float4(0, 0, 0, 1);

    const float4 albedo_texel = u_albedo_map.Sample(input.texco_);
    const float4 normal_texel = u_normal_map.Sample(input.texco_);

    const float depth = screen_pos.z;
    const float3 frag_pos_v = input.frag_pos_v_;
    const float3 albedo = albedo_texel.rgb;
    const float roughness = u_model.roughness;
    const float metallic = u_model.metallic;
    const float3 F0 = lerp(float3(0.04), albedo, metallic);

    float3 normal_v = normalize(mul(input.tbn_, (normal_texel.xyz * 2 - 1)));
    if (dot(normal_v, normalize(-frag_pos_v)) < 0)
        normal_v = -normal_v;

    let atmos_params = GetAtmosphereParameters();
    const float3x3 view_inv3 = float3x3(u_main.view_inv);
    const float3 frag_pos_w = mul(view_inv3, frag_pos_v);
    const float3 normal_w = mul(view_inv3, normal_v);
    const float3 view_pos_w = u_main.view_inv[3].xyz;
    const float3 view_dir_v = normalize(frag_pos_v);
    const float3 view_dir_w = mul(view_inv3, view_dir_v);
    const float frag_distance = length(frag_pos_v);

    let dlight_dir_w = mul(view_inv3, u_main.dlight_dir.xyz);

    // Aerial perspective
    {
        const float t_depth = length(frag_pos_w - view_pos_w);
        float slice = AerialPerspectiveDepthToSlice(t_depth);
        float weight = 1;
        if (slice < 0.5) {
            // We multiply by weight to fade to 0 at depth 0. That works for luminance and opacity.
            weight = clamp(slice * 2, 0, 1);
            slice = 0.5;
        }
        const float w = sqrt(slice * AP_SLICE_COUNT_RCP);  // squared distribution
        const float4 cam_scat_texel = u_cam_scat_vol.SampleLevel(float3(input.texco_, w), 0);
        output = weight * cam_scat_texel;
    }

    // IBL
    {
        output.xyz += ibl(
            normal_w,
            view_dir_w,
            albedo,
            F0,
            roughness,
            metallic,
            u_env_lut,
            u_env_diffuse,
            u_env_specular
        );
    }

    // Directional light
    {
        let transmittance = get_transmittance(
            frag_pos_w, dlight_dir_w, SamplerTransLut(u_trans_lut), atmos_params
        );
        let selected_dlight = select_cascade(depth, u_main.dlight_cascade_depths);
        let texco = make_shadow_texco(
            frag_pos_v, u_main.dlight_mats[selected_dlight], selected_dlight
        );

        float lit = 1;
        if (is_shadow_texco_in_range(texco))
            lit = u_dlight_shadow_maps[0].SampleCmp(texco.xy, texco.z);

        output.xyz += lit * transmittance *
                      calc_pbr_illumination(
                          roughness,
                          metallic,
                          albedo,
                          normal_v,
                          F0,
                          -view_dir_v,
                          u_main.dlight_dir.xyz,
                          float3(10)
                      );
    }

    output.a = albedo_texel.a;
    return output;
}

import "../module/konst";
import "../module/lighting";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
}


layout(set = 0, binding = 0) Sampler2D u_depth_map;
layout(set = 0, binding = 1) Sampler2D u_albedo_map;
layout(set = 0, binding = 2) Sampler2D u_normal_map;
layout(set = 0, binding = 3) Sampler2D u_material_map;
layout(set = 1, binding = 0) SamplerCube u_env_diffuse[1];
layout(set = 1, binding = 1) SamplerCube u_env_specular[1];
layout(set = 1, binding = 2) Sampler2D u_env_lut;

[push_constant]
cbuffer U_CompoEnvmapPushConst {
    float4x4 view_inv_;
    float4x4 proj_inv_;
    float4 fog_color_density_;
}
u_pc;


float3 ibl(
    const float3 normal,
    const float3 view_direc,
    const float3 albedo,
    const float3 f0,
    const float roughness,
    const float metallic,
    SamplerCube env_diffuse,
    SamplerCube env_specular
) {
    const float3 N = normalize(normal);
    const float3 V = -normalize(view_direc);
    const float3 R = reflect(-V, N);
    const float NoV = max(dot(N, V), 0.0);
    const float3 F = fresnel_schlick_rughness(NoV, f0, roughness);

    const float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    const float3 diffuse = env_diffuse.Sample(N).rgb * albedo;

    const float MAX_REFLECTION_LOD = 4.0;
    const float mip_lvl = roughness * MAX_REFLECTION_LOD;
    const float3 prefiltered_color = env_specular.SampleLevel(R, mip_lvl).rgb;
    const float2 env_brdf = u_env_lut.Sample(float2(NoV, roughness)).rg;
    const float3 specular = prefiltered_color * (F * env_brdf.x + env_brdf.y);

    return kD * diffuse + specular;
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
    let depth_texel = u_depth_map.Sample(input.texco_).r;
    let albedo_texel = u_albedo_map.Sample(input.texco_);
    let normal_texel = u_normal_map.Sample(input.texco_);
    let material_texel = u_material_map.Sample(input.texco_);

    const float3 frag_pos = calc_frag_pos(depth_texel, input.texco_, u_pc.proj_inv_);
    const float3 albedo = albedo_texel.rgb;
    const float3 normal = normalize(normal_texel.xyz * 2 - 1);
    const float roughness = material_texel.y;
    const float metallic = material_texel.z;

    let view_inv3 = float3x3(u_pc.view_inv_);

    const float3 world_normal = mul(view_inv3, normal);
    const float3 view_direc = normalize(frag_pos);
    const float3 world_direc = mul(view_inv3, view_direc);
    const float3 F0 = lerp(float3(0.04), albedo, metallic);
    const float frag_distance = length(frag_pos);

    const float3 light = ibl(
        world_normal,
        world_direc,
        albedo,
        F0,
        roughness,
        metallic,
        u_env_diffuse[0],
        u_env_specular[0]
    );

    return float4(light, 1);
}

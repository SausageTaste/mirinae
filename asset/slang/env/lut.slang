import "../module/envmap";
import "../module/konst";
import "../module/lighting";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};


[push_constant]
cbuffer U_EnvSkyPushConst {
    float4x4 proj_view_;
    float4 sun_dir_w_;
    float atmos_radius_bottom_;
}
u_pc;

layout(set = 0, binding = 0) Sampler2D u_equirectangular_map;


static const float2 POSITIONS[6] = {
    { 0, 0 }, { 0, 1 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 1, 0 },
};


static const float2 invAtan = float2(0.1591, 0.3183);
float2 map_cube(float3 v) {
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}


float2 integrate_brdf(float NdotV, float roughness) {
    float3 V;
    V.x = sqrt(1.0 - NdotV * NdotV);
    V.y = 0.0;
    V.z = NdotV;

    float A = 0.0;
    float B = 0.0;

    const float3 N = float3(0.0, 0.0, 1.0);

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        const float2 Xi = hammersley(i, SAMPLE_COUNT);
        const float3 H = importance_sample_ggx(Xi, N, roughness);
        const float3 L = normalize(2.0 * dot(V, H) * H - V);

        const float NdotL = max(L.z, 0.0);
        const float NdotH = max(H.z, 0.0);
        const float VdotH = max(dot(V, H), 0.0);

        if (NdotL > 0.0) {
            const float G = geometry_smith_ibl(N, V, L, roughness);
            const float G_Vis = (G * VdotH) / (NdotH * NdotV);
            const float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return float2(A, B);
}


[shader("vertex")]
VSOutput vert_main(int vtxid: SV_VertexID) {
    VSOutput output;
    output.pos_ = float4(POSITIONS[vtxid] * 2.0 - 1.0, 0, 1);
    output.texco_ = POSITIONS[vtxid];
    return output;
}


[shader("fragment")]
float2 frag_main(VSOutput input) : SV_Target {
    return integrate_brdf(input.texco_.x, input.texco_.y);
}

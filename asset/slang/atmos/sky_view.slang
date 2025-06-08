import "../module/atmos";

layout(set = 0, binding = 0, rgba16f) RWTexture2D<float4> out_image;
layout(set = 0, binding = 1) Sampler2D u_trans_lut;
layout(set = 0, binding = 2) Sampler2D u_multi_scat;

[push_constant]
cbuffer U_AtmosCamVolPushConst {
    float4x4 pv_inv;
    float4x4 proj_inv;
    float4x4 view_inv;
    float4 sun_direction;
    float4 view_pos;
}
u_pc;


[shader("compute")]
[numthreads(1, 1, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    let pixPos = float2(dispatch_id.xy) + 0.5;
    let Atmosphere = GetAtmosphereParameters();

    let tex_res = float2(192, 108);
    let uv = pixPos / tex_res;
    let view_inv3 = float3x3(u_pc.view_inv);
    const float3 ClipSpace = float3((pixPos / tex_res) * 2 - 1, 1);
    const float4 HViewPos = mul(u_pc.proj_inv, float4(ClipSpace, 1));
    float3 WorldDir = normalize(mul(view_inv3, (HViewPos.xyz / HViewPos.w)));
    float3 WorldPos = u_pc.view_pos.xyz / 1000 + float3(0, Atmosphere.BottomRadius, 0);

    const float viewHeight = length(WorldPos);

    float viewZenithCosAngle;
    float lightViewCosAngle;
    UvToSkyViewLutParams<true>(Atmosphere, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);

    float3 SunDir;
    {
        const float3 UpVector = WorldPos / viewHeight;
        const float sunZenithCosAngle = dot(UpVector, u_pc.sun_direction.xyz);
        SunDir = normalize(
            float3(sqrt(1 - sunZenithCosAngle * sunZenithCosAngle), sunZenithCosAngle, 0)
        );
    }

    WorldPos = float3(0, viewHeight, 0);

    const float viewZenithSinAngle = sqrt(1 - viewZenithCosAngle * viewZenithCosAngle);
    WorldDir = float3(
        viewZenithSinAngle * lightViewCosAngle,
        viewZenithCosAngle,
        viewZenithSinAngle * sqrt(1.0 - lightViewCosAngle * lightViewCosAngle)
    );

    // Move to top atmospehre
    if (!MoveToTopAtmosphere(WorldPos, WorldDir, Atmosphere.TopRadius)) {
        // Ray is not intersecting the atmosphere
        out_image[dispatch_id.xy] = float4(0, 0, 0, 1);
        return;
    }

    const bool ground = false;
    const float SampleCountIni = 30;
    const float DepthBufferValue = -1.0;
    const bool VariableSampleCount = true;
    const bool MieRayPhase = true;
    SingleScatteringResult ss = IntegrateScatteredLuminance(
        pixPos,
        tex_res,
        float2(4, 128),
        WorldPos,
        WorldDir,
        SunDir,
        float3(1),
        u_pc.pv_inv,
        SamplerTransLut(u_trans_lut),
        SamplerMulScatLut(u_multi_scat),
        Atmosphere,
        SampleCountIni,
        DepthBufferValue,
        VariableSampleCount,
        MieRayPhase
    );

    out_image[dispatch_id.xy] = float4(ss.L, 1);
}

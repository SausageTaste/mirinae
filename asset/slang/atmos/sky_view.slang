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
    let ClipSpace = float3((pixPos / tex_res) * 2 - 1, 1);
    let HViewPos = mul(u_pc.proj_inv, float4(ClipSpace, 1));
    let WorldPos = u_pc.view_pos.xyz / 1000 + float3(0, Atmosphere.BottomRadius, 0);
    let viewHeight = length(WorldPos);

    ScatLumiIntParams params;
    params.pixPos = pixPos;
    params.gResolution = tex_res;
    params.WorldPos = float3(0, viewHeight, 0);
    params.WorldDir = normalize(mul(view_inv3, (HViewPos.xyz / HViewPos.w)));
    params.gSkyInvViewProjMat = u_pc.pv_inv;
    params.SampleCountIni = 30;
    params.VariableSampleCount = true;
    params.MieRayPhase = true;

    {
        let UpVector = WorldPos / viewHeight;
        let cos_angle = dot(UpVector, u_pc.sun_direction.xyz);
        params.SunDir = normalize(float3(sqrt(1 - cos_angle * cos_angle), cos_angle, 0));
    }

    float viewZenithCosAngle;
    float lightViewCosAngle;
    UvToSkyViewLutParams<true>(Atmosphere, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);

    let viewZenithSinAngle = sqrt(1 - viewZenithCosAngle * viewZenithCosAngle);
    params.WorldDir.x = viewZenithSinAngle * lightViewCosAngle;
    params.WorldDir.y = viewZenithCosAngle;
    params.WorldDir.z = viewZenithSinAngle * sqrt(1.0 - lightViewCosAngle * lightViewCosAngle);

    // Move to top atmospehre
    if (!MoveToTopAtmosphere(params.WorldPos, params.WorldDir, Atmosphere.TopRadius)) {
        // Ray is not intersecting the atmosphere
        out_image[dispatch_id.xy] = float4(0, 0, 0, 1);
        return;
    }

    let ss = IntegrateScatteredLuminance(
        SamplerTransLut(u_trans_lut), SamplerMulScatLut(u_multi_scat), Atmosphere, params
    );

    out_image[dispatch_id.xy] = float4(ss.L, 1);
}

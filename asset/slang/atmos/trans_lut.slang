import "../module/atmos";

layout(set = 0, binding = 0, rgba16f) RWTexture2D<float4> out_image;

[push_constant]
cbuffer U_AtmosTransLutPushConst {
    float4x4 pv_inv_;
    int output_width_;
    int output_height_;
}
u_pc;

static const float PI = 3.14159265358979323846;


[shader("compute")]
[numthreads(16, 16, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    let pixPos = float2(dispatch_id.xy) + 0.5;
    let out_res = float2(u_pc.output_width_, u_pc.output_height_);
    let uv = pixPos / out_res;
    let Atmosphere = GetAtmosphereParameters();

    float viewHeight;
    float viewZenithCosAngle;
    UvToLutTransmittanceParams(Atmosphere, viewHeight, viewZenithCosAngle, uv);
    const float3 WorldPos = float3(0, viewHeight, 0);
    const float3 WorldDir = float3(
        0, viewZenithCosAngle, sqrt(1 - viewZenithCosAngle * viewZenithCosAngle)
    );

    // Can go a low as 10 sample but energy lost starts to be visible.
    const float SampleCountIni = 40;
    let ss = IntegrateScatteredLuminance(
        pixPos,
        out_res,
        float2(4, 128),
        WorldPos,
        WorldDir,
        float3(0, 1, 0),
        float3(1),
        u_pc.pv_inv_,
        NullTransLut(),
        NullMulScatLut(),
        Atmosphere,
        SampleCountIni
    );
    let transmittance = exp(-ss.optical_depth_);

    out_image[dispatch_id.xy] = float4(transmittance, 1);
}

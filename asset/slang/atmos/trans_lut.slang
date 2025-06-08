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
    float cos_angle;
    UvToLutTransmittanceParams(Atmosphere, viewHeight, cos_angle, uv);

    ScatLumiIntParams params;
    params.pixPos = pixPos;
    params.gResolution = out_res;
    params.WorldPos.y = viewHeight;
    params.WorldDir = float3(0, cos_angle, sqrt(1 - cos_angle * cos_angle));
    params.gSkyInvViewProjMat = u_pc.pv_inv_;
    // Can go a low as 10 sample but energy lost starts to be visible.
    params.SampleCountIni = 40;
    let ss = IntegrateScatteredLuminance(NullTransLut(), NullMulScatLut(), Atmosphere, params);
    let transmittance = exp(-ss.optical_depth_);

    out_image[dispatch_id.xy] = float4(transmittance, 1);
}

import "../module/atmos";


layout(set = 0, binding = 0) ConstantBuffer<AtmosphereParameters> u_atmos;
layout(set = 0, binding = 1, rgba16f) RWTexture2D<float4> out_image;

[push_constant]
cbuffer U_AtmosTransLutPushConst {
    float4x4 pv_inv_;
    int output_width_;
    int output_height_;
}
u_pc;


[shader("compute")]
[numthreads(16, 16, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    let pixPos = float2(dispatch_id.xy) + 0.5;
    let out_res = float2(u_pc.output_width_, u_pc.output_height_);
    let uv = pixPos / out_res;

    float viewHeight;
    float cos_angle;
    UvToLutTransmittanceParams(u_atmos, viewHeight, cos_angle, uv);

    ScatLumiIntParams params;
    params.pix_pos_ = pixPos;
    params.tex_res_ = out_res;
    params.world_pos_.y = viewHeight;
    params.world_dir_ = float3(0, cos_angle, sqrt(1 - cos_angle * cos_angle));
    params.pv_inv_ = u_pc.pv_inv_;
    params.sun_illuminance_ = 10;
    // Can go a low as 10 sample but energy lost starts to be visible.
    params.sample_count_ini_ = 40;
    let ss = IntegrateScatteredLuminance(NullTransLut(), NullMulScatLut(), u_atmos, params);
    let transmittance = exp(-ss.optical_depth_);

    out_image[dispatch_id.xy] = float4(transmittance, 1);
}

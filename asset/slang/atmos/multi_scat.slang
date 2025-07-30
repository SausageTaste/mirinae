import "../module/atmos";
import "../module/konst";


layout(set = 0, binding = 0, rgba16f) RWTexture2D<float4> out_image;
layout(set = 0, binding = 1) Sampler2D u_trans_lut;

[push_constant]
cbuffer U_AtmosMultiScatPushConst {
    float4x4 pv_inv_;
    int output_res_;
}
u_pc;

static const int SQRTSAMPLECOUNT = 8;

groupshared float3 g_multi_scat_as_1[64];
groupshared float3 g_L[64];


[shader("compute")]
[numthreads(1, 1, 64)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID, uint3 local_dispatch_id: SV_GroupThreadID) {
    let idx = int(local_dispatch_id.z);
    let pixPos = float2(dispatch_id.xy) + 0.5;
    let Atmosphere = GetAtmosphereParameters();

    float2 uv = pixPos / u_pc.output_res_;
    uv = float2(fromSubUvsToUnit(uv.x, u_pc.output_res_), fromSubUvsToUnit(uv.y, u_pc.output_res_));

    let cosSunZenithAngle = uv.x * 2 - 1;
    let sunDir = normalize(
        float3(0, cosSunZenithAngle, sqrt(saturate(1 - cosSunZenithAngle * cosSunZenithAngle)))
    );
    // We adjust again viewHeight according to PLANET_RADIUS_OFFSET to be in a valid range.
    let viewHeight = Atmosphere.radius_bottom +
                     saturate(uv.y + PLANET_RADIUS_OFFSET) *
                         (Atmosphere.radius_top - Atmosphere.radius_bottom -
                          PLANET_RADIUS_OFFSET);

    ScatLumiIntParams params;
    params.pix_pos_ = pixPos;
    params.tex_res_ = float2(u_pc.output_res_);
    params.world_pos_.y = viewHeight;
    params.world_dir_ = float3(0, 1, 0);
    params.sun_dir_ = sunDir;
    params.pv_inv_ = u_pc.pv_inv_;
    // a minimum set of step is required for accuracy unfortunately
    params.sample_count_ini_ = 20;
    params.ground_ = true;

    const float SphereSolidAngle = 4.0 * PI;
    const float IsotropicPhase = 1.0 / SphereSolidAngle;

    {
        const float sqrtSample = float(SQRTSAMPLECOUNT);
        const float i = 0.5 + float(idx / SQRTSAMPLECOUNT);
        const float j = 0.5 + float(idx - float((idx / SQRTSAMPLECOUNT) * SQRTSAMPLECOUNT));
        const float randA = i / sqrtSample;
        const float randB = j / sqrtSample;
        const float theta = 2 * PI * randA;
        // uniform distribution https://mathworld.wolfram.com/SpherePointPicking.html
        const float phi = acos(1 - 2 * randB);
        const float cosPhi = cos(phi);
        const float sinPhi = sin(phi);
        const float cosTheta = cos(theta);
        const float sinTheta = sin(theta);
        params.world_dir_.x = cosTheta * sinPhi;
        params.world_dir_.y = cosPhi;
        params.world_dir_.z = sinTheta * sinPhi;

        let result = IntegrateScatteredLuminance(
            SamplerTransLut(u_trans_lut), NullMulScatLut(), Atmosphere, params
        );

        g_multi_scat_as_1[idx] = result.MultiScatAs1 * SphereSolidAngle / (sqrtSample * sqrtSample);
        g_L[idx] = result.L * SphereSolidAngle / (sqrtSample * sqrtSample);
    }

    GroupMemoryBarrierWithGroupSync();
    // 64 to 32
    if (idx < 32) {
        g_multi_scat_as_1[idx] += g_multi_scat_as_1[idx + 32];
        g_L[idx] += g_L[idx + 32];
    }
    GroupMemoryBarrierWithGroupSync();
    // 32 to 16
    if (idx < 16) {
        g_multi_scat_as_1[idx] += g_multi_scat_as_1[idx + 16];
        g_L[idx] += g_L[idx + 16];
    }
    GroupMemoryBarrierWithGroupSync();
    // 16 to 8 (16 is thread group min hardware size with intel, no sync required from there)
    if (idx < 8) {
        g_multi_scat_as_1[idx] += g_multi_scat_as_1[idx + 8];
        g_L[idx] += g_L[idx + 8];
    }
    GroupMemoryBarrierWithGroupSync();
    if (idx < 4) {
        g_multi_scat_as_1[idx] += g_multi_scat_as_1[idx + 4];
        g_L[idx] += g_L[idx + 4];
    }
    GroupMemoryBarrierWithGroupSync();
    if (idx < 2) {
        g_multi_scat_as_1[idx] += g_multi_scat_as_1[idx + 2];
        g_L[idx] += g_L[idx + 2];
    }
    GroupMemoryBarrierWithGroupSync();
    if (idx < 1) {
        g_multi_scat_as_1[idx] += g_multi_scat_as_1[idx + 1];
        g_L[idx] += g_L[idx + 1];
    }
    GroupMemoryBarrierWithGroupSync();
    if (idx > 0)
        return;

    let MultiScatAs1 = g_multi_scat_as_1[0] * IsotropicPhase;  // Equation 7 f_ms
    let InScatteredLuminance = g_L[0] * IsotropicPhase;        // Equation 5 L_2ndOrder

    let r = MultiScatAs1;
    let SumOfAllMultiScatteringEventsContribution = 1 / (1 - r);
    // Equation 10 Psi_ms
    let L = InScatteredLuminance * SumOfAllMultiScatteringEventsContribution;

    const float MultipleScatteringFactor = 1;
    out_image[dispatch_id.xy] = float4(MultipleScatteringFactor * L, 1);
}

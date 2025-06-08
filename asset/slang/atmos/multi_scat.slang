#define MIRINAE_INTEGRATE_TRANS_LUT
#include "../module/atmos.slang"


layout(binding = 0) RWTexture2D<float4> out_image;
layout(set = 0, binding = 1) uniform Sampler2D u_trans_lut;

[push_constant]
cbuffer U_AtmosMultiScatPushConst {
    float4x4 pv_inv;
    int output_res;
}
u_pc;

static const int SQRTSAMPLECOUNT = 8;

groupshared float3 MultiScatAs1SharedMem[64];
groupshared float3 LSharedMem[64];


[shader("compute")]
[numthreads(1, 1, 64)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID, uint3 local_dispatch_id: SV_GroupThreadID) {
    let idx = int(local_dispatch_id.z);
    let pixPos = float2(dispatch_id.xy) + 0.5;

    float2 uv = pixPos / u_pc.output_res;
    uv = float2(fromSubUvsToUnit(uv.x, u_pc.output_res), fromSubUvsToUnit(uv.y, u_pc.output_res));

    const AtmosphereParameters Atmosphere = GetAtmosphereParameters();

    const float cosSunZenithAngle = uv.x * 2 - 1;
    const float3 sunDir = normalize(
        float3(0, cosSunZenithAngle, sqrt(saturate(1 - cosSunZenithAngle * cosSunZenithAngle)))
    );
    // We adjust again viewHeight according to PLANET_RADIUS_OFFSET to be in a valid range.
    let viewHeight = Atmosphere.BottomRadius +
                     saturate(uv.y + PLANET_RADIUS_OFFSET) *
                         (Atmosphere.TopRadius - Atmosphere.BottomRadius - PLANET_RADIUS_OFFSET);

    let WorldPos = float3(0, viewHeight, 0);
    var WorldDir = float3(0, 1, 0);

    const bool ground = true;
    // a minimum set of step is required for accuracy unfortunately
    const float SampleCountIni = 20;
    const float DepthBufferValue = -1.0;
    const bool VariableSampleCount = false;
    const bool MieRayPhase = false;

    const float SphereSolidAngle = 4.0 * PI;
    const float IsotropicPhase = 1.0 / SphereSolidAngle;

    const float sqrtSample = float(SQRTSAMPLECOUNT);
    const float i = 0.5 + float(idx / SQRTSAMPLECOUNT);
    const float j = 0.5 + float(idx - float((idx / SQRTSAMPLECOUNT) * SQRTSAMPLECOUNT));
    {
        const float randA = i / sqrtSample;
        const float randB = j / sqrtSample;
        const float theta = 2 * PI * randA;
        // uniform distribution https://mathworld.wolfram.com/SpherePointPicking.html
        const float phi = acos(1 - 2 * randB);
        const float cosPhi = cos(phi);
        const float sinPhi = sin(phi);
        const float cosTheta = cos(theta);
        const float sinTheta = sin(theta);
        WorldDir.x = cosTheta * sinPhi;
        WorldDir.y = cosPhi;
        WorldDir.z = sinTheta * sinPhi;
        let result = IntegrateScatteredLuminance(
            pixPos,
            float2(u_pc.output_res),
            float2(4, 128),
            WorldPos,
            WorldDir,
            sunDir,
            float3(1),
            u_pc.pv_inv,
            u_trans_lut,
            Atmosphere,
            SampleCountIni,
            DepthBufferValue,
            VariableSampleCount,
            MieRayPhase,
            ground
        );

        MultiScatAs1SharedMem[idx] = result.MultiScatAs1 * SphereSolidAngle /
                                     (sqrtSample * sqrtSample);
        LSharedMem[idx] = result.L * SphereSolidAngle / (sqrtSample * sqrtSample);
    }
    GroupMemoryBarrierWithGroupSync();
    // 64 to 32
    if (idx < 32) {
        MultiScatAs1SharedMem[idx] += MultiScatAs1SharedMem[idx + 32];
        LSharedMem[idx] += LSharedMem[idx + 32];
    }
    GroupMemoryBarrierWithGroupSync();
    // 32 to 16
    if (idx < 16) {
        MultiScatAs1SharedMem[idx] += MultiScatAs1SharedMem[idx + 16];
        LSharedMem[idx] += LSharedMem[idx + 16];
    }
    GroupMemoryBarrierWithGroupSync();
    // 16 to 8 (16 is thread group min hardware size with intel, no sync required from there)
    if (idx < 8) {
        MultiScatAs1SharedMem[idx] += MultiScatAs1SharedMem[idx + 8];
        LSharedMem[idx] += LSharedMem[idx + 8];
    }
    GroupMemoryBarrierWithGroupSync();
    if (idx < 4) {
        MultiScatAs1SharedMem[idx] += MultiScatAs1SharedMem[idx + 4];
        LSharedMem[idx] += LSharedMem[idx + 4];
    }
    GroupMemoryBarrierWithGroupSync();
    if (idx < 2) {
        MultiScatAs1SharedMem[idx] += MultiScatAs1SharedMem[idx + 2];
        LSharedMem[idx] += LSharedMem[idx + 2];
    }
    GroupMemoryBarrierWithGroupSync();
    if (idx < 1) {
        MultiScatAs1SharedMem[idx] += MultiScatAs1SharedMem[idx + 1];
        LSharedMem[idx] += LSharedMem[idx + 1];
    }
    GroupMemoryBarrierWithGroupSync();
    if (idx > 0)
        return;

    let MultiScatAs1 = MultiScatAs1SharedMem[0] * IsotropicPhase;  // Equation 7 f_ms
    let InScatteredLuminance = LSharedMem[0] * IsotropicPhase;     // Equation 5 L_2ndOrder

    let r = MultiScatAs1;
    let SumOfAllMultiScatteringEventsContribution = 1 / (1 - r);
    // Equation 10 Psi_ms
    let L = InScatteredLuminance * SumOfAllMultiScatteringEventsContribution;

    const float MultipleScatteringFactor = 1;
    out_image[dispatch_id.xy] = float4(MultipleScatteringFactor * L, 1);
}

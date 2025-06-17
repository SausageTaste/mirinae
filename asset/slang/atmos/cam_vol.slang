import "../module/atmos";

layout(set = 0, binding = 0, rgba16f) RWTexture3D<float4> out_image;
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

static const float OUT_TEX_RES = 32;


float4 gen_texel(uint3 dispatch_id) {
    let slice_id = float(dispatch_id.z);
    let pix_pos = float2(dispatch_id.xy) + 0.5;
    let Atmosphere = GetAtmosphereParameters();

    let view_inv3 = float3x3(u_pc.view_inv);
    let clip_space = float3(pix_pos / (OUT_TEX_RES) * 2 - 1, 0.5);
    let h_view_pos = mul(u_pc.proj_inv, float4(clip_space, 1.0));
    var world_dir = normalize(mul(view_inv3, (h_view_pos.xyz / h_view_pos.w)));


    float slice = (slice_id + 0.5) * AP_SLICE_COUNT_RCP;
    slice *= slice;  // squared distribution
    slice *= AP_SLICE_COUNT;

    let cam_pos = u_pc.view_pos.xyz * M_TO_KM + float3(0, Atmosphere.BottomRadius, 0);
    var world_pos = cam_pos;

    // Compute position from froxel information
    float tMax = AerialPerspectiveSliceToDepth(slice);
    float3 new_world_pos = world_pos + tMax * world_dir;

    // If the voxel is under the ground, make sure to offset it out on the ground.
    float view_height = length(new_world_pos);
    if (view_height <= (Atmosphere.BottomRadius + PLANET_RADIUS_OFFSET)) {
        // Apply a position offset to make sure no artefact are visible close to the earth
        // boundaries for large voxel.
        new_world_pos = normalize(new_world_pos) *
                        (Atmosphere.BottomRadius + PLANET_RADIUS_OFFSET + 0.001);
        world_dir = normalize(new_world_pos - cam_pos);
        tMax = length(new_world_pos - cam_pos);
    }
    float tMaxMax = tMax;

    // Move ray marching start up to top atmosphere.
    view_height = length(world_pos);
    if (view_height >= Atmosphere.TopRadius) {
        const float3 prev_world_pos = world_pos;
        if (!MoveToTopAtmosphere(world_pos, world_dir, Atmosphere.TopRadius)) {
            // Ray is not intersecting the atmosphere
            return float4(0, 0, 0, 1);
        }
        float LengthToAtmosphere = length(prev_world_pos - world_pos);
        if (tMaxMax < LengthToAtmosphere) {
            // tMaxMax for this voxel is not within earth atmosphere
            return float4(0, 0, 0, 1);
        }
        // Now world position has been moved to the atmosphere boundary: we need to reduce tMaxMax
        // accordingly.
        tMaxMax = max(0, tMaxMax - LengthToAtmosphere);
    }

    ScatLumiIntParams params;
    params.pixPos = pix_pos;
    params.gResolution = OUT_TEX_RES;
    params.WorldPos = world_pos;
    params.WorldDir = world_dir;
    params.SunDir = u_pc.sun_direction.xyz;
    params.gSkyInvViewProjMat = u_pc.pv_inv;
    params.SampleCountIni = max(1, (slice_id + 1) * 2);
    params.MieRayPhase = true;
    params.tMaxMax = tMaxMax;

    let ss = IntegrateScatteredLuminance(
        SamplerTransLut(u_trans_lut), SamplerMulScatLut(u_multi_scat), Atmosphere, params
    );

    const float transmittance = dot(ss.transmittance_, float3(1.0 / 3.0));
    return float4(ss.L, 1 - transmittance);
}


[shader("compute")]
[numthreads(1, 1, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    out_image[dispatch_id] = gen_texel(dispatch_id);
}

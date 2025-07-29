import "../module/atmos";
import "../module/konst";


static const float2 FULLSCREEN_POS[3] = {
    { -1, -1 },
    { 3, -1 },
    { -1, 3 },
};

static const float2 FULLSCREEN_UV[3] = {
    { 0, 0 },
    { 2, 0 },
    { 0, 2 },
};

struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};


[push_constant]
cbuffer U_CompoSkyAtmosMain {
    float4x4 proj_inv;
    float4x4 view_inv;
    float4 view_pos_w;
    float4 sun_direction_w;
}
u_pc;

layout(set = 0, binding = 0) Sampler2D u_trans_lut;
layout(set = 0, binding = 1) Sampler2D u_multi_scat;
layout(set = 0, binding = 2) Sampler2D u_sky_view_lut;
layout(set = 0, binding = 3) Sampler3D u_cam_scat_vol;


float3 pers_divide(const float4 clip_pos) {
    return clip_pos.xyz / clip_pos.w;
}

float3 get_sun_luminance(const float3 cam_pos_e, float3 cam_dir_w, const float planet_radius) {
    if (dot(cam_dir_w, u_pc.sun_direction_w.xyz) > cos(0.5 * 0.505 * PI / 180.0)) {
        const float t = raySphereIntersectNearest(
            cam_pos_e, cam_dir_w, float3(0.0, 0.0, 0.0), planet_radius
        );
        if (t < 0.0) {  // no intersection
            // arbitrary. But fine, not use when comparing the models
            return float3(1000000.0);
        }
    }

    return float3(0);
}


[shader("vertex")]
VSOutput vert_main(int vtx_id: SV_VertexID) {
    VSOutput output;
    output.pos_ = float4(FULLSCREEN_POS[vtx_id], 0, 1);
    output.texco_ = FULLSCREEN_UV[vtx_id];
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) {
    let atmos_params = GetAtmosphereParameters();

    const float4 clip_pos = float4(input.texco_ * 2 - 1, 1, 1);
    const float3 frag_pos_v = pers_divide(mul(u_pc.proj_inv, clip_pos));
    const float3 cam_dir_v = normalize(frag_pos_v);
    const float3 cam_dir_w = normalize(mul(float3x3(u_pc.view_inv), cam_dir_v));

    const float planet_radius = atmos_params.BottomRadius * 1000;
    const float3 cam_pos_e = u_pc.view_pos_w.xyz + float3(0, planet_radius, 0);
    const float cam_height_e = length(cam_pos_e);

    const float3 up_dir_e = normalize(cam_pos_e);
    const float view_zenith_cos_angle = dot(cam_dir_w, up_dir_e);

    const float3 sun_dir_w = u_pc.sun_direction_w.xyz;

    // assumes non parallel vectors
    const float3 side_dir_e = normalize(cross(up_dir_e, cam_dir_w));
    // aligns toward the sun light but perpendicular to up vector
    const float3 forward_dir_e = normalize(cross(side_dir_e, up_dir_e));
    const float2 light_on_plane = normalize(
        float2(dot(sun_dir_w, forward_dir_e), dot(sun_dir_w, side_dir_e))
    );
    const float light_view_cos_angle = light_on_plane.x;
    const bool intersect_ground = raySphereIntersectNearest(
                                      cam_pos_e, cam_dir_w, float3(0), planet_radius
                                  ) >= 0;

    const float2 uv = SkyViewLutParamsToUv<true>(
        atmos_params,
        intersect_ground,
        view_zenith_cos_angle,
        light_view_cos_angle,
        cam_height_e * M_TO_KM
    );

    const float4 sky_view_texel = u_sky_view_lut.SampleLevel(uv, 0);
    const float3 color = sky_view_texel.rgb +
                         get_sun_luminance(cam_pos_e, cam_dir_w, planet_radius);
    return float4(color, 1);
}

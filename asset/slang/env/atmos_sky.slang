import "../module/atmos";
import "../module/envmap";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float3 local_pos_;
};


layout(set = 0, binding = 0) Sampler2D u_sky_view_lut;

[push_constant]
cbuffer U_EnvSkyPushConst {
    float4x4 proj_view_;
    float4 sun_dir_w_;
    float atmos_radius_bottom_;
}
u_pc;


[shader("vertex")]
VSOutput vert_main(int vtx_id: SV_VertexID) {
    VSOutput output;
    output.local_pos_ = CUBE_VERTICES[vtx_id];

    let clip_pos = mul(float3x3(u_pc.proj_view_), output.local_pos_);
    output.pos_.xy = clip_pos.xy;
    output.pos_.z = 0;
    output.pos_.w = 1;

    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) : SV_Target {
    const float3 cam_dir_w = normalize(input.local_pos_);

    const float planet_radius = u_pc.atmos_radius_bottom_ * 1000;
    const float3 cam_pos_e = float3(0, planet_radius + 1, 0);
    const float cam_height_e = length(cam_pos_e);

    const float3 up_dir_e = normalize(cam_pos_e);
    const float view_zenith_cos_angle = dot(cam_dir_w, up_dir_e);

    const float3 sun_dir_w = u_pc.sun_dir_w_.xyz;

    // assumes non parallel vectors
    const float3 side_dir_e = normalize(cross(up_dir_e, cam_dir_w));
    // aligns toward the sun light but perpendicular to up vector
    const float3 forward_dir_e = normalize(cross(side_dir_e, up_dir_e));
    const float2 light_on_plane = normalize(
        float2(dot(sun_dir_w, forward_dir_e), dot(sun_dir_w, side_dir_e))
    );
    const float light_view_cos_angle = light_on_plane.x;
    const bool intersect_ground = raySphereIntersectNearest(
                                      cam_pos_e, cam_dir_w, float3(0, 0, 0), planet_radius
                                  ) >= 0.0;

    const float2 uv = SkyViewLutParamsToUv<true>(
        intersect_ground,
        u_pc.atmos_radius_bottom_,
        view_zenith_cos_angle,
        light_view_cos_angle,
        cam_height_e
    );

    const float4 sky_view_texel = u_sky_view_lut.SampleLevel(uv, 0);
    let color = sky_view_texel.rgb +
                get_sun_luminance(cam_pos_e, cam_dir_w, sun_dir_w, planet_radius);

    return float4(color, 1);
}

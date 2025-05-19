#version 450

#include "../utils/konst.glsl"
#include "../atmos/data.glsl"
#include "../atmos/integrate.glsl"

layout(location = 0) in vec2 v_uv_coord;

layout(location = 0) out vec4 f_color;


layout(push_constant) uniform U_CompoSkyAtmosMain {
    mat4 proj_inv;
    mat4 view_inv;
    vec4 view_pos_w;
    vec4 sun_direction_w;
} u_pc;

layout(set = 0, binding = 0) uniform sampler2D u_trans_lut;
layout(set = 0, binding = 1) uniform sampler2D u_multi_scat;
layout(set = 0, binding = 2) uniform sampler2D u_sky_view_lut;
layout(set = 0, binding = 3) uniform sampler2D u_cam_scat_vol;


vec2 SkyViewLutParamsToUv(
    const AtmosphereParameters atmos_params,
    const bool intersect_ground,
    const float view_zenith_cos_angle,
    const float light_view_cos_angle,
    const float cam_height_e
) {
    vec2 uv = vec2(0);

    const float Vhorizon = sqrt(cam_height_e * cam_height_e - atmos_params.BottomRadius * atmos_params.BottomRadius);
    const float CosBeta = Vhorizon / cam_height_e;  // GroundToHorizonCos
    const float Beta = acos(CosBeta);
    const float ZenithHorizonAngle = PI - Beta;

    if (!intersect_ground) {
        float coord = acos(view_zenith_cos_angle) / ZenithHorizonAngle;
        coord = 1.0 - coord;
        coord = sqrt(coord);
        coord = 1.0 - coord;
        uv.y = coord * 0.5f;
    } else {
        float coord = (acos(view_zenith_cos_angle) - ZenithHorizonAngle) / Beta;
        coord = sqrt(coord);
        uv.y = coord * 0.5 + 0.5;
    }

    {
        float coord = -light_view_cos_angle * 0.5 + 0.5;
        coord = sqrt(coord);
        uv.x = coord;
    }

    // Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
    return vec2(fromUnitToSubUvs(uv.x, 192.0), fromUnitToSubUvs(uv.y, 108.0));
}


vec3 get_sun_luminance(const vec3 cam_pos_e, vec3 cam_dir_w, const float PlanetRadius) {
    cam_dir_w.y = -cam_dir_w.y; // flip y axis to match the shader
    if (dot(cam_dir_w, u_pc.sun_direction_w.xyz) > cos(0.5 * 0.505 * PI / 180.0)) {
        const float t = raySphereIntersectNearest(cam_pos_e, cam_dir_w, vec3(0.0, 0.0, 0.0), PlanetRadius);
        if (t < 0.0) {  // no intersection
            // arbitrary. But fine, not use when comparing the models
            return vec3(1000000.0);
        }
    }

    return vec3(0);
}


void main() {
    const vec4 clip_pos = vec4(v_uv_coord * 2 - 1, 1, 1);
    const vec4 frag_pos = u_pc.proj_inv * clip_pos;
    const vec3 cam_dir_v = normalize(frag_pos.xyz / frag_pos.w);
    const vec3 cam_dir_w = normalize(mat3(u_pc.view_inv) * cam_dir_v);

    const AtmosphereParameters atmos_params = GetAtmosphereParameters();

    const vec3 cam_pos_e = u_pc.view_pos_w.xyz + vec3(0, atmos_params.BottomRadius, 0);
    const float cam_height_e = length(cam_pos_e);

    const vec3 up_dir_e = normalize(cam_pos_e);
    const float view_zenith_cos_angle = dot(cam_dir_w, up_dir_e);

    const vec3 sun_dir_w = u_pc.sun_direction_w.xyz;

    // assumes non parallel vectors
    const vec3 side_dir_e = normalize(cross(up_dir_e, cam_dir_w));
    // aligns toward the sun light but perpendicular to up vector
    const vec3 forward_dir_e = normalize(cross(side_dir_e, up_dir_e));
    const vec2 light_on_plane = normalize(vec2(
        dot(sun_dir_w, forward_dir_e),
        dot(sun_dir_w, side_dir_e)
    ));
    const float light_view_cos_angle = light_on_plane.x;
    const bool intersect_ground = raySphereIntersectNearest(
        cam_pos_e, cam_dir_w, vec3(0, 0, 0), atmos_params.BottomRadius
    ) >= 0.0;

    const vec2 uv = SkyViewLutParamsToUv(
        atmos_params,
        intersect_ground,
        view_zenith_cos_angle,
        light_view_cos_angle,
        cam_height_e
    );

    const vec4 sky_view_texel = textureLod(u_sky_view_lut, uv, 0);
    f_color.rgb = sky_view_texel.rgb + get_sun_luminance(cam_pos_e, cam_dir_w, atmos_params.BottomRadius);
    f_color.a = 1.0;
}

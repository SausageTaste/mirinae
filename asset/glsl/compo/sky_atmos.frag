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


const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 map_cube(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}


#define NONLINEARSKYVIEWLUT 1
void SkyViewLutParamsToUv(AtmosphereParameters Atmosphere,
bool IntersectGround,  float viewZenithCosAngle,  float lightViewCosAngle,  float viewHeight, out vec2 uv)
{
	float Vhorizon = sqrt(viewHeight * viewHeight - Atmosphere.BottomRadius * Atmosphere.BottomRadius);
	float CosBeta = Vhorizon / viewHeight;				// GroundToHorizonCos
	float Beta = acos(CosBeta);
	float ZenithHorizonAngle = PI - Beta;

	if (!IntersectGround)
	{
		float coord = acos(viewZenithCosAngle) / ZenithHorizonAngle;
		coord = 1.0 - coord;
#if NONLINEARSKYVIEWLUT
		coord = sqrt(coord);
#endif
		coord = 1.0 - coord;
		uv.y = coord * 0.5f;
	}
	else
	{
		float coord = (acos(viewZenithCosAngle) - ZenithHorizonAngle) / Beta;
#if NONLINEARSKYVIEWLUT
		coord = sqrt(coord);
#endif
		uv.y = coord * 0.5 + 0.5;
	}

	{
		float coord = -lightViewCosAngle * 0.5 + 0.5;
		coord = sqrt(coord);
		uv.x = coord;
	}

	// Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage visible)
	uv = vec2(fromUnitToSubUvs(uv.x, 192.0), fromUnitToSubUvs(uv.y, 108.0));
}


vec3 GetSunLuminance(vec3 WorldPos, vec3 WorldDir, float PlanetRadius)
{
    WorldDir.y = -WorldDir.y; // flip y axis to match the shader
    if (dot(WorldDir, u_pc.sun_direction_w.xyz) > cos(0.5*0.505*3.14159 / 180.0))
    {
        float t = raySphereIntersectNearest(WorldPos, WorldDir, vec3(0.0, 0.0, 0.0), PlanetRadius);
        if (t < 0.0) // no intersection
        {
            const vec3 SunLuminance = vec3(1000000.0); // arbitrary. But fine, not use when comparing the models
            return SunLuminance;
        }
    }

    return vec3(0);
}


void main() {
    const vec4 clip_pos = vec4(v_uv_coord * 2 - 1, 1, 1);
    const vec4 frag_pos = u_pc.proj_inv * clip_pos;
    const vec3 view_direc = normalize(frag_pos.xyz / frag_pos.w);
    const vec3 world_direc = (u_pc.view_inv * vec4(view_direc, 0)).xyz;
    //const vec2 uv = map_cube(normalize(world_direc));

    AtmosphereParameters Atmosphere = GetAtmosphereParameters();

    vec3 ClipSpace = vec3(v_uv_coord * 2 - 1, 1);
    vec4 HViewPos = u_pc.proj_inv * vec4(ClipSpace, 1.0);
    vec3 WorldDir = normalize(mat3(u_pc.view_inv) * (HViewPos.xyz / HViewPos.w));
    vec3 WorldPos = u_pc.view_pos_w.xyz + vec3(0, Atmosphere.BottomRadius, 0);

    float DepthBufferValue = -1.0;
    float viewHeight = length(WorldPos);
    vec3 L = vec3(0);

    vec2 uv;
    vec3 UpVector = normalize(WorldPos);
    float viewZenithCosAngle = dot(WorldDir, UpVector);

    vec3 sideVector = normalize(cross(UpVector, WorldDir));		// assumes non parallel vectors
    vec3 forwardVector = normalize(cross(sideVector, UpVector));	// aligns toward the sun light but perpendicular to up vector
    vec2 lightOnPlane = vec2(dot(u_pc.sun_direction_w.xyz, forwardVector), dot(u_pc.sun_direction_w.xyz, sideVector));
    lightOnPlane = normalize(lightOnPlane);
    float lightViewCosAngle = lightOnPlane.x;

    bool IntersectGround = raySphereIntersectNearest(WorldPos, WorldDir, vec3(0, 0, 0), Atmosphere.BottomRadius) >= 0.0;

    SkyViewLutParamsToUv(Atmosphere, IntersectGround, viewZenithCosAngle, lightViewCosAngle, viewHeight, uv);
    //uv.y = 1.0 - uv.y;


    //output.Luminance = vec4(SkyViewLutTexture.SampleLevel(samplerLinearClamp, pixPos / vec2(gResolution), 0).rgb + GetSunLuminance(WorldPos, WorldDir, Atmosphere.BottomRadius), 1.0);
    f_color = vec4(textureLod(u_sky_view_lut, uv, 0).rgb + GetSunLuminance(WorldPos, WorldDir, Atmosphere.BottomRadius), 1.0);
}

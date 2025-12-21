import "../module/atmos";
import "../module/konst";
import "../module/lighting";
import "../module/tess";
import "../module/shadow";


struct VSOutput {
    float3 pos_;
    float2 texco_;
}

struct HSOutput {
    float3 pos_;
    float2 texco_;
}

struct DSOutput {
    float4 pos_ : SV_POSITION;
    float4 lod_scales_;
    float4 frag_pos_scrn_;
    float3 frag_pos_;
    float2 uv_;
}


struct U_OceanTessParams {
    float4x4 light_mats_[4];
    float4x4 pvm_;
    float4x4 vm_;
    float4x4 view_;
    float4x4 view_inv_;
    float4x4 model_;
    float4 dlight_cascade_depths_;
    float4 dlight_color_;
    float4 dlight_dir;
    float4 jacobian_scale_;
    float4 len_scales_lod_scale_;
    float4 ocean_color_;
    float2 fbuf_size_;
    float foam_bias_;
    float foam_scale_;
    float foam_threshold_;
    float roughness_;
    float sss_base_;
    float sss_scale_;
    float tess_factor_;
    float patch_height_;
}

layout(set = 0, binding = 0) ConstantBuffer<U_OceanTessParams> u_params;
layout(set = 0, binding = 1) uniform Sampler2D u_disp_map[3];
layout(set = 0, binding = 2) uniform Sampler2D u_deri_map[3];
layout(set = 0, binding = 3) uniform Sampler2D u_turb_map[3];
layout(set = 0, binding = 4) uniform Sampler2D u_trans_lut;
layout(set = 0, binding = 5) uniform Sampler2D u_sky_view_lut;
layout(set = 0, binding = 6) uniform Sampler3D u_cam_scat_vol;
layout(set = 0, binding = 7) uniform Sampler2D u_sky_tex;
layout(set = 0, binding = 8) uniform Sampler2DArrayShadow u_shadow_map;

[push_constant]
cbuffer U_OceanTessPushConst {
    float4x4 patch_mat_;
    float2 patch_offset_;
    float2 patch_scale_;
}
u_pc;


static const float PLANET_BOTTOM = 6360;  // in km
static const float PLANET_TOP = 6460;     // in km

static const float3[] POSITIONS = {
    { 0, 0, 0 },
    { 0, 0, 1 },
    { 1, 0, 1 },
    { 1, 0, 0 },
};

static const float2 TEX_COORDS[] = {
    { 0, 0 },
    { 0, 1 },
    { 1, 1 },
    { 1, 0 },
};


float meanFresnel(float cosThetaV, float sigmaV) {
    return pow(1.0 - cosThetaV, 5.0 * exp(-2.69 * sigmaV)) / (1.0 + 22.7 * pow(sigmaV, 1.5));
}


float meanFresnel(float3 V, float3 N, float sigmaSq) {
    return meanFresnel(dot(V, N), sqrt(sigmaSq));
}


float reflectedSunRadiance(float3 V, float3 N, float3 L, float sigmaSq) {
    float3 H = normalize(L + V);

    float hn = dot(H, N);
    float p = exp(-2.0 * ((1.0 - hn * hn) / sigmaSq) / (1.0 + hn)) / (4.0 * PI * sigmaSq);

    float c = 1.0 - dot(V, H);
    float c2 = c * c;
    float fresnel = 0.02 + 0.98 * c2 * c2 * c;

    float zL = dot(L, N);
    float zV = dot(V, N);
    zL = max(zL, 0.01);
    zV = max(zV, 0.01);

    // brdf times cos(thetaL)
    return zL <= 0.0 ? 0.0 : max(fresnel * p * sqrt(abs(zL / zV)), 0.0);
}


float3 oceanRadiance(
    float3 V, float3 N, float3 L, float seaRoughness, float3 sunL, float3 skyE, float3 seaColor
) {
    float F = meanFresnel(V, N, seaRoughness);
    F = pow(F, 1.0 / 5.0);
    float3 Lsun = reflectedSunRadiance(V, N, L, seaRoughness) * sunL;
    float3 Lsky = skyE * F / PI;
    float3 Lsea = (1.0 - F) * seaColor * skyE / PI;
    return Lsun + Lsky + Lsea;
}


[shader("vertex")]
VSOutput vert_main(int vtx_id: SV_VertexID) {
    let pos_2d = TEX_COORDS[vtx_id] * u_pc.patch_scale_ + u_pc.patch_offset_;
    let pos_3d = float4(pos_2d.x, 0, pos_2d.y, 1);
    let pos_model = mul(u_pc.patch_mat_, pos_3d);
    let pos_world = mul(u_params.model_, pos_model);

    VSOutput output;
    output.pos_ = pos_model.xyz;
    output.texco_ = float2(pos_world.x, pos_world.z);
    return output;
}


TescFactors PatchConstantFunction(InputPatch<VSOutput, 4> patch) {
    TescFactors output;

    let points = float3[](
        persp_mul(u_params.pvm_, patch[0].pos_).xyz,
        persp_mul(u_params.pvm_, patch[1].pos_).xyz,
        persp_mul(u_params.pvm_, patch[2].pos_).xyz,
        persp_mul(u_params.pvm_, patch[3].pos_).xyz,
    );

    const float2 half_fbuf = u_params.fbuf_size_ * 0.5;
    const float2 s00 = (points[0].xy * half_fbuf + half_fbuf);
    const float2 s01 = (points[1].xy * half_fbuf + half_fbuf);
    const float2 s11 = (points[2].xy * half_fbuf + half_fbuf);
    const float2 s10 = (points[3].xy * half_fbuf + half_fbuf);

    const float edge0 = distance(s00, s01);
    const float edge1 = distance(s01, s11);
    const float edge2 = distance(s11, s10);
    const float edge3 = distance(s10, s00);

    let MAX_TESS_LEVEL = 64;
    output.tess_factors_[0] = clamp(edge3 * u_params.tess_factor_, 1, MAX_TESS_LEVEL);
    output.tess_factors_[1] = clamp(edge0 * u_params.tess_factor_, 1, MAX_TESS_LEVEL);
    output.tess_factors_[2] = clamp(edge1 * u_params.tess_factor_, 1, MAX_TESS_LEVEL);
    output.tess_factors_[3] = clamp(edge2 * u_params.tess_factor_, 1, MAX_TESS_LEVEL);
    output.interpolate_inside_factors();
    return output;
}

[shader("hull")]
[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("PatchConstantFunction")]
[maxtessfactor(64)]
HSOutput tesc_main(InputPatch<VSOutput, 4> patch, uint cpID: SV_OutputControlPointID) {
    HSOutput output;
    output.pos_ = patch[cpID].pos_;
    output.texco_ = patch[cpID].texco_;
    return output;
}


[shader("domain")]
[domain("quad")]
DSOutput tese_main(
    TescFactors input, float2 TessCoord: SV_DomainLocation, const OutputPatch<HSOutput, 4> patch
) {
    DSOutput output;

    let u = TessCoord.x;
    let v = TessCoord.y;

    let t00 = patch[0].texco_;
    let t01 = patch[1].texco_;
    let t11 = patch[2].texco_;
    let t10 = patch[3].texco_;

    let t0 = lerp(t00, t01, u);
    let t1 = lerp(t10, t11, u);
    let texco = lerp(t0, t1, v);

    {
        let p00 = patch[0].pos_;
        let p01 = patch[1].pos_;
        let p11 = patch[2].pos_;
        let p10 = patch[3].pos_;

        let p0 = lerp(p00, p01, u);
        let p1 = lerp(p10, p11, u);
        var p = lerp(p0, p1, v);

        const float p_dist = length((mul(u_params.vm_, float4(p, 1))).xyz);
        const float lod_c0 = min(
            u_params.len_scales_lod_scale_[3] * u_params.len_scales_lod_scale_[0] / p_dist, 1
        );
        const float lod_c1 = min(
            u_params.len_scales_lod_scale_[3] * u_params.len_scales_lod_scale_[1] / p_dist, 1
        );
        const float lod_c2 = min(
            u_params.len_scales_lod_scale_[3] * u_params.len_scales_lod_scale_[2] / p_dist, 1
        );

        let len_scales = u_params.len_scales_lod_scale_;
        float3 displacement = u_disp_map[0].SampleLevel(texco / len_scales[0], 0).xyz * lod_c0;
        const float largeWavesBias = displacement.y;
        displacement += u_disp_map[1].SampleLevel(texco / len_scales[1], 0).xyz * lod_c1;
        displacement += u_disp_map[2].SampleLevel(texco / len_scales[2], 0).xyz * lod_c2;
        p += displacement;

        output.lod_scales_ = float4(
            lod_c0,
            lod_c1,
            lod_c2,
            max(displacement.y - largeWavesBias * 0.8 - u_params.sss_base_, 0) / u_params.sss_scale_
        );
        output.frag_pos_ = mul(u_params.vm_, float4(p, 1)).xyz;
        output.frag_pos_scrn_ = mul(u_params.pvm_, float4(p, 1));
        output.pos_ = output.frag_pos_scrn_;
        output.uv_ = texco;
    }

    return output;
}


[shader("fragment")]
float4 frag_main(DSOutput input) {
    const float4x4 view_inv = u_params.view_inv_;
    const float3x3 view_inv3 = float3x3(view_inv);
    const float3 albedo = u_params.ocean_color_.xyz;
    const float roughness = u_params.roughness_;
    const float metallic = 0;
    const float3 F0 = lerp(float3(0.04), albedo, metallic);
    const float3 len_scales = u_params.len_scales_lod_scale_.xyz;

    float4 derivatives = float4(0);
    {
        derivatives += u_deri_map[0].Sample(input.uv_ / len_scales[0]);
        derivatives += u_deri_map[1].Sample(input.uv_ / len_scales[1]) * input.lod_scales_.y;
        derivatives += u_deri_map[2].Sample(input.uv_ / len_scales[2]) * input.lod_scales_.z;
    }

    const float2 slope = float2(
        derivatives.x / (1.0 + derivatives.z), derivatives.y / (1.0 + derivatives.w)
    );
    const float3 normal_m = normalize(float3(-slope.x, 1, -slope.y));
    const float3 normal_w = normalize(mul(float3x3(u_params.model_), normal_m));
    const float3 normal_v = normalize(mul(float3x3(u_params.view_), normal_w));
    const float3 frag_pos_w = (mul(view_inv, float4(input.frag_pos_, 1))).xyz;
    const float3 to_sun_dir_v = normalize(u_params.dlight_dir.xyz);
    const float3 to_sun_dir_w = normalize(mul(view_inv3, to_sun_dir_v));
    const float frag_dist = length(input.frag_pos_);
    const float3 cam_dir_v = normalize(input.frag_pos_);
    const float3 cam_dir_w = normalize(mul(view_inv3, cam_dir_v));
    const float3 cam_pos_w = float3(view_inv[0].w, view_inv[1].w, view_inv[2].w);
    const float3 sun_trans = get_transmittance(
        frag_pos_w, to_sun_dir_w, SamplerTransLut(u_trans_lut), PLANET_BOTTOM, PLANET_TOP
    );

    float3 light = float3(0);

    float lit = 1;
    {
        let selected_dlight = select_cascade(input.pos_.z, u_params.dlight_cascade_depths_);
        let texco = make_shadow_texco(input.frag_pos_, u_params.light_mats_[selected_dlight]);
        lit = u_shadow_map.SampleCmp(float3(texco.xy, selected_dlight), texco.z);
    }

    // Sky
    float3 sky_color = float3(0);
    {
        float3 cam_dir_reflec_w = reflect(cam_dir_w, normal_w);
        // if (cam_dir_reflec_w.y < 0) {
        //     cam_dir_reflec_w.y = 0;
        //     cam_dir_reflec_w = normalize(cam_dir_reflec_w);
        // }

        const float planet_radius = PLANET_BOTTOM * 1000;
        const float3 cam_pos_e = cam_pos_w + float3(0, planet_radius, 0);
        const float cam_height_e = length(cam_pos_e);

        const float3 up_dir_e = normalize(cam_pos_e);
        const float view_zenith_cos_angle = dot(cam_dir_reflec_w, up_dir_e);

        // assumes non parallel vectors
        const float3 side_dir_e = normalize(cross(up_dir_e, cam_dir_w));
        // aligns toward the sun light but perpendicular to up vector
        const float3 forward_dir_e = normalize(cross(side_dir_e, up_dir_e));
        const float2 light_on_plane = normalize(
            float2(dot(to_sun_dir_w, forward_dir_e), dot(to_sun_dir_w, side_dir_e))
        );
        const float light_view_cos_angle = light_on_plane.x;
        const bool intersect_ground = raySphereIntersectNearest(
                                          cam_pos_e, cam_dir_reflec_w, float3(0), planet_radius
                                      ) >= 0;

        const float2 uv = SkyViewLutParamsToUv<true>(
            intersect_ground,
            PLANET_BOTTOM,
            view_zenith_cos_angle,
            light_view_cos_angle,
            cam_height_e * M_TO_KM
        );

        const float4 sky_view_texel = u_sky_view_lut.SampleLevel(uv, 0);
        sky_color = sky_view_texel.xyz;
    }

    {
        const float3 to_cam_w = -cam_dir_w;
        light += oceanRadiance(
            to_cam_w, normal_m, to_sun_dir_w, roughness, sun_trans * 10 * lit, sky_color, albedo
        );
    }

    // Foam
    {
        float jacobian =
            u_turb_map[0].Sample(input.uv_ / len_scales[0]).x * u_params.jacobian_scale_[0] +
            u_turb_map[1].Sample(input.uv_ / len_scales[1]).x * u_params.jacobian_scale_[1] +
            u_turb_map[2].Sample(input.uv_ / len_scales[2]).x * u_params.jacobian_scale_[2];

        jacobian = (-jacobian + u_params.foam_bias_) * u_params.foam_scale_;
        jacobian = clamp(jacobian, 0, 1);
        jacobian *= clamp(u_params.len_scales_lod_scale_[3] / frag_dist, 0, 1);

        const float3 foam_light = calc_pbr_illumination(
            0.5, metallic, float3(1), normal_v, F0, -cam_dir_v, to_sun_dir_v, sun_trans * 10 * lit
        );

        light = lerp(light, foam_light, jacobian);
    }

    // Aerial perspective
    {
        const float t_depth = frag_dist;
        float slice = AerialPerspectiveDepthToSlice(t_depth);
        float weight = 1;
        if (slice < 0.5) {
            // We multiply by weight to fade to 0 at depth 0. That works for luminance and opacity.
            weight = clamp(slice * 2, 0, 1);
            slice = 0.5;
        }
        const float2 texco = input.frag_pos_scrn_.xy * (0.5 / input.frag_pos_scrn_.w) + 0.5;
        const float w = sqrt(slice * AP_SLICE_COUNT_RCP);  // squared distribution
        const float4 cam_scat_texel = u_cam_scat_vol.SampleLevel(float3(texco, w), 0);
        light += cam_scat_texel.xyz * weight;
    }

    return float4(light, 1);
}

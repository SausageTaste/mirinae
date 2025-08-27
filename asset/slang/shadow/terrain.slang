import "../module/tess";


struct VSInput {
    float3 pos_;
    float2 texco_;
};

struct VSOutput {
    float3 pos_ : SV_POSITION;
    float2 texco_;
};

struct HSOutput {
    float3 pos_ : SV_POSITION;
    float2 texco_ : TEXCOORD0;
}

struct DSOutput {
    float4 pos_ : SV_POSITION;
};


layout(set = 0, binding = 0) Sampler2D u_height_map;

[push_constant]
cbuffer U_ShadowTerrainPushConst {
    float4x4 pvm_;
    float2 fbuf_size_;
    float height_scale_;
    float tess_factor_;
}
u_pc;


[shader("vertex")]
VSOutput vert_main(VSInput input) {
    VSOutput output;
    output.pos_ = input.pos_;
    output.texco_ = input.texco_;
    return output;
}


TescFactors PatchConstantFunction(InputPatch<VSOutput, 4> patch) {
    TescFactors output;

    let height_vec = float3(0, u_pc.height_scale_, 0);
    let points = float3[](
        persp_mul(u_pc.pvm_, patch[0].pos_).xyz,
        persp_mul(u_pc.pvm_, patch[1].pos_).xyz,
        persp_mul(u_pc.pvm_, patch[2].pos_).xyz,
        persp_mul(u_pc.pvm_, patch[3].pos_).xyz,
        persp_mul(u_pc.pvm_, patch[0].pos_ + height_vec).xyz,
        persp_mul(u_pc.pvm_, patch[1].pos_ + height_vec).xyz,
        persp_mul(u_pc.pvm_, patch[2].pos_ + height_vec).xyz,
        persp_mul(u_pc.pvm_, patch[3].pos_ + height_vec).xyz,
    );

    if (!are_in_screen(points)) {
        output.set_zero();
        return output;
    }

    const float2 half_fbuf = u_pc.fbuf_size_ * 0.5;
    const float2 s00 = (points[0].xy * half_fbuf + half_fbuf);
    const float2 s01 = (points[1].xy * half_fbuf + half_fbuf);
    const float2 s11 = (points[2].xy * half_fbuf + half_fbuf);
    const float2 s10 = (points[3].xy * half_fbuf + half_fbuf);

    const float edge0 = distance(s00, s01);
    const float edge1 = distance(s01, s11);
    const float edge2 = distance(s11, s10);
    const float edge3 = distance(s10, s00);

    let MAX_TESS_LEVEL = 64;
    output.tess_factors_[0] = clamp(edge3 * u_pc.tess_factor_, 1, MAX_TESS_LEVEL);
    output.tess_factors_[1] = clamp(edge0 * u_pc.tess_factor_, 1, MAX_TESS_LEVEL);
    output.tess_factors_[2] = clamp(edge1 * u_pc.tess_factor_, 1, MAX_TESS_LEVEL);
    output.tess_factors_[3] = clamp(edge2 * u_pc.tess_factor_, 1, MAX_TESS_LEVEL);
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
        let height = u_height_map.Sample(texco).r * u_pc.height_scale_;

        let p00 = patch[0].pos_.xyz;
        let p01 = patch[1].pos_.xyz;
        let p11 = patch[2].pos_.xyz;
        let p10 = patch[3].pos_.xyz;

        let p0 = lerp(p00, p01, u);
        let p1 = lerp(p10, p11, u);
        var p = lerp(p0, p1, v);
        p.y += height;

        output.pos_ = mul(u_pc.pvm_, float4(p, 1));
    }

    return output;
}


[shader("fragment")]
void frag_main() {}

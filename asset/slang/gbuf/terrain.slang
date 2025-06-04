struct VSInput {
    float3 pos_;
    float2 texco_;
};

struct VSOutput {
    float3 pos_ : SV_POSITION;
    float2 texco_;
};

struct TescFactors {
    float tess_factors[4] : SV_TessFactor;
    float inside_tess_factors[2] : SV_InsideTessFactor;
};

struct HSOutput {
    float3 pos_ : SV_POSITION;
    float2 texco_ : TEXCOORD0;
}

struct DSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_ : TEXCOORD0;
};

struct FSOutput {
    float4 albedo_ : SV_Target0;
    float4 normal_ : SV_Target1;
    float4 material_ : SV_Target2;
};


layout(set = 0, binding = 0) Sampler2D u_height_map;
layout(set = 0, binding = 1) Sampler2D u_albedo_map;

[push_constant]
cbuffer U_GbufTerrainPushConst {
    float4x4 pvm_;
    float4x4 view_model_;
    float2 fbuf_size_;
    float2 len_per_texel_;
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
    float4 p00 = mul(u_pc.pvm_, float4(patch[0].pos_, 1));
    float4 p01 = mul(u_pc.pvm_, float4(patch[1].pos_, 1));
    float4 p11 = mul(u_pc.pvm_, float4(patch[2].pos_, 1));
    float4 p10 = mul(u_pc.pvm_, float4(patch[3].pos_, 1));
    p00 /= p00.w;
    p01 /= p01.w;
    p11 /= p11.w;
    p10 /= p10.w;

    p00.xy = (p00.xy * 0.5 + 0.5) * u_pc.fbuf_size_;
    p01.xy = (p01.xy * 0.5 + 0.5) * u_pc.fbuf_size_;
    p11.xy = (p11.xy * 0.5 + 0.5) * u_pc.fbuf_size_;
    p10.xy = (p10.xy * 0.5 + 0.5) * u_pc.fbuf_size_;

    let edge0 = distance(p00.xy, p01.xy);
    let edge1 = distance(p01.xy, p11.xy);
    let edge2 = distance(p11.xy, p10.xy);
    let edge3 = distance(p10.xy, p00.xy);

    let MAX_TESS_LEVEL = 64;
    let tess_level0 = clamp(edge3 * u_pc.tess_factor_, 0, MAX_TESS_LEVEL);
    let tess_level1 = clamp(edge0 * u_pc.tess_factor_, 0, MAX_TESS_LEVEL);
    let tess_level2 = clamp(edge1 * u_pc.tess_factor_, 0, MAX_TESS_LEVEL);
    let tess_level3 = clamp(edge2 * u_pc.tess_factor_, 0, MAX_TESS_LEVEL);

    TescFactors output;
    output.tess_factors[0] = tess_level0;
    output.tess_factors[1] = tess_level1;
    output.tess_factors[2] = tess_level2;
    output.tess_factors[3] = tess_level3;
    output.inside_tess_factors[0] = (tess_level1 + tess_level3) * 0.5;
    output.inside_tess_factors[1] = (tess_level0 + tess_level2) * 0.5;
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

    {
        let t00 = patch[0].texco_;
        let t01 = patch[1].texco_;
        let t11 = patch[2].texco_;
        let t10 = patch[3].texco_;

        let t0 = lerp(t00, t01, u);
        let t1 = lerp(t10, t11, u);
        output.texco_ = lerp(t0, t1, v);
    }

    {
        let height = u_height_map.SampleLevel(output.texco_, 0).r * u_pc.height_scale_;

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
FSOutput frag_main(DSOutput input) {
    FSOutput output;

    {
        let right = u_height_map.SampleLevel(input.texco_, 0, int2(2, 0)).r;
        let left = u_height_map.SampleLevel(input.texco_, 0, int2(-2, 0)).r;
        let up = u_height_map.SampleLevel(input.texco_, 0, int2(0, 2)).r;
        let down = u_height_map.SampleLevel(input.texco_, 0, int2(0, -2)).r;

        var normal = float3(
            u_pc.height_scale_ * (left - right) / (u_pc.len_per_texel_.x * 4),
            1,
            u_pc.height_scale_ * (down - up) / (u_pc.len_per_texel_.y * 4)
        );
        normal = mul(float3x3(u_pc.view_model_), normal);
        output.normal_.xyz = normalize(normal) * 0.5 + 0.5;
    }

    {
        let albedo_texel = u_albedo_map.Sample(input.texco_);
        output.albedo_ = float4(albedo_texel.xyz, 1);
    }

    output.material_ = float4(0, 0.9, 0, 0);
    return output;
}

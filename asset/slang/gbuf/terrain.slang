struct VSInput {
    float3 pos_;
    float2 texco_;
};

struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_;
};

struct TescFactors {
    float tess_factors[4] : SV_TessFactor;
    float inside_tess_factors[2] : SV_InsideTessFactor;
};

struct HSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_ : TEXCOORD0;
}

struct DSOutput {
    float4 pos_ : SV_POSITION;
    float3 normal_ : NORMAL;
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
    float4x4 pvm;
    float4x4 view;
    float4x4 model;
    float4 height_map_size_fbuf_size;
    float4 terrain_size;
    float height_scale;
    float tess_factor;
}
u_pc;


[shader("vertex")]
VSOutput vert_main(VSInput input) {
    VSOutput output;
    output.pos_.xyz = input.pos_;
    output.pos_.w = 1.0;
    output.texco_ = input.texco_;
    return output;
}


TescFactors PatchConstantFunction(InputPatch<VSOutput, 4> patch) {
    float4 p00 = mul(u_pc.pvm, patch[0].pos_);
    float4 p01 = mul(u_pc.pvm, patch[1].pos_);
    float4 p11 = mul(u_pc.pvm, patch[2].pos_);
    float4 p10 = mul(u_pc.pvm, patch[3].pos_);
    p00 /= p00.w;
    p01 /= p01.w;
    p11 /= p11.w;
    p10 /= p10.w;

    float2 fbuf_size = u_pc.height_map_size_fbuf_size.zw;
    p00.xy = (p00.xy * 0.5 + 0.5) * fbuf_size;
    p01.xy = (p01.xy * 0.5 + 0.5) * fbuf_size;
    p11.xy = (p11.xy * 0.5 + 0.5) * fbuf_size;
    p10.xy = (p10.xy * 0.5 + 0.5) * fbuf_size;

    float edge0 = distance(p00.xy, p01.xy);
    float edge1 = distance(p01.xy, p11.xy);
    float edge2 = distance(p11.xy, p10.xy);
    float edge3 = distance(p10.xy, p00.xy);

    const int MAX_TESS_LEVEL = 64;
    float tess_level0 = clamp(edge3 * u_pc.tess_factor, 0, MAX_TESS_LEVEL);
    float tess_level1 = clamp(edge0 * u_pc.tess_factor, 0, MAX_TESS_LEVEL);
    float tess_level2 = clamp(edge1 * u_pc.tess_factor, 0, MAX_TESS_LEVEL);
    float tess_level3 = clamp(edge2 * u_pc.tess_factor, 0, MAX_TESS_LEVEL);

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
    const float u = TessCoord.x;
    const float v = TessCoord.y;

    const float2 t00 = patch[0].texco_;
    const float2 t01 = patch[1].texco_;
    const float2 t11 = patch[2].texco_;
    const float2 t10 = patch[3].texco_;

    const float2 t0 = (t01 - t00) * u + t00;
    const float2 t1 = (t11 - t10) * u + t10;
    const float2 tex_coord = (t1 - t0) * v + t0;


    const float height = u_height_map.Sample(tex_coord).r * u_pc.height_scale;

    const float3 p00 = patch[0].pos_.xyz;
    const float3 p01 = patch[1].pos_.xyz;
    const float3 p11 = patch[2].pos_.xyz;
    const float3 p10 = patch[3].pos_.xyz;

    const float3 p0 = (p01 - p00) * u + p00;
    const float3 p1 = (p11 - p10) * u + p10;
    float3 p = (p1 - p0) * v + p0;
    p.y += height;

    let vm = mul(u_pc.view, u_pc.model);
    let normal = mul(float3x3(vm), float3(0, 1, 0));

    DSOutput output;
    output.pos_ = mul(u_pc.pvm, float4(p, 1));
    output.normal_ = normalize(normal) * 0.5 + 0.5;
    output.texco_ = tex_coord;
    return output;
}


[shader("fragment")]
FSOutput frag_main(DSOutput input) {
    FSOutput output;

    {
        let size_per_texel = u_pc.terrain_size.xy / u_pc.height_map_size_fbuf_size.xy;
        let right = u_height_map.Sample(input.texco_, int2(2, 0)).r;
        let left = u_height_map.Sample(input.texco_, int2(-2, 0)).r;
        let up = u_height_map.Sample(input.texco_, int2(0, 2)).r;
        let down = u_height_map.Sample(input.texco_, int2(0, -2)).r;

        var normal = float3(
            u_pc.height_scale * (left - right) / (size_per_texel.x * 4),
            1,
            u_pc.height_scale * (down - up) / (size_per_texel.y * 4)
        );
        let vm = mul(u_pc.view, u_pc.model);
        normal = mul(float3x3(vm), normal);
        output.normal_.xyz = normalize(normal) * 0.5 + 0.5;
    }

    {
        let albedo_texel = u_albedo_map.Sample(input.texco_);
        output.albedo_ = float4(albedo_texel.xyz, 1);
    }

    output.material_ = float4(0, 0.9, 0, 0);
    return output;
}

struct VSInput {
    float3 pos_;
    float3 normal_;
    float3 tangent_;
    float2 texco_;
};

struct VSOutput {
    float4 pos_ : SV_POSITION;
    float2 texco_ : TEXCOORD0;
};


[push_constant]
cbuffer U_ShadowPushConst {
    float4x4 pvm_;
}
u_pc;

layout(set = 1, binding = 1) Sampler2D u_albedo_map;


// https://github.com/hughsk/glsl-dither/blob/master/4x4.glsl
float dither4x4(float2 position, float brightness) {
    int x = int(fmod(position.x, 4.0));
    int y = int(fmod(position.y, 4.0));
    int index = x + y * 4;
    float limit = 0.0;

    if (x < 8) {
        if (index == 0)
            limit = 0.0625;
        if (index == 1)
            limit = 0.5625;
        if (index == 2)
            limit = 0.1875;
        if (index == 3)
            limit = 0.6875;
        if (index == 4)
            limit = 0.8125;
        if (index == 5)
            limit = 0.3125;
        if (index == 6)
            limit = 0.9375;
        if (index == 7)
            limit = 0.4375;
        if (index == 8)
            limit = 0.25;
        if (index == 9)
            limit = 0.75;
        if (index == 10)
            limit = 0.125;
        if (index == 11)
            limit = 0.625;
        if (index == 12)
            limit = 1.0;
        if (index == 13)
            limit = 0.5;
        if (index == 14)
            limit = 0.875;
        if (index == 15)
            limit = 0.375;
    }

    return brightness < limit ? 0.0 : 1.0;
}


[shader("vertex")]
VSOutput vert_main(VSInput input) {
    VSOutput output;
    output.pos_ = mul(u_pc.pvm_, float4(input.pos_, 1));
    output.texco_ = input.texco_;
    return output;
}


[shader("fragment")]
void frag_main(VSOutput input, float4 screen_pos: SV_Position) {
    let albedo_texel = u_albedo_map.Sample(input.texco_);
    let alpha = albedo_texel.a;
    let dithered = dither4x4(screen_pos.xy, pow(alpha, 1));
    if (dithered < 0.5)
        discard;
}

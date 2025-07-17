struct VSInput {
    float4 pos_;
    float4 color_;
};

struct VSOutput {
    float4 pos_ : SV_POSITION;
    float4 color_;
};


[shader("vertex")]
VSOutput vert_main(VSInput input) {
    VSOutput output;
    output.pos_ = input.pos_;
    output.color_ = input.color_;
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) {
    float4 color = input.color_;
    color.w *= 0.5;
    return color;
}

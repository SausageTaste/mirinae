import "../module/konst";


struct VSOutput {
    float4 pos_ : SV_POSITION;
    float3 local_pos_;
};


[push_constant]
cbuffer U_EnvdiffusePushConst {
    float4x4 proj_view_;
}
u_pc;

layout(set = 0, binding = 0) SamplerCube u_envmap;


[shader("vertex")]
VSOutput vert_main(int vtxid: SV_VertexID) {
    let local_pos = CUBE_VERTICES[vtxid];
    let clip_pos = mul(u_pc.proj_view_, float4(local_pos, 0));

    VSOutput output;
    output.local_pos_ = local_pos;
    output.pos_ = clip_pos.xyww;
    return output;
}


[shader("fragment")]
float4 frag_main(VSOutput input) : SV_Target {
    const float3 normal = normalize(input.local_pos_);
    const float3 right = cross(float3(0, 1, 0), normal);
    const float3 up = cross(normal, right);
    const float sample_delta = 0.025;

    float3 irradiance = float3(0);
    float num_samples = 0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sample_delta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sample_delta) {
            const float3 tangent_sample = float3(
                sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta)
            );
            const float3 sample_vec =
                (tangent_sample.x * right + tangent_sample.y * up + tangent_sample.z * normal);
            const float cos_x_sin = cos(theta) * sin(theta);
            irradiance += u_envmap.SampleLevel(sample_vec, 3).rgb * cos_x_sin;
            num_samples++;
        }
    }

    return float4(PI * irradiance * (1.0 / float(num_samples)), 1);
}

import "../module/complex";
import "../module/konst";


layout(binding = 0, rgba32f) RWTexture2D<float4> ppong_images[6];
layout(binding = 1, rgba32f) RWTexture2D<float4> hkt_images[6];

[push_constant]
cbuffer U_OceanNaiveIftPushConst {
    int N_;
    int stage_;  // 0: hor, 1: ver
}
u_pc;


[shader("compute")]
[numthreads(16, 16, 1)]
void comp_main(uint3 dispatch_id: SV_DispatchThreadID) {
    const int img_idx = int(dispatch_id.z);
    const int2 nm = int2(dispatch_id.xy);
    float4 sum = float4(0);

    if (0 == u_pc.stage_) {
        const float factor = TAU * float(nm.x) / float(u_pc.N_);

        for (int i = 0; i < u_pc.N_; ++i) {
            const float4 texel = hkt_images[img_idx][int2(i, nm.y)];
            let c = Complex::from_exp(float(i) * factor);
            sum.xy += (Complex(texel.xy) * c).to_float2();
            sum.zw += (Complex(texel.zw) * c).to_float2();
        }

        ppong_images[img_idx][nm] = sum;
    } else {
        const float factor = TAU * float(nm.y) / float(u_pc.N_);

        for (int i = 0; i < u_pc.N_; ++i) {
            const float4 texel = ppong_images[img_idx][int2(nm.x, i)];
            let c = Complex::from_exp(float(i) * factor);
            sum.xy += (Complex(texel.xy) * c).to_float2();
            sum.zw += (Complex(texel.zw) * c).to_float2();
        }

        hkt_images[img_idx][nm] = sum;
    }
}

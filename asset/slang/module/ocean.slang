module ocean;

import "konst";


float2 make_spatial_pos(uint2 dispatch_id, int N) {
    return float2(dispatch_id) - float(N) * 0.5;
}

float2 make_wave_vector(float2 spatial_pos, float L) {
    return spatial_pos * (TAU / L);
}

public float2 make_wave_vector(int2 dispatch_id, int N, float L) {
    return float2(dispatch_id * 2 - N) * (PI / L);
}

public float calc_angular_frequency(float magnitude, float depth) {
    return sqrt(GRAVITY_EARTH * magnitude * tanh(min(magnitude * depth, 20)));
}

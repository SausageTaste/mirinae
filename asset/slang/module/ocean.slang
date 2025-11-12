module ocean;

import "konst";


public float2 make_spatial_pos(uint2 dispatch_id, int N) {
    return float2(dispatch_id) - float(N) * 0.5;
}

public float2 make_wave_vector(float2 spatial_pos, float L) {
    return spatial_pos * (TAU / L);
}

public float calc_angular_frequency(float magnitude, float depth) {
    return sqrt(GRAVITY_EARTH * magnitude * tanh(min(magnitude * depth, 20)));
}

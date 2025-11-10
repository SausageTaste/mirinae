module ocean;

import "konst";


public float2 make_spatial_pos(uint2 dispatch_id, int N) {
    return float2(dispatch_id) - float(N) * 0.5;
}

public float2 make_wave_vector(float2 spatial_pos, float L) {
    return spatial_pos * (TAU / L);
}

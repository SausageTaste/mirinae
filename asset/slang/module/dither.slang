module dither;


// https://github.com/hughsk/glsl-dither/blob/master/4x4.glsl
public float dither4x4(float2 position, float brightness) {
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


public float get_dither_value(float2 position) {
    const float dither_pattern[16] = float[](
        0.0,
        0.5,
        0.125,
        0.625,
        0.75,
        0.22,
        0.875,
        0.375,
        0.1875,
        0.6875,
        0.0625,
        0.5625,
        0.9375,
        0.4375,
        0.8125,
        0.3125
    );

    const int i = int(position.x) % 4;
    const int j = int(position.y) % 4;
    return dither_pattern[4 * i + j];
}

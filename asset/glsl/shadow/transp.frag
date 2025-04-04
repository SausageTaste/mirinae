#version 450


layout(location = 0) in vec2 v_uv;

layout(set = 1, binding = 1) uniform sampler2D u_albedo_map;


// https://github.com/hughsk/glsl-dither/blob/master/4x4.glsl
float dither4x4(vec2 position, float brightness) {
  int x = int(mod(position.x, 4.0));
  int y = int(mod(position.y, 4.0));
  int index = x + y * 4;
  float limit = 0.0;

  if (x < 8) {
    if (index == 0) limit = 0.0625;
    if (index == 1) limit = 0.5625;
    if (index == 2) limit = 0.1875;
    if (index == 3) limit = 0.6875;
    if (index == 4) limit = 0.8125;
    if (index == 5) limit = 0.3125;
    if (index == 6) limit = 0.9375;
    if (index == 7) limit = 0.4375;
    if (index == 8) limit = 0.25;
    if (index == 9) limit = 0.75;
    if (index == 10) limit = 0.125;
    if (index == 11) limit = 0.625;
    if (index == 12) limit = 1.0;
    if (index == 13) limit = 0.5;
    if (index == 14) limit = 0.875;
    if (index == 15) limit = 0.375;
  }

  return brightness < limit ? 0.0 : 1.0;
}


void main() {
    const vec4 albedo_texel = texture(u_albedo_map, v_uv);
    const float alpha = albedo_texel.a;
    const float dithered = dither4x4(gl_FragCoord.xy, pow(alpha, 1));
    if (dithered < 0.5)
        discard;
}

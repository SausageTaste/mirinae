#version 450 core

layout (quads, fractional_odd_spacing, ccw) in;

in vec2 tc_uv[];

out vec3 te_normal;
out float te_height;

uniform sampler2D u_height_map;
uniform mat4 u_model_mat;
uniform mat4 u_view_mat;
uniform mat4 u_proj_mat;
uniform float u_cur_sec;


vec3 mod289(vec3 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
    return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 permute(vec4 x) {
    return mod289(((x*34.0)+10.0)*x);
}

vec4 taylorInvSqrt(vec4 r) {
    return 1.79284291400159 - 0.85373472095314 * r;
}

float snoise(vec3 v) {
    const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
    const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);

    // First corner
    vec3 i  = floor(v + dot(v, C.yyy) );
    vec3 x0 =   v - i + dot(i, C.xxx) ;

    // Other corners
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min( g.xyz, l.zxy );
    vec3 i2 = max( g.xyz, l.zxy );

    //   x0 = x0 - 0.0 + 0.0 * C.xxx;
    //   x1 = x0 - i1  + 1.0 * C.xxx;
    //   x2 = x0 - i2  + 2.0 * C.xxx;
    //   x3 = x0 - 1.0 + 3.0 * C.xxx;
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
    vec3 x3 = x0 - D.yyy;      // -1.0+3.0*C.x = -0.5 = -D.y

    // Permutations
    i = mod289(i);
    vec4 p = permute( permute( permute(
          i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
        + i.y + vec4(0.0, i1.y, i2.y, 1.0 ))
        + i.x + vec4(0.0, i1.x, i2.x, 1.0 ));

    // Gradients: 7x7 points over a square, mapped onto an octahedron.
    // The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
    float n_ = 0.142857142857; // 1.0/7.0
    vec3  ns = n_ * D.wyz - D.xzx;

    vec4 j = p - 49.0 * floor(p * ns.z * ns.z);  //  mod(p,7*7)

    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_ );    // mod(j,N)

    vec4 x = x_ *ns.x + ns.yyyy;
    vec4 y = y_ *ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4( x.xy, y.xy );
    vec4 b1 = vec4( x.zw, y.zw );

    //vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
    //vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
    vec4 s0 = floor(b0)*2.0 + 1.0;
    vec4 s1 = floor(b1)*2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0));

    vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
    vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;

    vec3 p0 = vec3(a0.xy,h.x);
    vec3 p1 = vec3(a0.zw,h.y);
    vec3 p2 = vec3(a1.xy,h.z);
    vec3 p3 = vec3(a1.zw,h.w);

    //Normalise gradients
    vec4 norm = taylorInvSqrt(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    // Mix final noise value
    vec4 m = max(0.5 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    m = m * m;
    return 105.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1),
                                   dot(p2,x2), dot(p3,x3) ) );
}

float color(vec2 xy) {
    return 0.7 * snoise(vec3(xy, 0.3 * u_cur_sec));
}

// https://stegu.github.io/webgl-noise/webdemo/
float calc_noise_at(vec2 p) {
    p *= 0.1;
    vec2 stepp = vec2(1.3, 1.7);
    float n = 0;
    n += color(p);
    n += 0.5 * color(p * 2.0 - stepp);
    n += 0.25 * color(p * 4.0 - 2.0 * stepp);
    n += 0.125 * color(p * 8.0 - 3.0 * stepp);
    n += 0.0625 * color(p * 16.0 - 4.0 * stepp);
    n += 0.03125 * color(p * 32.0 - 5.0 * stepp);
    return n * 1;
}


void main() {
    // get patch coordinate
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;

    // ----------------------------------------------------------------------
    // retrieve control point texture coordinates
    vec2 t00 = tc_uv[0];
    vec2 t10 = tc_uv[1];
    vec2 t11 = tc_uv[2];
    vec2 t01 = tc_uv[3];

    // bilinearly interpolate texture coordinate across patch
    vec2 t0 = (t10 - t00) * u + t00;
    vec2 t1 = (t11 - t01) * u + t01;
    vec2 tex_coord = (t1 - t0) * v + t0;

    // lookup texel at patch coordinate for height and scale + shift as desired
    te_height = texture(u_height_map, tex_coord).y * 64.0 - 16.0;

    // ----------------------------------------------------------------------
    // retrieve control point position coordinates
    vec4 p00 = gl_in[0].gl_Position;
    vec4 p10 = gl_in[1].gl_Position;
    vec4 p11 = gl_in[2].gl_Position;
    vec4 p01 = gl_in[3].gl_Position;

    // compute patch surface normal
    vec4 u_vec = p10 - p00;
    vec4 v_vec = p01 - p00;
    vec4 normal = normalize( vec4(cross(v_vec.xyz, u_vec.xyz), 0) );

    // bilinearly interpolate position coordinate across patch
    vec4 p0 = (p10 - p00) * u + p00;
    vec4 p1 = (p11 - p01) * u + p01;
    vec4 p = (p1 - p0) * v + p0;

    float hl = calc_noise_at(vec2(p.x - 1, p.z));
    float hr = calc_noise_at(vec2(p.x + 1, p.z));
    float hn = calc_noise_at(vec2(p.x, p.z + 1));
    float hf = calc_noise_at(vec2(p.x, p.z - 1));

    vec3 vertex_normal = vec3(0);
    vertex_normal.x = (hr - hl);
    vertex_normal.y = 2;
    vertex_normal.z = -(hn - hf);
    vertex_normal = normalize(vertex_normal);
    te_normal = vertex_normal;

    // displace point along normal
    p += normal * calc_noise_at(p.xz);

    // ----------------------------------------------------------------------
    // output patch point position in clip space
    gl_Position = u_proj_mat * u_view_mat * u_model_mat * p;
}

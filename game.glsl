#pragma sokol @ctype mat4 Mat4
#pragma sokol @ctype vec4 Vec4
#pragma sokol @ctype vec3 Vec3
#pragma sokol @ctype vec2 Vec2

#pragma sokol @block util
vec4 encode_depth(float v) {
    vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;
    enc = fract(enc);
    enc -= enc.yzww * vec4(1.0/255.0, 1.0/255.0, 1.0/255.0, 0.0);
    return enc;
}

float decode_depth(vec4 rgba) {
    return dot(rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0));
}

//  simple shadow map lookup, returns 0.0 (unlit) or 1.0 (lit)
float sample_shadow(sampler2D shadow_map, vec2 uv, float compare) {
    #if !SOKOL_GLSL
    uv.y = 1.0-uv.y;
    #endif
    float depth = decode_depth(texture(shadow_map, vec2(uv.x, uv.y)));
    depth += 0.001;
    return step(compare, depth);
}

//  perform percentage-closer shadow map lookup
float sample_shadow_pcf(sampler2D shadow_map, vec2 uv, vec2 sm_size, float compare) {
    float result = 0.0;
    for (int x =- 2; x <= 2; x++) {
        for (int y =- 2; y <= 2; y++) {
            vec2 off = vec2(x, y) / sm_size;
            result += sample_shadow(shadow_map, uv + off, compare);
        }
    }
    return result / 25.0;
}

//  gamma correction
vec4 gamma(vec4 c) {
    float p = 1.0/2.2;
    return vec4(pow(c.xyz, vec3(p, p, p)), c.w);
}
#pragma sokol @end

#pragma sokol @vs shadow_vs
uniform vs_shadow_params {
    mat4 mvp;
};

in vec4 position;
out vec2 proj_zw;

void main() {
    gl_Position = mvp * position;
    proj_zw = gl_Position.zw;
}
#pragma sokol @end

#pragma sokol @fs shadow_fs
#pragma sokol @include_block util
in vec2 proj_zw;
out vec4 frag_color;

void main() {
    frag_color = encode_depth(proj_zw.x / proj_zw.y);
}
#pragma sokol @end
#pragma sokol @program shadow shadow_vs shadow_fs

#pragma sokol @vs color_vs
uniform vs_color_params {
    mat4 view_proj;
    mat4 model;
    mat4 inv_trans_model;
    mat4 light_mvp;
    vec4 base_color;
    vec4 tint_color;
};

in vec4 pos;
in vec4 normal;
in vec2 texcoord0;

out vec3 norm;
out vec4 light_proj_pos;
out vec4 base;
out vec4 tint;
out vec2 uv;

void main() {
    gl_Position = view_proj * model * pos;
    light_proj_pos = light_mvp * pos;
    norm = mat3(inv_trans_model) * normal.xyz;
    base = base_color;
    tint = tint_color;
    uv = texcoord0;
}
#pragma sokol @end

#pragma sokol @fs color_fs
#pragma sokol @include_block util
uniform fs_color_params {
    vec3 light_dir;
    vec2 shadow_map_size;
};
uniform sampler2D tex;
uniform sampler2D shadow_map;

in vec3 norm;
in vec4 light_proj_pos;
in vec4 base;
in vec4 tint;
in vec2 uv;

out vec4 frag_color;

void main() {
    vec3 light_color = vec3(0.98, 0.98, 0.58);
    vec3 ambient = light_color * 0.6;

    float n_dot_l = dot(normalize(norm), -light_dir);
    vec3 diffuse = vec3(0.0, 0.0, 0.0);
    if (n_dot_l > 0.0) {
        vec3 light_pos = light_proj_pos.xyz / light_proj_pos.w;
        vec2 sm_uv = (light_pos.xy+1.0)*0.5;
        float depth = light_pos.z;
        float s = sample_shadow_pcf(shadow_map, sm_uv, shadow_map_size, depth);

        diffuse = max(n_dot_l, 0.0) * light_color * 0.4;
    }

    vec4 tex_color = texture(tex, uv);
    frag_color = (base + tint * tex_color) * vec4((ambient + diffuse), 1.0);
}
#pragma sokol @end

#pragma sokol @program color color_vs color_fs

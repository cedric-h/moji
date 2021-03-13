#pragma sokol @ctype mat4 Mat4
#pragma sokol @ctype vec4 Vec4
#pragma sokol @ctype vec3 Vec3
#pragma sokol @ctype vec2 Vec2

//  float/rgba8 encoding/decoding so that we can use an RGBA8
//  shadow map instead of floating point render targets which might
//  not be supported everywhere
//
//  http://aras-p.info/blog/2009/07/30/encoding-floats-to-rgba-the-final/
#pragma sokol @block util
vec4 encodeDepth(float v) {
    vec4 enc = vec4(1.0, 255.0, 65025.0, 16581375.0) * v;
    enc = fract(enc);
    enc -= enc.yzww * vec4(1.0/255.0,1.0/255.0,1.0/255.0,0.0);
    return enc;
}

float decodeDepth(vec4 rgba) {
    return dot(rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0));
}

//  perform simple shadow map lookup returns 0.0 (unlit) or 1.0 (lit)
float sampleShadow(sampler2D shadowMap, vec2 uv, float compare) {
    #if !SOKOL_GLSL
    uv.y = 1.0-uv.y;
    #endif
    float depth = decodeDepth(texture(shadowMap, vec2(uv.x, uv.y)));
    // depth -= 0.0004;
    return step(compare, depth);
}

float sampleShadowPCF(sampler2D shadowMap, vec2 uv, vec2 smSize, float compare) {
    float result = 0.0;
    for (float x = -1.5; x <= 1.5; x += 1.0) {
        for (float y = -1.5; y <= 1.5; y += 1.0) {
            vec2 off = vec2(x,y)/smSize;
            result += sampleShadow(shadowMap, uv+off, compare);
        }
    }
    return result / 16.0;
}

//  perform gamma correction
vec4 gamma(vec4 c) {
    float p = 1.0/2.2;
    return vec4(pow(c.xyz, vec3(p, p, p)), c.w);
}
#pragma sokol @end

#pragma sokol @block shadowUniforms
uniform vs_shadow_params {
    mat4 mvp;
    vec2 min_tex;
    vec2 max_tex;
};
#pragma sokol @end
//  Shadowmap pass shaders
#pragma sokol @vs shadowVS
#pragma sokol @include_block shadowUniforms

in vec4 position;
in vec2 uv;
out vec2 projZW;
out vec2 tex_coord;

void main() {
    gl_Position = mvp * position;
    projZW = gl_Position.zw;
    tex_coord = mix(min_tex, max_tex, uv);
}
#pragma sokol @end

#pragma sokol @fs shadowFS
#pragma sokol @include_block util
in vec2 projZW;
in vec2 tex_coord;
uniform sampler2D tex;

out vec4 fragColor;

void main() {
    if (texture(tex, tex_coord).w == 0.0) discard;
    float depth = projZW.x / projZW.y;
    fragColor = encodeDepth(depth);
}
#pragma sokol @end

#pragma sokol @program shadow shadowVS shadowFS

#pragma sokol @block muiUniforms
uniform mui_params {
    mat4 mvp;
};
#pragma sokol @end
#pragma sokol @vs muiVS
#pragma sokol @include_block muiUniforms
in vec2 position;
in vec2 tex0;
in vec4 color0;

out vec4 color;
out vec2 uv;

void main() {
  gl_Position = mvp * vec4(position, 0, 1);
  color = color0;
  uv = tex0;
}
#pragma sokol @end

#pragma sokol @fs muiFS
uniform sampler2D tex;

in vec4 color;
in vec2 uv;

out vec4 frag_color;
void main() {
  frag_color = texture(tex, uv) * color;
}
#pragma sokol @end

#pragma sokol @program mui muiVS muiFS

//  Color pass shaders
#pragma sokol @block colorUniforms
uniform vs_light_params {
    mat4 model;
    vec2 min_tex;
    vec2 max_tex;
    mat4 mvp;
    mat4 lightMVP;
    vec4 tintColor;
    vec4 baseColor;
};
#pragma sokol @end

#pragma sokol @vs colorVS
#pragma sokol @include_block colorUniforms
in vec4 position;
in vec2 uv;
in vec3 normal;

out vec4 lightProjPos;
out vec4 P;
out vec3 N;
out vec4 tint;
out vec4 base;
out vec2 tex_coord;

void main() {
    gl_Position = mvp * position;
    lightProjPos = lightMVP * position;
    P = (model * position);
    N = (model * vec4(normal, 0.0)).xyz;
    tint = tintColor;
    base = baseColor;
    tex_coord = mix(min_tex, max_tex, uv);
}
#pragma sokol @end

#pragma sokol @fs colorFS
#pragma sokol @include_block util
in vec4 lightProjPos;
in vec4 P;
in vec3 N;
in vec4 tint;
in vec4 base;
in vec2 tex_coord;

uniform fs_light_params {
    vec2 shadowMapSize;
    vec3 lightDir;
    vec3 eyePos;
};
uniform sampler2D shadowMap;
uniform sampler2D art;

out vec4 fragColor;

void main() {
    vec3 light_color = vec3(0.78, 0.88, 0.28);
    float specPower = 2.2;
    float ambientIntensity = 0.25;

    // diffuse lighting
    vec3 l = normalize(lightDir);
    vec3 n = normalize(N);

    fragColor = max(base, texture(art, tex_coord) * tint);
    float alpha = fragColor.w;

    float diffIntensity = 0.0;
    float n_dot_l = dot(n,l);
    if (n_dot_l > 0.0) {
        vec3 lightPos = lightProjPos.xyz / lightProjPos.w;
        vec2 smUV = (lightPos.xy+1.0)*0.5;
        float depth = lightPos.z;
        float s = sampleShadowPCF(shadowMap, smUV, shadowMapSize, depth);
        // float bias = max(0.001 * (1.0 - dot(n, lightDir)), 0.0005);
        // s += bias;

        diffIntensity = max(n_dot_l * s, 0.0);
    }
    fragColor.xyz /= alpha;
    fragColor.xyz *= light_color * (ambientIntensity + diffIntensity);
    fragColor = gamma(fragColor);
    vec3 gray = vec3(dot(vec3(0.2126,0.7152,0.0722), fragColor.xyz));
    fragColor.xyz = vec3(mix(fragColor.xyz, gray, -0.225));
    fragColor.xyz *= alpha;
}
#pragma sokol @end

#pragma sokol @program color colorVS colorFS

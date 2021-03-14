#pragma warning(disable:4324)
#include "../build/client.glsl.h"
#include "../mui/microui.h"
#include "../mui/mui_renderer.c"

#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#if defined(__GNUC__) || defined(__clang__)
    #define _PRIVATE __attribute__((unused)) static
#else
    #define _PRIVATE static
#endif

typedef enum {
    Art_Wizard,
    Art_Bag,
    Art_Moon,
    Art_Fish,
    Art_Cloud,
    Art_WhiteCircle,
    Art_RainyCloud,
    Art_SunnyCloud,
    Art_ThunderCloud,
    Art_Tanabata,
    Art_Herb,
    Art_Cucumber,
    Art_Broccoli,
    Art_FourLeafClover,
    Art_Shamrock,
    Art_LeafyGreen,
    Art_Cactus,
    Art_Seedling,
    Art_Fog,
    Art_COUNT,
} Art;
const Art cloud_art[] = { Art_Cloud, Art_RainyCloud, Art_SunnyCloud, Art_ThunderCloud };
const char *art_path(Art art) {
    /* https://emojipedia.org is a great source for finding the unicode codepoints
       for various emojis. Make sure to lowercase the strings and remove all "U+".
       Keeping track of them as codepoints instead of renaming the .pngs may seem
       counterintuitive, but it should allow us later to easily pull fonts from
       the user's operating system instead of packaging the emoji images with the
       application, to cut down on size. */

    switch (art) {
    case (Art_Wizard):         return "img/1f9d9-200d-2642-fe0f.png";
    case (Art_Bag):            return "img/1f392.png";
    case (Art_Fish):           return "img/1f41f.png";
    case (Art_Moon):           return "img/1f311.png";
    case (Art_Cloud):          return "img/2601.png";
    case (Art_WhiteCircle):    return "img/26AA.png";
    case (Art_RainyCloud):     return "img/1f327.png";
    case (Art_SunnyCloud):     return "img/26C5.png";
    case (Art_ThunderCloud):   return "img/1f329.png";
    case (Art_Tanabata):       return "img/1f38b.png";
    case (Art_Herb):           return "img/1f33f.png";
    case (Art_Cucumber):       return "img/1f952.png";
    case (Art_Broccoli):       return "img/1f966.png";
    case (Art_FourLeafClover): return "img/1f340.png";
    case (Art_Shamrock):       return "img/2618.png";
    case (Art_LeafyGreen):     return "img/1f96C.png";
    case (Art_Cactus):         return "img/1f335.png";
    case (Art_Seedling):       return "img/1f331.png";
    case (Art_Fog):            return "img/1f32B.png";
    default:                   return NULL;
    }
}

#define OFFSCREEN_SAMPLE_COUNT 4
#define EMPTY_IMAGE rendr.empty_image
typedef enum { Shape_Plane, Shape_GroundPlane, Shape_Cube, Shape_Circle } Shape;
/* cached draw to be done at the end of a render */
typedef struct {
    Mat4 mat;
    Vec4 base_color, tint_color;
    Shape shape;
    sg_image img;
} Draw;

static struct {
    struct {
        sg_pass_action pass_action;
        sg_pass pass;
        sg_pipeline pip;
        sg_bindings bind;
    } shadow;
    struct {
        sg_pass_action pass_action;
        sg_pipeline pip;
        sg_bindings bind;
    } deflt;
    float ry;

    Mat4 view_proj;
    Vec3 eye;
    struct {
        Mat4 mvp;
        Vec3 dir;
    } light;

    struct {
        sg_pipeline pip;
        sg_image img;
        bool active;
    } offscreen;

    struct {
        sg_image img;
        bool loaded;
    } art[Art_COUNT];
    sg_image empty_image;

    struct {
        Draw cache[500];
        int count;
    } draw;

    bool loaded;
} rendr;

typedef struct {
    Vec3 pos;
    i16 u, v;
    Vec3 norm;
} Vertex;

_PRIVATE void _smooth_normals(u16 *indices, int index_count, Vertex *verts) {
    for (u16 v, i = 0; v = indices[i], i < index_count; i++)
        verts[v].norm = vec3f(0.0);

    for (int i = 0; i < index_count; i += 3) {
        Vertex *v0 = &verts[indices[i  ]],
               *v1 = &verts[indices[i+1]],
               *v2 = &verts[indices[i+2]];
        Vec3 edge0 = sub3(v0->pos, v1->pos),
             edge1 = sub3(v2->pos, v1->pos),
              norm = cross3(edge0, edge1);
        v0->norm = add3(v0->norm, norm);
        v1->norm = add3(v1->norm, norm);
        v2->norm = add3(v2->norm, norm);
    }

    for (u16 v, i = 0; v = indices[i], i < index_count; i++)
        verts[v].norm = norm3(verts[v].norm);
}

#define CIRCLE_RES 75
_PRIVATE void _rendr_init() {
    rendr.loaded = true;

    /* default pass action: clear to blue-ish */
    rendr.deflt.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = {0.2f, 0.6f, 0.8f, 1.0f} }
    };

    /* shadow pass action: clear to white */
    rendr.shadow.pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = {1.0f, 1.0f, 1.0f, 1.0f} }
    };

    /* a render pass with one color- and one depth-attachment image */
    sg_image_desc img_desc = {
        .render_target = true,
        .width = 2048,
        .height = 2048,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .sample_count = 1,
        .label = "shadow-map-color-image"
    };
    sg_image color_img = sg_make_image(&img_desc);
    img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    img_desc.label = "shadow-map-depth-image";
    sg_image depth_img = sg_make_image(&img_desc);
    rendr.shadow.pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = color_img,
        .depth_stencil_attachment.image = depth_img,
        .label = "shadow-map-pass"
    });

    /* cube vertex buffer with positions & normals */
    #define ALL_VERTS (4 * 6 + 4 * 2 + 4 + CIRCLE_RES)
    #define VERT_DATA_SIZE (ALL_VERTS * sizeof(Vertex))
    Vertex* vert_data = malloc(VERT_DATA_SIZE);
    Vertex *v = vert_data;
        /* pos                  normals             */
    *v++ = (Vertex){{-0.5f,0.0f,-0.5f},    0,32767,vec3f(0.0f)}; /*CUBE BACK FACE*/
    *v++ = (Vertex){{ 0.5f,0.0f,-0.5f},32767,32767,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,1.0f,-0.5f},32767,    0,vec3f(0.0f)};
    *v++ = (Vertex){{-0.5f,1.0f,-0.5f},    0,    0,vec3f(0.0f)};

    *v++ = (Vertex){{-0.5f,0.0f, 0.5f},32767,32767,vec3f(0.0f)}; /*CUBE FRONT FACE*/
    *v++ = (Vertex){{ 0.5f,0.0f, 0.5f},    0,32767,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,1.0f, 0.5f},    0,    0,vec3f(0.0f)};
    *v++ = (Vertex){{-0.5f,1.0f, 0.5f},32767,    0,vec3f(0.0f)};

    *v++ = (Vertex){{-0.5f,0.0f,-0.5f},    0,32767,vec3f(0.0f)}; /*CUBE LEFT FACE*/
    *v++ = (Vertex){{-0.5f,1.0f,-0.5f},    0,    0,vec3f(0.0f)};
    *v++ = (Vertex){{-0.5f,1.0f, 0.5f},32767,    0,vec3f(0.0f)};
    *v++ = (Vertex){{-0.5f,0.0f, 0.5f},32767,32767,vec3f(0.0f)};

    *v++ = (Vertex){{ 0.5f,0.0f,-0.5f},    0,32767,vec3f(0.0f)}; /*CUBE RIGHT FACE*/
    *v++ = (Vertex){{ 0.5f,1.0f,-0.5f},    0,    0,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,1.0f, 0.5f},32767,    0,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,0.0f, 0.5f},32767,32767,vec3f(0.0f)};

    *v++ = (Vertex){{-0.5f,0.0f,-0.5f},    0,32767,vec3f(0.0f)}; /*CUBE BOTTOM FACE*/
    *v++ = (Vertex){{-0.5f,0.0f, 0.5f},32767,32767,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,0.0f, 0.5f},32767,    0,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,0.0f,-0.5f},    0,    0,vec3f(0.0f)};

    *v++ = (Vertex){{-0.5f,1.0f,-0.5f},    0,32767,vec3f(0.0f)}; /*CUBE TOP FACE*/
    *v++ = (Vertex){{-0.5f,1.0f, 0.5f},32767,32767,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,1.0f, 0.5f},32767,    0,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,1.0f,-0.5f},    0,    0,vec3f(0.0f)};

    *v++ = (Vertex){{-0.5f,0.0f,-0.5f},32767,    0,vec3f(0.0f)}; /*GROUND PLANE GEOMETRY*/
    *v++ = (Vertex){{-0.5f,0.0f, 0.5f},32767,32767,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,0.0f, 0.5f},    0,32767,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,0.0f,-0.5f},    0,    0,vec3f(0.0f)};

    *v++ = (Vertex){{-0.5f,0.0f, 0.0f},    0,32767,vec3f(0.0f)};  /*PLANE BACK FACE */
    *v++ = (Vertex){{ 0.5f,0.0f, 0.0f},32767,32767,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,1.0f, 0.0f},32767,    0,vec3f(0.0f)};
    *v++ = (Vertex){{-0.5f,1.0f, 0.0f},    0,    0,vec3f(0.0f)};

    *v++ = (Vertex){{-0.5f,0.0f, 0.0f},    0,32767,vec3f(0.0f)};  /*PLANE FRONT FACE*/
    *v++ = (Vertex){{ 0.5f,0.0f, 0.0f},32767,32767,vec3f(0.0f)};
    *v++ = (Vertex){{ 0.5f,1.0f, 0.0f},32767,    0,vec3f(0.0f)};
    *v++ = (Vertex){{-0.5f,1.0f, 0.0f},    0,    0,vec3f(0.0f)};

    u16 pre_cir_verts = (u16) (v - vert_data);
    vert_data[pre_cir_verts] = (Vertex) { .u = 32767 / 2, .v = 32767 };
    for (int i = 1; i < CIRCLE_RES; i++) {
        Vec3 pos = vec3(cosf((f32) (i-1) / (f32) (CIRCLE_RES-1) * TAU32),
                        sinf((f32) (i-1) / (f32) (CIRCLE_RES-1) * TAU32),
                        0.0f);
        vert_data[pre_cir_verts + i] = (Vertex) {
            .pos = pos,
            .norm = vec3f(0.0f),
            .u = (i16) ((pos.x + 1.0f) / 2.0f * 32767.0f),
            .v = (i16) ((pos.y + 1.0f) / 2.0f * 32767.0f),
        };
    }

    #define STATIC_INDICES (6 + 6 * 6 + 6 * 2)
    #define CYLINDER_INDICES (3 * CIRCLE_RES)
    #define ALL_INDICES (STATIC_INDICES + CYLINDER_INDICES)
    uint16_t indices[ALL_INDICES] = {
        /* cube */
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20,
        /* plane */
        28, 29, 30,  28, 30, 31,
        34, 33, 32,  35, 34, 32,
        /* ground plane */
        26, 25, 24,  27, 26, 24,
    };
    int writer = STATIC_INDICES;
    for (u16 i = pre_cir_verts + 1; i < ALL_VERTS; i++) {
        u16 l = (i == (pre_cir_verts + 1)) ? (ALL_VERTS - 1) : (i - 1);
        u16 r = i;
        indices[writer++] = l;
        indices[writer++] = pre_cir_verts;
        indices[writer++] = r;
    }
    _smooth_normals(indices, ALL_INDICES, vert_data);

    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .data = { .ptr = vert_data, .size = VERT_DATA_SIZE },
        .label = "cube-vertices"
    });
    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .data = SG_RANGE(indices),
        .label = "cube-indices"
    });
    free(vert_data);

    /* pipeline-rendr-object for shadow-rendered cube, don't need texture coord here */
    rendr.shadow.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .layout = {
            /* need to provide stride, because the buffer's normal vector is skipped */
            .buffers[0].stride = sizeof(Vertex),
            /* but don't need to provide attr offsets, because pos and normal are continuous */
            .attrs = {
                [ATTR_shadowVS_position].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_shadowVS_uv].format = SG_VERTEXFORMAT_SHORT2N,
            }
        },
        .shader = sg_make_shader(shadow_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        /* Cull front faces in the shadow map pass */
        .cull_mode = SG_CULLMODE_FRONT,
        .sample_count = 1,
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .label = "shadow-map-pipeline"
    });

    /* and another pipeline-rendr-object for the default pass */
    sg_pipeline_desc deflt_pip_desc = {
        .layout = {
            /* don't need to provide buffer stride or attr offsets, no gaps here */
            .attrs = {
                [ATTR_colorVS_position].format = SG_VERTEXFORMAT_FLOAT3,
                [ATTR_colorVS_uv].format = SG_VERTEXFORMAT_SHORT2N,
                [ATTR_colorVS_normal].format = SG_VERTEXFORMAT_FLOAT3,
            }
        },
        .shader = sg_make_shader(color_shader_desc(sg_query_backend())),
        .index_type = SG_INDEXTYPE_UINT16,
        /* Cull back faces when rendering to the screen */
        .cull_mode = SG_CULLMODE_BACK,
        .depth = {
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true
        },
        .colors[0].blend = (sg_blend_state) {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_ONE, 
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, 
            .src_factor_alpha = SG_BLENDFACTOR_ONE, 
            .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        },
        .label = "default-pipeline"
    };
    rendr.deflt.pip = sg_make_pipeline(&deflt_pip_desc);

    sg_pipeline_desc offscreen_pip_desc = deflt_pip_desc;
    offscreen_pip_desc.label = "offscreen-pipeline";
    offscreen_pip_desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA8;
    offscreen_pip_desc.depth.write_enabled = false;
    offscreen_pip_desc.depth.pixel_format = SG_PIXELFORMAT_NONE;
    offscreen_pip_desc.sample_count = OFFSCREEN_SAMPLE_COUNT,
    rendr.offscreen.pip = sg_make_pipeline(&offscreen_pip_desc);

    /* the resource bindings for rendering the cube into the shadow map render target */
    rendr.shadow.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf
    };

    /* resource bindings to render the cube, using the shadow map render target as texture */
    rendr.deflt.bind = (sg_bindings){
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[SLOT_shadowMap] = color_img
    };

    u64 pixels[1] = { 0x00000000 };
    rendr.empty_image = sg_make_image(&(sg_image_desc) {
        .width = 1,
        .height = 1,
        .data.subimage[0][0] = SG_RANGE(pixels),
        .label = "empty texture"
    });
}

_PRIVATE void _load_image(const sfetch_response_t* res) {
    if (res->failed) {
        puts("resource loading failed");
        sapp_request_quit();
        return;
    }

    Art art = (Art) res->lane;

    int x = 72, y = 72, n = 4;
    u8 *img = stbi_load_from_memory(res->buffer_ptr, res->fetched_size, &x, &y, &n, 4);
    if (img == NULL) printf("image load failure :(");
    u32 img_size = x * y;

    /* premultiply the alpha */
    for (u32 i = 0; i < img_size * n; i += 4) {
        f32 a = (f32) img[i+3] / 255.0f;
        img[i+0] = (u8) ((f32) img[i+0] / 255.0f * a * 255.0f);
        img[i+1] = (u8) ((f32) img[i+1] / 255.0f * a * 255.0f);
        img[i+2] = (u8) ((f32) img[i+2] / 255.0f * a * 255.0f);
    }

    /* NOTE: https://github.com/floooh/sokol/issues/102 */
    rendr.art[art].img = sg_make_image_with_mipmaps(&(sg_image_desc) {
        .width = x,
        .height = y,
        .num_mipmaps = 1 + (int) floor(log2((f32) max(x, y))),
        .max_anisotropy = 4,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR_MIPMAP_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .data.subimage[0][0] = (sg_range) {
            .ptr = img,
            .size = img_size,
        },
        .label = res->path,
    });
    stbi_image_free(img);
    free(res->buffer_ptr);

    rendr.art[art].loaded = true;
    /* initializes the renderer if all the assets are loaded */
    for (Art i = (Art) 0; i < Art_COUNT; i++)
        if (!rendr.art[i].loaded) return;
    
    _rendr_init();
}

void rendr_load(void) {
    sg_setup(&(sg_desc){
        .context = sapp_sgcontext()
    });

    r_init();

    #define ASSET_BUF_SIZE 30000
    for (Art i = (Art) 0; i < Art_COUNT; i++) {
        const char *path = art_path(i);
        if (path) sfetch_send(&(sfetch_request_t) {
            .path = art_path(i),
            .callback = _load_image,
            .buffer_ptr = malloc(ASSET_BUF_SIZE),
            .buffer_size = ASSET_BUF_SIZE,
        });
        else puts("no known path for Art");
    }
}

sg_image get_art(Art art) {
    return rendr.art[art].img;
}

void start_offscreen(int width, int height, char *label) {
    assert(!rendr.offscreen.active);

    sg_image img = sg_make_image(&(sg_image_desc) {
        .render_target = true,
        .sample_count = OFFSCREEN_SAMPLE_COUNT,
        .width = width,
        .height = height,
        .max_anisotropy = 8,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR_MIPMAP_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
        .label = label,
    });

    sg_pass offscreen_pass = sg_make_pass(&(sg_pass_desc) {
        .color_attachments[0].image = img,
        .label = "offscreen-pass",
    });

    sg_pass_action offscreen_pass_action = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_LOAD }
    };

    sg_begin_pass(offscreen_pass, &offscreen_pass_action);
    sg_apply_pipeline(rendr.offscreen.pip);
    rendr.view_proj = mul4x4(
        ortho4x4(-0.5f, 0.5f, 0.5f, -0.5f, -1.0f, 1.0f),
        scale4x4(vec3(-1.0f, 1.0f, 1.0f))
    );

    rendr.offscreen.img = img;
    rendr.offscreen.active = true;
}

sg_image end_offscreen(void) {
    assert(rendr.offscreen.active);
    rendr.offscreen.active = false;
    sg_end_pass();
    return rendr.offscreen.img;
}

_PRIVATE void _draw_default(Draw *d) {
    rendr.deflt.bind.fs_images[SLOT_art] = d->img;
    sg_apply_bindings(&rendr.deflt.bind);

    vs_light_params_t vs_light_params = {
        .lightMVP = mul4x4(rendr.light.mvp, d->mat),
        .model = d->mat,
        .mvp = mul4x4(rendr.view_proj, d->mat),
        .tintColor = d->tint_color,
        .baseColor = d->base_color,
    };
    sg_apply_uniforms(SG_SHADERSTAGE_VS,
                      SLOT_vs_light_params,
                      &SG_RANGE(vs_light_params));
    switch (d->shape) {
    case Shape_Cube:;        sg_draw(    0,          6 * 6, 1); break;
    case Shape_Plane:;       sg_draw(6 * 6,          2 * 6, 1); break;
    case Shape_GroundPlane:; sg_draw(8 * 6,          1 * 6, 1); break;
    case Shape_Circle:;      sg_draw(9 * 6, CIRCLE_RES * 3, 1); break;
    }
}

void submit_draw(Draw d) {
    if (rendr.offscreen.active) _draw_default(&d);
    else rendr.draw.cache[rendr.draw.count++] = d;
}

void start_render(Vec3 cam) {
    rendr.draw.count = 0;

    /* matrices for shadow pass */
    rendr.light.dir = vec3(4.0f,24.0f,24.0f);
    Mat4 light_view = look_at4x4(rendr.light.dir, vec3f(0.0f), vec3_y());

    /* bias matrix for converting view-space coordinates into uv coordinates */
    Mat4 light_proj = { {
        { 0.5f, 0.0f, 0.0f, 0 },
        { 0.0f, 0.5f, 0.0f, 0 },
        { 0.0f, 0.0f, 0.5f, 0 },
        { 0.5f, 0.5f, 0.5f, 1 }
    } };
    light_proj = mul4x4(light_proj, ortho4x4(-0.0f, 7.0f, -0.0f, 7.0f, 0, 400.0f));
    rendr.light.mvp = mul4x4(light_proj, light_view);

    /* matrices for camera pass */
    static Vec3 eye_offset = { 0.4f, 6.8f, 8.4f };
    rendr.eye = add3(cam, eye_offset);
    Mat4 view = look_at4x4(rendr.eye, cam, vec3_y());
    Mat4 proj = perspective4x4(45.0f, sapp_widthf()/sapp_heightf(), 0.01f, 100.0f);
    rendr.view_proj = mul4x4(proj, view);
}

/* this is a massive hack, but it works */
_PRIVATE int _camera_close_cmp(const void *av, const void *bv) {
    f32 a, b;

    Draw *ad = (Draw *)av;
    Draw *bd = (Draw *)bv;
    f32 ay = (ad->shape == Shape_GroundPlane) ? -0.5f : 0.0f;
    f32 by = (ad->shape == Shape_GroundPlane) ? -0.5f : 0.0f;
    Vec4 ap = mul4x44(ad->mat, vec4(0.0f, 0.0f, ay, 1.0f)),
         bp = mul4x44(bd->mat, vec4(0.0f, 0.0f, by, 1.0f));
    a = magmag3(sub3(rendr.eye, div3f(ap.xyz, ap.w)));
    b = magmag3(sub3(rendr.eye, div3f(bp.xyz, bp.w)));

    return (a < b) - (a > b);
}

void end_render(void) {
    qsort(rendr.draw.cache,
          rendr.draw.count,
          sizeof(Draw),
          _camera_close_cmp);
    /* fragment uniforms for light shader */
    fs_light_params_t fs_light_params = {
        .lightDir = norm3(rendr.light.dir),
        .shadowMapSize = vec2f(2048),
        .eyePos = vec3f(5.0f)
    };

    /* the shadow map pass, render the vertices into the depth image */
    sg_begin_pass(rendr.shadow.pass, &rendr.shadow.pass_action);
    sg_apply_pipeline(rendr.shadow.pip);
    /* Render into the shadow map */
    for (int i = 0; i < rendr.draw.count; i++) {
        Draw *d = &rendr.draw.cache[i];

        rendr.shadow.bind.fs_images[SLOT_tex] = d->img;
        sg_apply_bindings(&rendr.shadow.bind);

        vs_shadow_params_t vs_shadow_params = {
            .mvp = mul4x4(rendr.light.mvp, d->mat)
        };
        sg_apply_uniforms(SG_SHADERSTAGE_VS,
                          SLOT_vs_shadow_params,
                          &SG_RANGE(vs_shadow_params));
        switch (d->shape) {
        case Shape_Cube:;  sg_draw( 0, 36, 1); break;
        case Shape_Plane:; sg_draw(36, 12, 1); break;
        case Shape_GroundPlane:;               break;
        }
    }
    sg_end_pass();

    /* render scene using the previously rendered shadow map as a texture */
    sg_begin_default_pass(&rendr.deflt.pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(rendr.deflt.pip);
    sg_apply_uniforms(SG_SHADERSTAGE_FS,
                      SLOT_fs_light_params,
                      &SG_RANGE(fs_light_params));
    for (int i = 0; i < rendr.draw.count; i++)
        _draw_default(&rendr.draw.cache[i]);

    sg_end_pass();
}

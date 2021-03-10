#include "atlas.inl"

#define BUFFER_SIZE 16384

static f32 tex_buf[BUFFER_SIZE *  8];
static f32 vert_buf[BUFFER_SIZE *  8];
static u8  color_buf[BUFFER_SIZE * 16];
static u32 index_buf[BUFFER_SIZE *  6];

static int buf_idx;

static sg_pipeline s_pip;
static sg_bindings s_bind;

static sg_buffer s_vbuf;
static sg_buffer s_vcol;
static sg_buffer s_vtex;
static sg_buffer s_ibuf;

/* a uniform block with a model-view-projection matrix */
typedef struct {
    Mat4 mvp;
} mui_params;

void r_init(void) {
    sg_buffer_desc vbuf_desc = { .size = sizeof(vert_buf), .usage = SG_USAGE_STREAM };
    sg_buffer_desc vcol_desc = { .size = sizeof(color_buf), .usage = SG_USAGE_STREAM };
    sg_buffer_desc vtex_desc = { .size = sizeof(tex_buf), .usage = SG_USAGE_STREAM };

    sg_buffer_desc ibuf_desc = {
        .size = sizeof(index_buf),
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .usage = SG_USAGE_STREAM
    };

    s_vbuf = sg_make_buffer(&vbuf_desc);
    s_vcol = sg_make_buffer(&vcol_desc);
    s_vtex = sg_make_buffer(&vtex_desc);
    s_ibuf = sg_make_buffer(&ibuf_desc);

    sg_image img = sg_make_image(&(sg_image_desc){
        .width = ATLAS_WIDTH,
        .height = ATLAS_HEIGHT,
        .pixel_format = SG_PIXELFORMAT_R8,// RGBA8,
        .min_filter = SG_FILTER_NEAREST,
        .mag_filter = SG_FILTER_NEAREST,
        .data.subimage[0][0] = SG_RANGE(atlas_texture)
    });

    /* define the resource bindings */
    s_bind = (sg_bindings){
        .vertex_buffers[0] = s_vbuf,
        .vertex_buffers[1] = s_vtex,
        .vertex_buffers[2] = s_vcol,
        .fs_images[0] = img,
        .index_buffer = s_ibuf
    };

    /* create a shader (use vertex attribute locations) */
    sg_shader shd = sg_make_shader(mui_shader_desc(sg_query_backend()));

    /* create a pipeline object (default render state is fine) */
    s_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT32,
        .layout = {
            // number of vertex buffers doesn't match number of pipeline vertex layouts
            .attrs = {
                [ATTR_muiVS_position] = {
                    .format = SG_VERTEXFORMAT_FLOAT2,
                    .buffer_index = 0,
                },
                [ATTR_muiVS_tex0] = {
                    .format = SG_VERTEXFORMAT_FLOAT2,
                    .buffer_index = 1,
                },
                [ATTR_muiVS_color0] = {
                    .format = SG_VERTEXFORMAT_UBYTE4N,
                    .buffer_index = 2,
                }
            }
        },
        .colors[0].blend = {
            .enabled = true,
            .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA,
            .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        },
        .label = "mui-pipeline"
    });
}


static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
    assert(buf_idx < BUFFER_SIZE);

    int texvert_idx = buf_idx *  8;
    int   color_idx = buf_idx * 16;
    int element_idx = buf_idx *  4;
    int   index_idx = buf_idx *  6;
    buf_idx++;

    /* update texture buffer */
    f32 x = src.x / (f32) ATLAS_WIDTH;
    f32 y = src.y / (f32) ATLAS_HEIGHT;
    f32 w = src.w / (f32) ATLAS_WIDTH;
    f32 h = src.h / (f32) ATLAS_HEIGHT;
    tex_buf[texvert_idx + 0] = x;
    tex_buf[texvert_idx + 1] = y;
    tex_buf[texvert_idx + 2] = x + w;
    tex_buf[texvert_idx + 3] = y;
    tex_buf[texvert_idx + 4] = x;
    tex_buf[texvert_idx + 5] = y + h;
    tex_buf[texvert_idx + 6] = x + w;
    tex_buf[texvert_idx + 7] = y + h;

    /* update vertex buffer */
    vert_buf[texvert_idx + 0] = (f32) dst.x;
    vert_buf[texvert_idx + 1] = (f32) dst.y;
    vert_buf[texvert_idx + 2] = (f32) dst.x + dst.w;
    vert_buf[texvert_idx + 3] = (f32) dst.y;
    vert_buf[texvert_idx + 4] = (f32) dst.x;
    vert_buf[texvert_idx + 5] = (f32) dst.y + dst.h;
    vert_buf[texvert_idx + 6] = (f32) dst.x + dst.w;
    vert_buf[texvert_idx + 7] = (f32) dst.y + dst.h;

    /* update color buffer */
    memcpy(color_buf + color_idx +  0, &color, 4);
    memcpy(color_buf + color_idx +  4, &color, 4);
    memcpy(color_buf + color_idx +  8, &color, 4);
    memcpy(color_buf + color_idx + 12, &color, 4);

    /* update index buffer */
    index_buf[index_idx + 0] = element_idx + 0;
    index_buf[index_idx + 1] = element_idx + 1;
    index_buf[index_idx + 2] = element_idx + 2;
    index_buf[index_idx + 3] = element_idx + 2;
    index_buf[index_idx + 4] = element_idx + 3;
    index_buf[index_idx + 5] = element_idx + 1;
}


void r_draw_rect(mu_Rect rect, mu_Color color) {
    push_quad(rect, atlas[ATLAS_WHITE], color);
}


void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    for (const char *p = text; *p; p++) {
        if ((*p & 0xc0) == 0x80) { continue; }
        int chr = mu_min((unsigned char) *p, 127);
        mu_Rect src = atlas[ATLAS_FONT + chr];
        dst.w = src.w;
        dst.h = src.h;
        push_quad(dst, src, color);
        dst.x += dst.w;
    }
}


void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    mu_Rect src = atlas[id];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    push_quad(mu_rect(x, y, src.w, src.h), src, color);
}


int r_get_text_width(const char *text, int len) {
    int res = 0;
    for (const char *p = text; *p && len--; p++) {
        if ((*p & 0xc0) == 0x80) { continue; }
        int chr = mu_min((unsigned char) *p, 127);
        res += atlas[ATLAS_FONT + chr].w;
    }
    return res;
}


int r_get_text_height(void) {
    return 18;
}

typedef enum {
    CMD_DRAW,
    CMD_CLIP
} cmd_type;

#define MAX_CMDS 1024

typedef struct {
    cmd_type type;
    union {
        mu_Rect clip;
        struct {
            int start_buf_idx;
            int length;
        } draw;
    };
} draw_cmd;

typedef struct {
    int cmd_idx;
    int start_buf_idx;
    draw_cmd cmds[MAX_CMDS];
} draw_fifo;

void draw_fifo_start(draw_fifo* self) {
    self->cmd_idx = 0;
    self->start_buf_idx = 0;
}

void draw_fifo_queue_draw(draw_fifo* self, int cur_buf_idx) {
    assert(self->cmd_idx < MAX_CMDS);
    self->cmds[self->cmd_idx++] = (draw_cmd){
        .type = CMD_DRAW,
        .draw = {
            .start_buf_idx = self->start_buf_idx,
            .length = cur_buf_idx - self->start_buf_idx
        }
    };
    self->start_buf_idx = cur_buf_idx;
}

void draw_fifo_queue_clip(draw_fifo* self, mu_Rect r) {
    assert(self->cmd_idx < MAX_CMDS);
    self->cmds[self->cmd_idx++] = (draw_cmd){
        .type = CMD_CLIP,
        .clip = r
    };
}

void r_draw_commands(mu_Context* ctx, int width, int height) {
    mui_params vs_params;

    vs_params.mvp = ortho4x4(0.f, (f32) width, (f32) height, 0.f, -1.f, 1.f);

    /* default pass action */
    sg_pass_action pass_action = {
        .colors[0] = { .action = SG_ACTION_LOAD }
    };

    sg_begin_default_pass(&pass_action, width, height);
    sg_apply_pipeline(s_pip);
    sg_apply_bindings(&s_bind);
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &SG_RANGE(vs_params));

    /* render */
    draw_fifo cmd_fifo;
    draw_fifo_start(&cmd_fifo);

    mu_Command *cmd = NULL;
    while (mu_next_command(ctx, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT:
                r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
            case MU_COMMAND_RECT:
                r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
            case MU_COMMAND_ICON:
                r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
            case MU_COMMAND_CLIP: {
                draw_fifo_queue_draw(&cmd_fifo, buf_idx);
                draw_fifo_queue_clip(&cmd_fifo, cmd->clip.rect);
                break;
            }
        }
    }
    draw_fifo_queue_draw(&cmd_fifo, buf_idx);

    sg_update_buffer(s_vbuf, &(sg_range){.ptr= vert_buf,.size=buf_idx*8*sizeof(f32)});
    sg_update_buffer(s_vtex, &(sg_range){.ptr=  tex_buf,.size=buf_idx*8*sizeof(f32)});
    sg_update_buffer(s_vcol, &(sg_range){.ptr=color_buf,.size=           buf_idx*16});
    sg_update_buffer(s_ibuf, &(sg_range){.ptr=index_buf,.size=buf_idx*6*sizeof(int)});

    for (int i = 0; i < cmd_fifo.cmd_idx; i++) {
        const draw_cmd* c = &cmd_fifo.cmds[i];
        switch(c->type) {
            case CMD_DRAW: {
                if (c->draw.length != 0) {
                    sg_draw(c->draw.start_buf_idx*6, c->draw.length*6, 1);
                }
                break;
            }
            case CMD_CLIP: {
                mu_Rect r = c->clip;
                sg_apply_scissor_rect(r.x, r.y, r.w, r.h, true);
                break;
            }
        }
    }
    buf_idx = 0;

    sg_end_pass();
    sg_commit();
}

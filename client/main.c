#define LEN(arr) ((int) (sizeof arr / sizeof arr[0]))

#define _CRT_SECURE_NO_WARNINGS
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <assert.h>

#include "../common/common.h"
#include "../bq_websocket/bq_websocket.h"
#include "../bq_websocket/bq_websocket_platform.h"

#define TEST_SERV

#ifdef TEST_SERV
#define EMBED_SERV
#include <process.h>
#include "../server/main.c"
#endif

#include "../sokol/sokol_time.h"
#include "../sokol/sokol_app.h"
#include "../sokol/sokol_gfx.h"
#include "../sokol/sokol_glue.h"
#include "../sokol/sokol_fetch.h"

#include "renderer.h"

void panic(char *msg) {
    puts(msg);
    sapp_request_quit();
}

static struct {
    bool keys_down[350];
    bool keys_pressed[350];

    /* left mouse button down? */
    bool left_mb_down;
    Vec2 mouse_pos;
} input;

typedef enum {
    EntProp_Active,
    EntProp_Phys,
    EntProp_Wall,
    EntProp_Hidden,
    EntProp_COUNT,
} EntProp;

typedef struct Ent Ent;
struct Ent {
    /* specifies which of the following field groups are valid */
    u64 props[(EntProp_COUNT + 63)/64];

    /* appearance */
    Shape shape;
    Vec4 base_color, tint_color;
    sg_image img;

    /* bob animation */
    f32 bob_scale, bob_freq;

    /* EntProp_Wall */
    Vec3 wall_l, wall_r;

    /* positioning */
    Ent *parent, *first_child, *last_child, *next, *prev;
    Vec3 pos, rot, scale;
    /* If true, children of this ent are not positioned relative to it */
    bool independent_children;

    /* physics & gravity: EntProp_Phys */
    Vec3 vel, acc;
    f32 mass;
};

INLINE bool has_ent_prop(Ent *ent, EntProp prop) {
    return !!(ent->props[prop/64] & ((u64)1 << (prop%64)));
}

INLINE bool toggle_ent_prop(Ent *ent, EntProp prop) {
    bool before = has_ent_prop(ent, prop);
    ent->props[prop/64] ^= (u64)1 << (prop%64);
    return before;
}

INLINE bool take_ent_prop(Ent *ent, EntProp prop) {
    bool before = has_ent_prop(ent, prop);
    ent->props[prop/64] &= ~((u64)1 << (prop%64));
    return before;
}

INLINE bool give_ent_prop(Ent *ent, EntProp prop) {
    bool before = has_ent_prop(ent, prop);
    ent->props[prop/64] |= (u64)1 << (prop%64);
    return before;
}

INLINE void add_ent_child(Ent *parent, Ent *child) {
    child->parent = parent;
    child->prev = parent->last_child;
    child->next = NULL;
    if (parent->last_child) parent->last_child->next = child;
    else parent->first_child = child;

    parent->last_child = child;
}

/* Similar to ent_tree_iter, but not recursive. */
INLINE Ent *ent_child_iter(Ent *node) {
    if (node == NULL) return NULL;
    return node->next;
}

/* Used to iterate through an ent's children, recursively */
INLINE Ent *ent_tree_iter(Ent *node) {
    if (node == NULL) return NULL;
    if (node->first_child) return node->first_child;

    while (node && node->next == NULL)
        node = node->parent;
    
    if (node) return node->next;
    return NULL;
}

/* NOTE: positioning assumes child is actually a child of parent */
/*
INLINE void detach_child(Ent *child) {
    for (Ent *c; c = child->first_child;)
        detach_child(c);

    child->mat = get_ent_unscaled_mat(child);
    child->pick_up_start_pos = child->mat.w.xyz;
    Ent *parent = child->parent;
    if (parent) {
        if (parent->last_child == child) parent->last_child = child->prev;
        if (parent->first_child == child) parent->first_child = child->next;
    }

    if (child->prev) child->prev->next = child->next;
    if (child->next) child->next->prev = child->prev;
    child->next = NULL;
    child->prev = NULL;
    child->parent = NULL;

    if (child->mass > 0.0)
        give_ent_prop(child, EntProp_Phys);
}*/




/* --------- WORLD */

#define MAX_ENTS 2000
#define NETBUF_SIZE 100
static struct {
    bool generated;
    u64 start_time, last_render;
    f64 dt, elapsed;

    Ent ents[MAX_ENTS];
    Ent *cam;

    mu_Context mu_ctx;

    bqws_pt_noblock_connector *connector;
    bqws_socket *ws;
    struct { bqws_msg *msg; NetToClient ntc; } netbuf[NETBUF_SIZE];
} game;

/* Use this function to iterate over all of the Ents in the game.
   ex:
        for (Ent *e = 0; e = ent_game_iter(e); )
            draw_ent(e);
*/
INLINE Ent *ent_game_iter(Ent *ent) {
    if (ent == NULL) ent = game.ents - 1;

    while (!has_ent_prop((++ent), EntProp_Active))
        if ((ent - game.ents) == MAX_ENTS)
            return NULL;

    if (has_ent_prop(ent, EntProp_Active)) return ent;
    return NULL;
}

/* NOTE: in most cases you want to be calling add_ent instead */
INLINE Ent default_ent() {
    return (Ent) {
        .base_color = vec4f(0.0f),
        .tint_color = vec4f(1.0f),
        .scale = vec3f(1.0f),
    };
}

/* Use this function to spawn new entities. */
INLINE Ent *add_ent(void) {
    for (int i = 0; i < MAX_ENTS; i++)
        if (!has_ent_prop(&game.ents[i], EntProp_Active)) {
            Ent *slot = &game.ents[i];
            *slot = default_ent();
            give_ent_prop(slot, EntProp_Active);
            return slot;
        }

    return NULL;
}

Mat4 unscaled_ent_mat(Ent *e) {
    Vec3 rot = e->rot;
    Vec3 pos = e->pos;

    if (e->bob_freq != 0.0f)
        pos.y += (f32) (sin(game.elapsed / e->bob_freq) * e->bob_scale);

    Quat rot_q = mulQ(mulQ(axis_angleQ(vec3_x(), rot.x),
                           axis_angleQ(vec3_y(), rot.y)),
                           axis_angleQ(vec3_z(), rot.z));
    Mat4 mat = mul4x4(translate4x4(pos), quat4x4(rot_q));

    if (e->parent && !e->parent->independent_children)
        mat = mul4x4(unscaled_ent_mat(e->parent), mat);
    
    return mat;
}

void draw_ent(Ent *e) {
    Mat4 mat = unscaled_ent_mat(e);
    Vec3 scale = e->scale;

    Draw draw = (Draw) {
        .mat        = mul4x4(mat, scale4x4(scale)),
        .base_color = e->base_color,
        .tint_color = e->tint_color,
        .img        = e->img,
        .shape      = e->shape,
    };
    submit_draw(draw);
}


Ent *wall_at(Vec3 p, f32 height) {
    Ent *w = add_ent();
    w->img = get_art(Art_Fog);
    w->shape = Shape_Cube;
    Vec4 color = vec4(0.58f, 0.51f, 0.85f, 1.0f);
    w->base_color = mul4f(color, 0.3f);
    w->base_color.w = 1.0f;
    w->tint_color = mul4f(color, 0.5f);
    w->tint_color.w = 1.0f;
    w->pos = p;
    w->scale.y = height;
    return w;
}

static int text_width(mu_Font font, const char *text, int len) {
    font;
    if (len == -1) { len = (int) strlen(text); }
    return r_get_text_width(text, len);
}

static int text_height(mu_Font font) {
    font;
    return r_get_text_height();
}

/* initializes the game */
void init(void) {
    srandf(9, 12, 32, 10);

    game.cam = add_ent();
    give_ent_prop(game.cam, EntProp_Hidden);

    sfetch_setup(&(sfetch_desc_t){ .num_lanes = Art_COUNT });
    rendr_load();
    stm_setup();

    mu_init(&game.mu_ctx);
    game.mu_ctx.text_width = text_width;
    game.mu_ctx.text_height = text_height;

    bqws_pt_init(NULL);
    puts("connecting");
    game.connector = bqws_pt_connect_noblock("ws://localhost:6666", NULL, NULL, NULL);
    puts("connect done");

    game.start_time = stm_now();
    game.dt = 0.0;
    game.last_render = stm_now();
}

Ent *wall_to(Vec3 from, Vec3 to) {
    Ent *w = wall_at(div3f(add3(from, to), 2.0f), 1.0f);
    w->scale.z = mag3(sub3(from, to));
    w->rot.y = atan2f(to.x - from.x, to.z - from.z);
    give_ent_prop(w, EntProp_Wall);
    w->wall_l = from, w->wall_r = to;
    return w;
}

void gen_castle(Vec3 origin, f32 radius) {
    for (int i = 1; i <= 10; i++) {
        /* wall */
        f32 n0 = ((f32) (i+0) / 10.0f) * TAU32,
            n1 = ((f32) (i+1) / 10.0f) * TAU32;
        Vec3 p0 = add3(origin, vec3(cosf(n0) * radius, 0.0f, sinf(n0) * radius)),
             p1 = add3(origin, vec3(cosf(n1) * radius, 0.0f, sinf(n1) * radius));
        Ent *wall = wall_to(p0, p1);
        wall->scale.x *= 0.7f;

        /* post */
        f32 rot = atan2f(p0.x - origin.x, p0.z - origin.z);
        Vec3 in = vec3(sinf(rot), 0.0f, cosf(rot));
        Ent *post = wall_at(sub3(p0, mul3f(in, 0.185f)), 1.4f);
        post->rot.y = rot;
        post->scale.x = post->scale.z = 1.1f;
    }
}

Ent *wall_test(Vec3 p) {
    for (Ent *e = 0; e = ent_game_iter(e); )
        if (has_ent_prop(e, EntProp_Wall))
            if (line_dist3(e->wall_l, e->wall_r, p) < e->scale.x)
                return e;
    return NULL;
}

void gen_castle_ground(Vec3 origin) {
    Ent *e = add_ent();
    // e->pos.y = 0.01f;
    e->shape = Shape_GroundPlane;
    e->pos = origin;
    e->scale = vec3f(15.0f);

    const Art flat_plants[] = { Art_FourLeafClover, Art_Shamrock, Art_Seedling };
    
    start_offscreen(2048, 2048, "ground circle tex");
    {
        Ent c = default_ent();
        c.shape = Shape_Circle;
        c.scale = mul3f(c.scale, 0.5f);
        c.base_color = vec4(0.125f, 0.3f, 0.195f, 1.0f);
        c.img = EMPTY_IMAGE;
        draw_ent(&c);
    }

    {
        Ent l = default_ent();
        l.tint_color = vec4(0.08f, 0.13f, 0.17f, 0.65f);
        l.scale = mul3f(l.scale, (1.0f / 15.0f) * 0.3f);

        const int max = 70;
        for (int x = 0; x < max; x++) {
        for (int y = 0; y < max; y++) {
            l.rot.z = PI32 * randf() * 0.25f + (PI32 * (3.0f / 8.0f));
            l.img = get_art(flat_plants[rand_u32() % LEN(flat_plants)]);

            const f32 w = sqrtf(3.0f), h = 2.0f;
            l.pos.x = ((f32) (x * 2 + (y & 1)) / 2.0f * w) / 20.0f - 0.5f;
            l.pos.y = ((3.0f / 4.0f) * (f32)y * h) / (f32)max - 0.5f;

            Vec3 center = add3(l.pos, mul3f(vec3_x(), l.scale.x / 2.0f));
            l.pos = add3(l.pos, mul3f(rand3(), 0.01f));
            l.pos.z = 0.0f;
            if (mag3(center) < 0.4f) draw_ent(&l);
        }
        }

        const int circle = 45;
        Vec3 outer = mul3f(l.scale, 1.3f);
        Vec3 inner = mul3f(l.scale, 0.9f);
        for (int i = 0; i < circle; i++) {
            l.img = get_art(flat_plants[rand_u32() % LEN(flat_plants)]);

            f32 r = ((f32) i / (f32) circle) * TAU32;
            l.rot.z = r - PI32/2.0f;
            l.scale = outer;
            l.pos.x = cosf(r) * (0.5f - l.scale.z * 1.2f);
            l.pos.y = sinf(r) * (0.5f - l.scale.z * 1.2f);
            draw_ent(&l);

            for (int q = 0; q < 3; q++) {
                r += 0.06f;
                l.rot.z = r - PI32/2.0f;
                l.scale = inner;
                l.pos.x = cosf(r) * (0.5f - outer.z * 2.35f - 0.031f * (f32) q);
                l.pos.y = sinf(r) * (0.5f - outer.z * 2.35f - 0.031f * (f32) q);
                draw_ent(&l);
            }
        }
    }

    {
        const int max = 10;
        const f32 grid_scale = 32.0f;
        for (int x = -max; x <= max; x++) {
        for (int y = -max; y <= max; y++) {
            Ent m = default_ent();
            m.img = get_art(Art_Moon);
            m.scale = mul3f(m.scale, (1.0f / 15.0f) * 0.85f);
            m.pos = vec3f(0.0f);
            Vec4 color = vec4(0.225f, 0.035f, 0.05f, 1.0f);

            const f32 w = sqrtf(3.0f), h = 2.0f;
            m.pos.x += ((f32) (x * 2 + (y & 1)) / 2.0f * w) / grid_scale;
            m.pos.y += ((3.0f / 4.0f) * (f32)y * h)         / grid_scale;

            /* find roughly where this tile is in game-space */
            Vec3 game_pos; {
                Vec3 pos = m.pos;
                pos = mul3f(m.pos, 15.0f);
                pos.z = pos.y + 0.5f;
                pos.x = pos.x - 0.36f;
                pos.y = 0.0f;
                game_pos = add3(pos, origin);
            }

            Vec3    patch = div3f(vec3(-4.3f, 3.0f, 0.0f), grid_scale),
                 subpatch = add3(patch, vec3(-0.15f, -0.05f, 0.0f)),
                 road_end = div3f(vec3(4.0f, -1.0f, 0.0f), grid_scale);
            f32 road_dist = line_dist2(patch.xy, road_end.xy, m.pos.xy);
            if (mag3(m.pos) > 0.42f) continue;
            else if (mag3(sub3(m.pos, patch)) < 0.18f);
            else if (mag3(sub3(m.pos, subpatch)) < 0.085f);
            else if (m.pos.x > -0.1f && road_dist < 0.063f) {
                /* remove walls that intersect roads */
                Ent *wall = wall_test(game_pos);
                if (wall) take_ent_prop(wall, EntProp_Active);
            } else {
                Vec2 pt = vec2(game_pos.x, game_pos.z);
                f32 simp = simplex2(div2f(pt, 24.0f)) * 0.2f +
                           simplex2(div2f(pt,  2.0f)) * 0.5f +
                           simplex2(div2f(pt,  0.3f)) * 0.5f ;
                if (simp < 0.35f)
                    continue;
                if (wall_test(game_pos))
                    continue;

                simp += 0.1f;
                Ent *p = add_ent();
                p->scale = vec3f(simp);
                p->pos = game_pos;
                p->img = get_art(simp > 0.775f ? Art_Tanabata : Art_Herb);
                continue;
            };

            // m.tint_color.x += 0.05f * randf();

            m.tint_color = color;
            draw_ent(&m);

            f32 spray_alpha = 0.2f;
            f32 spray = 0.1f;
            m.tint_color = vec4(spray, spray, spray, spray_alpha);
            draw_ent(&m);

            m.pos.y -= (m.scale.x * 1.35f - m.scale.x) / 2.0f;
            m.scale = mul3f(m.scale, 1.35f);

            m.tint_color = mul4f(color, 0.8f);
            draw_ent(&m);

            m.tint_color = vec4(spray, spray, spray, spray_alpha * 0.8f);
            draw_ent(&m);
        }
        }
    }
    e->img = end_offscreen();
}

void generate_game() {
    /* cloud */
    for (int i = 0; i < 30; i++) {
        Ent *e = add_ent();
        e->img = get_art(cloud_art[rand_u32() % LEN(cloud_art)]);
        e->bob_scale = 0.2f + randf() * 0.3f;
        e->bob_freq = 2.0f + randf() * 1.0f;
        e->scale = mul3f(e->scale, 2.0f);

        f32 q = randf() * TAU32;
        e->pos.z = cosf(q) * randf() * 50.0f - 30.0f;
        e->pos.x = sinf(q) * randf() * 50.0f;
        e->pos.y = -10.0f - 10.0f * powf(randf() + 0.5f, 5);

        add_ent_child(game.cam, e);
    }

    Vec3 origin = vec3(0.0f, 0.0f, 0.0f);
    game.cam->pos = origin;
    gen_castle(origin, 6.0f);
    gen_castle_ground(origin);
    
    {
        Ent *e = add_ent();
        e->img = get_art(Art_Wizard);
        e->pos = add3(origin, vec3(1.0f, 0.0f, 1.0f));
    }

    {
        Ent *e = add_ent();
        e->img = get_art(Art_Fish);
        e->pos = add3(origin, vec3(-2.0f, 0.0f, 0.0f));
    }

    for (Ent *e = 0; e = ent_game_iter(e); )
        if (e->shape == Shape_Plane)
            e->tint_color = vec4(1.3f, 1.0f, 1.5f, 1.0f);
}


void event(const sapp_event *ev) {
    static const char key_map[512] = {
        [SAPP_KEYCODE_LEFT_SHIFT]       = MU_KEY_SHIFT,
        [SAPP_KEYCODE_RIGHT_SHIFT]      = MU_KEY_SHIFT,
        [SAPP_KEYCODE_LEFT_CONTROL]     = MU_KEY_CTRL,
        [SAPP_KEYCODE_RIGHT_CONTROL]    = MU_KEY_CTRL,
        [SAPP_KEYCODE_LEFT_ALT]         = MU_KEY_ALT,
        [SAPP_KEYCODE_RIGHT_ALT]        = MU_KEY_ALT,
        [SAPP_KEYCODE_ENTER]            = MU_KEY_RETURN,
        [SAPP_KEYCODE_BACKSPACE]        = MU_KEY_BACKSPACE,
    };

    // mui first
    switch (ev->type) {
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        mu_input_mousedown(&game.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        mu_input_mouseup(&game.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        mu_input_mousemove(&game.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y);
        break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        mu_input_scroll(&game.mu_ctx, 0, (int)ev->scroll_y);
        break;
    case SAPP_EVENTTYPE_KEY_DOWN:
        mu_input_keydown(&game.mu_ctx, key_map[ev->key_code & 511]);
        break;
    case SAPP_EVENTTYPE_KEY_UP:
        mu_input_keyup(&game.mu_ctx, key_map[ev->key_code & 511]);
        break;
    case SAPP_EVENTTYPE_CHAR:
        {
            char txt[2] = { (char)(ev->char_code & 255), 0 };
            mu_input_text(&game.mu_ctx, txt);
        }
        break;
    default:
        break;
    }

    switch (ev->type) {
        case SAPP_EVENTTYPE_MOUSE_MOVE:;
            input.mouse_pos = vec2(ev->mouse_x, ev->mouse_y);
            break;
        case SAPP_EVENTTYPE_MOUSE_DOWN:;
            input.mouse_pos = vec2(ev->mouse_x, ev->mouse_y);
            if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT)
                input.left_mb_down = true;
            break;
        case SAPP_EVENTTYPE_MOUSE_UP:;
            input.mouse_pos = vec2(ev->mouse_x, ev->mouse_y);
            if (ev->mouse_button == SAPP_MOUSEBUTTON_LEFT)
                input.left_mb_down = false;
            break;
        case SAPP_EVENTTYPE_KEY_DOWN:;
            #ifndef NDEBUG
                if (ev->key_code == SAPP_KEYCODE_ESCAPE)
                    sapp_request_quit();
            #endif
            if (!input.keys_down[(int) ev->key_code])
                input.keys_pressed[(int) ev->key_code] = true;
            input.keys_down[(int) ev->key_code] = true;
            break;
        case SAPP_EVENTTYPE_KEY_UP:;
            input.keys_down[(int) ev->key_code] = false;
            break;
        case SAPP_EVENTTYPE_QUIT_REQUESTED:;
            puts("application closing!");
            break;
        default:;
            break;
    }
}

typedef enum {
    Login_User, Login_ServerError,
    Login_Exists, Login_Trial, Login_Register,
    Login_Available, Login_Pass,
    Login_Done, 
} Login;
static bool focus_next;
int linked_textbox(mu_Context *ctx, char *str, int size) {
    int res = mu_textbox(ctx, str, size);
    if (focus_next) {
        focus_next = false;
        mu_set_focus(ctx, ctx->last_id);
    } else if (res & MU_RES_SUBMIT) {
        focus_next = true;
    }
    return res;
}
typedef struct {
    char pass_str[128], bad_pass_reason[256], confirm_pass_str[256],
         user_str[128], bad_user_reason[256], server_error_str[256];
    int skip_tutorial, show_pass; /* booleans typed as ints for mu compat */
    Login login;
} LoginWindow;
static LoginWindow login;
/* if confirm is enabled, returns true if "confirm password" input matches original */
bool password_input(mu_Context *ctx, bool confirm) {
    bool pass_match = false;

    if (confirm) mu_push_id(ctx, "confirm", sizeof("confirm"));
    char hidden_str[128],
         *pass_str = confirm ? login.confirm_pass_str : login.pass_str;
    size_t pass_len = strlen(pass_str);
    if (login.show_pass) strcpy(hidden_str, pass_str);
    else {
        for (int i = 0; i < pass_len; i++) hidden_str[i] = '*';
        if (pass_len > 0) hidden_str[pass_len-1] = pass_str[pass_len-1];
        hidden_str[pass_len-0] = '\0';
    }

    int res = linked_textbox(ctx, hidden_str, sizeof(hidden_str));
    if (res & MU_RES_CHANGE) {
        mu_set_focus(ctx, ctx->last_id);
        login.bad_pass_reason[0] = '\0';

        strncpy(hidden_str, pass_str, min(strlen(hidden_str), pass_len));
        strcpy(pass_str, hidden_str);
    }
    char input_pass_label[128] = "Password";
    if (confirm) {
        pass_match = pass_len && strcmp(login.pass_str, login.confirm_pass_str) == 0;
        sprintf(input_pass_label,
                pass_match ? "Passwords Match!" : "Confirm Password");
    }
    else if (login.bad_pass_reason[0] != '\0')
        sprintf(input_pass_label,
                "Invalid Password: %s",
                login.bad_pass_reason);
    mu_draw_control_text(ctx, input_pass_label,
                         mu_layout_next(ctx),
                         MU_COLOR_TEXT,
                         MU_OPT_ALIGNRIGHT);
    if (confirm) mu_pop_id(ctx);

    return pass_match;
}
void login_window(mu_Context *ctx) {
    char *title = "Login/Signup";
    mu_Container *window = mu_get_container(ctx, title);

    for (int i = 0; i < NETBUF_SIZE; i++) if (game.netbuf[i].msg) {
        NetToClient *ntc = &game.netbuf[i].ntc;
        bool consume = true;
        String user = string_from_nulterm(login.user_str),
               pass = string_from_nulterm(login.pass_str);

        String *reason;
        switch (ntc->kind) {
        case (NetToClientKind_AccountStatus):;
            if (!string_eq(&user, &ntc->account_status.user)) break;
            if (login.login != Login_User) break;

            login.login = ntc->account_status.exists ? Login_Exists : Login_Available;
            break;
        case (NetToClientKind_AccountBadUser):;
            String *bad_user = &ntc->account_bad_user.user;
            reason = &ntc->account_bad_user.reason;

            if (login.login != Login_User) break;
            if (!string_eq(&user, bad_user)) break;

            strncpy(login.bad_user_reason, reason->str, reason->len);
            login.bad_user_reason[reason->len] = '\0';
            break;
        case (NetToClientKind_AccountBadPass):;
            String *bad_pass = &ntc->account_bad_pass.pass;
            reason = &ntc->account_bad_pass.reason;

            if (!(login.login == Login_Pass
                  || login.login == Login_Register)) break;
            if (!string_eq(&pass, bad_pass)) break;

            strncpy(login.bad_pass_reason, reason->str, reason->len);
            login.bad_pass_reason[reason->len] = '\0';
            break;
        case (NetToClientKind_AccountTrialAccept):;
            login.login = Login_Trial;
            window->open = 0;
            strncpy(login.user_str,
                    ntc->account_trial_accept.user.str,
                    ntc->account_trial_accept.user.len);
            break;
        case (NetToClientKind_AccountLoginAccept):;
            login.login = Login_Done;
            strncpy(login.user_str,
                    ntc->account_login_accept.user.str,
                    ntc->account_login_accept.user.len);
            break;
        case (NetToClientKind_AccountServerError):;
            login.login = Login_ServerError;
            strncpy(login.server_error_str,
                    ntc->account_server_error.desc.str,
                    ntc->account_server_error.desc.len);
            break;
        /* whoops, you're not one of ours! */
        default: consume = false; break;
        }

        if (consume) {
            bqws_free_msg(game.netbuf[i].msg);
            game.netbuf[i].msg = NULL;
        }
    }
    if (login.login == Login_Done) return;
    if (login.login == Login_Trial || !window->open) {
        int opt = MU_OPT_AUTOSIZE | MU_OPT_NORESIZE |
                  MU_OPT_NOSCROLL | MU_OPT_NOTITLE;
        if (mu_begin_window_ex(ctx, "Trial", mu_rect(0, 100, 0, 0), opt)) {
            mu_layout_set_next(ctx, mu_rect(0, 0, 200, 30), 1);
            //if (mu_button(ctx, "Finish signing up to save progress")) {
            if (mu_button_ex(ctx, NULL, -(int) get_art(Art_Bag).id, MU_OPT_ALIGNCENTER)) {
                if (login.login == Login_Trial) login.login = Login_Register;
                window->open = 1;
            }
            mu_end_window(ctx);
        }
        return;
    }
    int w = 365, h = 225,
        view_w = sapp_width(), view_h = sapp_height(),
        x = (view_w - w) / 2, y = (view_h - h) / 2;
    if (mu_begin_window_ex(ctx, title, mu_rect(x, y, w, h), MU_OPT_NOCLOSE)) {
        mu_layout_set_next(ctx, mu_rect(40, 35, 270, 150), true);
        mu_layout_begin_column(ctx);
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);

        if (login.login == Login_ServerError) {
            mu_label(ctx, "Internal Server Error:");
            mu_label(ctx, login.server_error_str);
            if (mu_button(ctx, "Retry")) {
                login = (LoginWindow) {0};
            }
            mu_layout_end_column(ctx);
            mu_end_window(ctx);
            return;
        }

        int res = linked_textbox(ctx, login.user_str, sizeof(login.user_str));
        if (res & MU_RES_CHANGE) {
            mu_set_focus(ctx, ctx->last_id);
            login.login = Login_User;
            NetToServer_AccountExists exists = {
                .user = string_from_nulterm(login.user_str),
            };
            send_net_to_server_account_exists(exists, game.ws);
            login.bad_user_reason[0] = '\0';
        }
        char input_user_label[128] = "Username";
        if (login.bad_user_reason[0] != '\0')
            sprintf(input_user_label,
                    "Invalid Username: %s",
                    login.bad_user_reason);
        else if (login.login == Login_Available || login.login == Login_Trial)
            sprintf(input_user_label, "Available Username");
        else if (login.login == Login_Exists || login.login == Login_Pass)
            sprintf(input_user_label, "Registered Username");
        mu_draw_control_text(ctx, input_user_label,
                             mu_layout_next(ctx),
                             MU_COLOR_TEXT,
                             MU_OPT_ALIGNRIGHT);
        
        if (login.login == Login_Exists) {
            password_input(ctx, false);
            mu_checkbox(ctx, "Show Password", &login.show_pass);

            if (mu_button(ctx, "Done")) {
                NetToServer_AccountLogin log = {
                    .user = string_from_nulterm(login.user_str),
                    .pass = string_from_nulterm(login.pass_str),
                };
                send_net_to_server_account_login(log, game.ws);
            }
        }
        if (login.login == Login_Available) {
            mu_checkbox(ctx, "Skip Tutorial", &login.skip_tutorial);

            if (mu_button(ctx, "Play Now")) {
                NetToServer_AccountTrial trial = {
                    .user = string_from_nulterm(login.user_str),
                };
                send_net_to_server_account_trial(trial, game.ws);
            }

            if (mu_button(ctx, "Finish Signing up")) {
                login.login = Login_Register;
            }
        } else if (login.login == Login_Register) {
            password_input(ctx, false);
            password_input(ctx, true);
            mu_checkbox(ctx, "No Tutorial", &login.skip_tutorial);
            if (mu_button(ctx, "Done")) {
                NetToServer_AccountRegister reg = {
                    .user = string_from_nulterm(login.user_str),
                    .pass = string_from_nulterm(login.pass_str),
                };
                send_net_to_server_account_register(reg, game.ws);
            }
        }

        mu_layout_end_column(ctx);
        mu_end_window(ctx);
    }
}

void frame(void) {
    if (!rendr.loaded) {
        sfetch_dowork();
        return;
    }

    if (!game.generated) {
        generate_game();
        game.generated = true;
    }

    if (game.connector && (game.ws = bqws_pt_try_connector(game.connector))) {
        game.connector = NULL;
    }
    if (game.ws) {
        bqws_update(game.ws);
        bqws_msg *msg = NULL;
        while (msg = bqws_recv(game.ws)) {
            if (msg->type == BQWS_MSG_TEXT) {
                printf("[Server]  %.*s", (int)msg->size, msg->data);
                bqws_free_msg(msg);
            } else if (msg->type == BQWS_MSG_BINARY) {
                NetToClient ntc = unpack_net_to_client((u8 *) msg->data);
                bool stored = false;
                for (int i = 0; i < NETBUF_SIZE; i++)
                    if (game.netbuf[i].msg == NULL) {
                        game.netbuf[i].ntc = ntc;
                        game.netbuf[i].msg = msg;
                        stored = true;
                        break;
                    }
                if (!stored) printf("netbuf filled!");
                /* freed when consumed in netbuf */
            } else {
                printf("unknown message type?");
                bqws_free_msg(msg);
            }
        }
    }
    
    mu_begin(&game.mu_ctx);
    login_window(&game.mu_ctx);
    mu_end(&game.mu_ctx);

    Vec3 cam = game.cam->pos;
    start_render(cam);
    for (Ent *e = 0; e = ent_game_iter(e); )
        if (!has_ent_prop(e, EntProp_Hidden)) draw_ent(e);
    end_render();

    r_draw_commands(&game.mu_ctx, sapp_width(), sapp_height());
    sg_commit();

    game.elapsed = stm_sec(stm_since(game.start_time));
    game.dt = stm_sec(stm_since(game.last_render));
    game.last_render = stm_now();

    for (int i = 0; i < LEN(input.keys_pressed); i++)
        input.keys_pressed[i] = false;
}

void cleanup(void) {
    sfetch_shutdown();
    sg_shutdown();

    /* TODO: flush or whatever to make this work */
    if (game.ws) {
        bqws_close(game.ws, BQWS_CLOSE_GENERIC_ERROR, NULL, 0);
        bqws_free_socket(game.ws);
    }
    bqws_pt_shutdown();
}

sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    #ifdef TEST_SERV
    _beginthread(start_server, 0, NULL);
    #endif

    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .high_dpi = true,
        .width = 1280,
        .height = 720,
        .sample_count = 8,
        .gl_force_gles2 = true,
        .window_title = "emojia",
    };
}

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

#include "../sokol/sokol_time.h"
#include "../sokol/sokol_app.h"
#include "../sokol/sokol_gfx.h"
#include "../sokol/sokol_glue.h"
#include "../sokol/sokol_fetch.h"

#include "renderer.h"

static struct {
    bool keys_down[350];
    bool keys_pressed[350];

    /* is the left mouse button down? */
    bool left_mb_down;
    Vec2 mouse_pos;
} input;



/* --------- WORLD */

typedef enum {
    Login_User, Login_ServerError,
    Login_Exists, Login_Trial, Login_Register,
    Login_Available, Login_Pass,
    Login_Done, 
} Login;

typedef struct Ent Ent;
typedef enum {
    EntProp_Active,
    EntProp_Phys,
    EntProp_Hidden,
    EntProp_COUNT,
} EntProp;
struct Ent {
    /* specifies which of the following field groups are valid */
    u64 props[(EntProp_COUNT + 63)/64];

    /* appearance */
    Shape shape;
    Vec4 base_color, tint_color;
    SubImg img;

    /* bob animation */
    f32 bob_scale, bob_freq;

    /* positioning */
    Ent *parent, *first_child, *last_child, *next, *prev;
    Vec3 pos, rot, scale;
    /* If true, children of this ent are not positioned relative to it */
    bool independent_children;

    /* physics & gravity: EntProp_Phys */
    Vec3 vel, acc;
    f32 mass;
};

#define MAX_ENTS 2000
#define NETBUF_SIZE 100
static struct {
    bool generated;
    u64 start_time, last_render;
    f64 dt, elapsed;

    Ent ents[MAX_ENTS];
    Ent *cam;

    Login login;
    bqws_socket *ws;
    struct { bqws_msg *msg; NetToClient ntc; } netbuf[NETBUF_SIZE];
} game;
#define ENT_STORE game.ents
#include "ent.h"
#include "ui.h"

// #define TEST_SERV
/* when this is defined, a server is started in a
   separate thread for singleplayer testing */
#ifdef TEST_SERV
#define EMBED_SERV
#include <pthread.h>
#include "../server/main.c"

void *srv_thr_func(void *arg) {
    (void) arg;
    start_server();
    pthread_exit(NULL);
}
#endif

/* sokol will call these functions, our game exists in them */
void init(void); // called once at the beginning
void frame(void); // called every frame, rendering happens here
void cleanup(void); // you can dealloc things here, but kinda unnecessary
void event(const sapp_event* ev); // tells us about key presses, other input
sapp_desc sokol_main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    #ifdef TEST_SERV
    pthread_t srv_thread = 0;
    assert(0 == pthread_create(&srv_thread, NULL, srv_thr_func, NULL));
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
        .window_title = "swashbucklers",
    };
}

void init(void) {
    srandf(9, 12, 32, 10);

    game.cam = add_ent();
    give_ent_prop(game.cam, EntProp_Hidden);

    sfetch_setup(&(sfetch_desc_t){ .num_lanes = Art_COUNT });
    rendr_load();
    stm_setup();

    ui_init();

    bqws_pt_init(NULL);
    game.ws = bqws_pt_connect("ws://localhost:80", NULL, NULL, NULL);

    game.start_time = stm_now();
    game.dt = 0.0;
    game.last_render = stm_now();
}

void event(const sapp_event *ev) {
    ui_event(ev);

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

void net_frame(void);
void frame(void) {
    if (!rendr.loaded) {
        sfetch_dowork();
        return;
    }

    if (!game.generated) {
        {
            Ent *e = add_ent();
            e->img = art_sub_img(Art_Wizard);
            e->pos = vec3(1.0f, 0.0f, 1.0f);
        }
        game.generated = true;
    }

    if (game.ws) net_frame();
    
    Vec3 cam = game.cam->pos;
    start_render(cam);
    for (Ent *e = 0; (e = ent_game_iter(e)); )
        if (!has_ent_prop(e, EntProp_Hidden)) draw_ent(e);
    end_render();

    ui_frame();
    sg_commit();

    game.elapsed = stm_sec(stm_since(game.start_time));
    game.dt = stm_sec(stm_since(game.last_render));
    game.last_render = stm_now();

    for (int i = 0; i < LEN(input.keys_pressed); i++)
        input.keys_pressed[i] = false;
}

void net_frame(void) {
    bqws_update(game.ws);
    bqws_msg *msg = NULL;
    while ((msg = bqws_recv(game.ws))) {
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

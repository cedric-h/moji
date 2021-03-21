static struct {
    mu_Context mu_ctx;
} ui;

#include "ui/login_window.h"

void _inventory_window(mu_Context *ctx) {
    (void) ctx;
    int opt = MU_OPT_AUTOSIZE | MU_OPT_NOFRAME | MU_OPT_NOTITLE;
    if (mu_begin_window_ex(ctx, "Inventory Opener", mu_rect(0, 150, 0, 0), opt)) {
        mu_layout_set_next(ctx, mu_rect(0, 0, 72, 72), 1);
        if (mu_button_ex(ctx, 0, 0, Art_Bag, MU_OPT_NOFRAME)) {
        }
        mu_end_window(ctx);
    }
}


static int text_width(mu_Font font, const char *text, int len) {
    (void) font;
    if (len == -1) { len = (int) strlen(text); }
    return r_get_text_width(text, len);
}

static int text_height(mu_Font font) {
    (void) font;
    return r_get_text_height();
}

void ui_init(void) {
    mu_init(&ui.mu_ctx);

    ui.mu_ctx.text_width = text_width;
    ui.mu_ctx.text_height = text_height;
}

void ui_frame(void) {
    mu_begin(&ui.mu_ctx);
    _login_window(&ui.mu_ctx);
    _inventory_window(&ui.mu_ctx);
    mu_end(&ui.mu_ctx);

    r_draw_commands(&ui.mu_ctx, sapp_width(), sapp_height());
}

void ui_event(const sapp_event *ev) {
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

    switch (ev->type) {
    case SAPP_EVENTTYPE_MOUSE_DOWN:
        mu_input_mousedown(&ui.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
        break;
    case SAPP_EVENTTYPE_MOUSE_UP:
        mu_input_mouseup(&ui.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y, (1<<ev->mouse_button));
        break;
    case SAPP_EVENTTYPE_MOUSE_MOVE:
        mu_input_mousemove(&ui.mu_ctx, (int)ev->mouse_x, (int)ev->mouse_y);
        break;
    case SAPP_EVENTTYPE_MOUSE_SCROLL:
        mu_input_scroll(&ui.mu_ctx, 0, (int)ev->scroll_y);
        break;
    case SAPP_EVENTTYPE_KEY_DOWN:
        mu_input_keydown(&ui.mu_ctx, key_map[ev->key_code & 511]);
        break;
    case SAPP_EVENTTYPE_KEY_UP:
        mu_input_keyup(&ui.mu_ctx, key_map[ev->key_code & 511]);
        break;
    case SAPP_EVENTTYPE_CHAR:
        {
            char txt[2] = { (char)(ev->char_code & 255), 0 };
            mu_input_text(&ui.mu_ctx, txt);
        }
        break;
    default:
        break;
    }

}

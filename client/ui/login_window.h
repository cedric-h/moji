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
} LoginWindow;
static LoginWindow login;
/* if confirm is enabled, returns true if "confirm password" input matches original */
bool password_input(mu_Context *ctx, bool confirm) {
    bool pass_match = false;

    if (confirm) mu_push_id(ctx, "confirm", sizeof("confirm"));
    char hidden_str[128],
         *pass_str = confirm ? login.confirm_pass_str : login.pass_str;
    int pass_len = (int) strlen(pass_str);
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
    char input_pass_label[400] = "Password";
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
void _login_window(mu_Context *ctx) {
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
            if (game.login != Login_User) break;

            game.login = ntc->account_status.exists ? Login_Exists : Login_Available;
            break;
        case (NetToClientKind_AccountBadUser):;
            String *bad_user = &ntc->account_bad_user.user;
            reason = &ntc->account_bad_user.reason;

            if (game.login != Login_User) break;
            if (!string_eq(&user, bad_user)) break;

            strncpy(login.bad_user_reason, reason->str, reason->len);
            login.bad_user_reason[reason->len] = '\0';
            break;
        case (NetToClientKind_AccountBadPass):;
            String *bad_pass = &ntc->account_bad_pass.pass;
            reason = &ntc->account_bad_pass.reason;

            if (!(game.login == Login_Pass
                  || game.login == Login_Register)) break;
            if (!string_eq(&pass, bad_pass)) break;

            strncpy(login.bad_pass_reason, reason->str, reason->len);
            login.bad_pass_reason[reason->len] = '\0';
            break;
        case (NetToClientKind_AccountTrialAccept):;
            game.login = Login_Trial;
            window->open = 0;
            strncpy(login.user_str,
                    ntc->account_trial_accept.user.str,
                    ntc->account_trial_accept.user.len);
            break;
        case (NetToClientKind_AccountLoginAccept):;
            game.login = Login_Done;
            strncpy(login.user_str,
                    ntc->account_login_accept.user.str,
                    ntc->account_login_accept.user.len);
            break;
        case (NetToClientKind_AccountServerError):;
            game.login = Login_ServerError;
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
    if (game.login == Login_Done) return;
    if (game.login == Login_Trial || !window->open) {
        int opt = MU_OPT_AUTOSIZE | MU_OPT_NORESIZE |
                  MU_OPT_NOSCROLL | MU_OPT_NOTITLE;
        if (mu_begin_window_ex(ctx, "Trial", mu_rect(0, 100, 0, 0), opt)) {
            mu_layout_set_next(ctx, mu_rect(0, 0, 200, 30), 1);
            if (mu_button(ctx, "Finish signing up to save progress")) {
                if (game.login == Login_Trial) game.login = Login_Register;
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

        if (game.login == Login_ServerError) {
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
            game.login = Login_User;
            NetToServer_AccountExists exists = {
                .user = string_from_nulterm(login.user_str),
            };
            send_net_to_server_account_exists(exists, game.ws);
            login.bad_user_reason[0] = '\0';
        }
        char input_user_label[400] = "Username";
        if (login.bad_user_reason[0] != '\0')
            sprintf(input_user_label,
                    "Invalid Username: %s",
                    login.bad_user_reason);
        else if (game.login == Login_Available || game.login == Login_Trial)
            sprintf(input_user_label, "Available Username");
        else if (game.login == Login_Exists || game.login == Login_Pass)
            sprintf(input_user_label, "Registered Username");
        mu_draw_control_text(ctx, input_user_label,
                             mu_layout_next(ctx),
                             MU_COLOR_TEXT,
                             MU_OPT_ALIGNRIGHT);
        
        if (game.login == Login_Exists) {
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
        if (game.login == Login_Available) {
            mu_checkbox(ctx, "Skip Tutorial", &login.skip_tutorial);

            if (mu_button(ctx, "Play Now")) {
                NetToServer_AccountTrial trial = {
                    .user = string_from_nulterm(login.user_str),
                };
                send_net_to_server_account_trial(trial, game.ws);
            }

            if (mu_button(ctx, "Finish Signing up")) {
                game.login = Login_Register;
            }
        } else if (game.login == Login_Register) {
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

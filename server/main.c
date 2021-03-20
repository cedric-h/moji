#define _CRT_SECURE_NO_WARNINGS

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <memory.h>
#include <assert.h>
#include <errno.h>

#define MAX_USER_LEN (20)
#define MAX_PASS_LEN (80)
#define PFILE_PATH "../pfiles/"

#ifndef EMBED_SERV
#include "../common/common.h"
#include "../bq_websocket/bq_websocket.h"
#include "../bq_websocket/bq_websocket_platform.h"
#endif

typedef enum { Signin_No, Signin_Trial, Signin_Full } Signin;
typedef struct {
    Signin signin;
    char user[MAX_USER_LEN], pass[MAX_PASS_LEN];
    bqws_socket *ws;
} Client;

#define MAX_CLIENTS 128
static struct {
    Client clients[MAX_CLIENTS];
} state;

void update_client_socket(Client*);

#ifdef EMBED_SERV
void start_server() {
#else
int main() {
#endif
    bqws_pt_init(NULL);
    bqws_pt_server *sv = bqws_pt_listen(&(bqws_pt_listen_opts) { .port = 6666 });

    for (;;) {
        /* Accept new connections */
        bqws_socket *new_ws = bqws_pt_accept(sv, NULL, NULL);
        if (new_ws) {
            for (size_t i = 0; i < MAX_CLIENTS; i++) {
                if (state.clients[i].ws == NULL) {
                    bqws_server_accept(new_ws, NULL);
                    state.clients[i] = (Client) {
                        .ws = new_ws,
                    };
                    new_ws = NULL;
                    break;
                }
            }
            bqws_free_socket(new_ws);
        }

        /* Update existing clients */
        for (size_t i = 0; i < MAX_CLIENTS; i++)
            if (state.clients[i].ws)
                update_client_socket(&state.clients[i]);

        bqws_pt_sleep_ms(10);
    }

    /* technically unreachable :( */
    // bqws_pt_free_server(sv);
    // bqws_pt_shutdown();
}

void apply_client_request(Client *client, NetToServer req);
void update_client_socket(Client *client) {
    bqws_socket *ws = client->ws;

    bqws_update(ws);
    bqws_msg *msg;
    while ((msg = bqws_recv(ws)) != NULL) {
        if (msg->type == BQWS_MSG_TEXT) {
            printf("msg: %.*s\n", (int) msg->size, msg->data);
        } else if (msg->type == BQWS_MSG_BINARY) {
            uint8_t *data = (uint8_t *) msg->data;
            apply_client_request(client, unpack_net_to_server(data));
        } else {
            bqws_close(ws, BQWS_CLOSE_GENERIC_ERROR, NULL, 0);
        }
        bqws_free_msg(msg);
    }

    if (bqws_is_closed(ws)) {
        /* Free the socket and slot */
        bqws_free_socket(ws);
        client->ws = NULL;
    }
}

bool pfile_exists(int user_len, char *user) {
    char fpath[40 + MAX_USER_LEN];
    sprintf(fpath, "%s/%.*s.pfile", PFILE_PATH, user_len, user);

    return !(fopen(fpath, "rb") == NULL && errno == ENOENT);
}

void send_pfile_io_err(Client *client) {
    char desc[256], *err_str = strerror(errno);
    int err_str_len = (int) min(235, strlen(err_str));
    sprintf(desc, "pfile i/o: %d (%.*s)", errno, err_str_len, err_str);
    NetToClient_AccountServerError res = { .desc = string_from_nulterm(desc) };
    send_net_to_client_account_server_error(res, client->ws);
}

bool make_pfile(Client *client, int user_len, char *user, int len, char *buf) {
    char fpath[40 + MAX_USER_LEN];
    sprintf(fpath, "%s/%.*s.pfile", PFILE_PATH, user_len, user);

    FILE* f = fopen(fpath, "w");
    if (f != NULL) {
        fwrite(buf, 1, len, f);
        return true;
    } else {
        send_pfile_io_err(client);
        return false;
    }
}

int read_pfile(Client *client, int user_len, char *user, int len, char *buf) {
    char fpath[40 + MAX_USER_LEN];
    sprintf(fpath, "%s/%.*s.pfile", PFILE_PATH, user_len, user);

    FILE* f = fopen(fpath, "r+b");
    if (f != NULL) return (int) fread(buf, 1, len, f);
    else {
        send_pfile_io_err(client);
        return 0;
    }
}

bool user_valid(Client *client, String *user) {
    char wrong[256] = "";
    assert(wrong[0] == '\0');

    if (user->len < 3)
        sprintf(wrong, "needs at least three characters");
    else if (user->len > MAX_USER_LEN)
        sprintf(wrong, "needs fewer than %d characters", MAX_USER_LEN);
    else for (int i = 0; i < user->len; i++)
        if (!isalpha(user->str[i]))
            sprintf(wrong, "must be alphabetic");
    
    if (wrong[0] == '\0') return true;
    else {
        NetToClient_AccountBadUser res = {
            .user = *user,
            .reason = string_from_nulterm(wrong),
        };
        send_net_to_client_account_bad_user(res, client->ws);
        return false;
    }
}

bool pass_valid(Client *client, String *pass) {
    char wrong[256] = "";
    assert(wrong[0] == '\0');

    if (pass->len < 6)
        sprintf(wrong, "needs at least six characters");
    else if (pass->len > MAX_USER_LEN)
        sprintf(wrong, "needs fewer than %d characters", MAX_PASS_LEN);
    
    if (wrong[0] == '\0') return true;
    else {
        NetToClient_AccountBadPass res = {
            .pass = *pass,
            .reason = string_from_nulterm(wrong),
        };
        send_net_to_client_account_bad_pass(res, client->ws);
        return false;
    }
}

void apply_client_request(Client *client, NetToServer nts) {
    String *user, *pass;
    switch (nts.kind) {
    case (NetToServerKind_AccountExists):;
        user = &nts.account_exists.user;
        if (!user_valid(client, user)) break;

        NetToClient_AccountStatus res = {
            .user = *user,
            .exists = pfile_exists(unspool(*user)),
        };
        send_net_to_client_account_status(res, client->ws);
        break;
    case (NetToServerKind_AccountTrial):;
        user = &nts.account_trial.user;

        if (client->signin == Signin_Full) break;
        if (!user_valid(client, user)) break;
        if (pfile_exists(unspool(*user))) break;

        client->signin = Signin_Trial;
        send_net_to_client_account_trial_accept(
            (NetToClient_AccountTrialAccept) { .user = *user },
            client->ws
        );
        break;
    case (NetToServerKind_AccountRegister):;
        user = &nts.account_register.user;
        pass = &nts.account_register.pass;

        if (client->signin == Signin_Full) break;
        if (!user_valid(client, user)) break;
        if (!pass_valid(client, pass)) break;
        if (pfile_exists(unspool(*user))) break;

        if (!make_pfile(client, unspool(*user), unspool(*pass))) break;
        client->signin = Signin_Full;
        send_net_to_client_account_login_accept(
            (NetToClient_AccountLoginAccept) { .user = *user },
            client->ws
        );
        break;
    case (NetToServerKind_AccountLogin):;
        user = &nts.account_login.user;
        pass = &nts.account_login.pass;

        if (client->signin == Signin_Full) break;
        if (!user_valid(client, user)) break;
        if (!pass_valid(client, pass)) break;
        if (!pfile_exists(unspool(*user))) break;

        char stored_pass_str[MAX_PASS_LEN];
        int len = read_pfile(client, unspool(*user), MAX_PASS_LEN, stored_pass_str);
        if (len == 0) break;
        String stored_pass = { .str = stored_pass_str, .len = (uint8_t) len };

        if (string_eq(&stored_pass, pass)) {
            client->signin = Signin_Full;
            send_net_to_client_account_login_accept(
                (NetToClient_AccountLoginAccept) { .user = *user },
                client->ws
            );
        } else {
            NetToClient_AccountBadPass bad_pass = {
                .pass = *pass,
                .reason = string_from_nulterm("doesn't match what we have; incorrect"),
            };
            send_net_to_client_account_bad_pass(bad_pass, client->ws);
        }

        break;
    }
}

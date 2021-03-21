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

/* tracks how logged in a client is */
typedef enum { _srv_Login_No, _srv_Login_Trial, _srv_Login_Full } _srv_Login;
typedef struct {
    _srv_Login signin;
    char user[MAX_USER_LEN], pass[MAX_PASS_LEN];
    bqws_socket *ws;
} _srv_Client;

#define MAX_CLIENTS 128
/* internal server state */
static struct {
    _srv_Client clients[MAX_CLIENTS];
} _srv;

/* call once per tick, handles client input */
void _srv_update_client_socket(_srv_Client*);

#ifdef EMBED_SERV
void srv_start() {
#else
int main() {
#endif
    bqws_pt_init(NULL);
    bqws_pt_server *sv = bqws_pt_listen(&(bqws_pt_listen_opts) { .port = 80 });

    int tick = 0;
    for (;;) {
        tick++;

        /* Accept new connections */
        bqws_socket *new_ws = bqws_pt_accept(sv, NULL, NULL);
        if (new_ws) {
            for (size_t i = 0; i < MAX_CLIENTS; i++) {
                if (_srv.clients[i].ws == NULL) {
                    bqws_server_accept(new_ws, NULL);
                    _srv.clients[i] = (_srv_Client) {
                        .ws = new_ws,
                    };
                    new_ws = NULL;
                    break;
                }
            }
            bqws_free_socket(new_ws);
        }

        /* Update existing clients */
        int connection_count = 0;
        for (size_t i = 0; i < MAX_CLIENTS; i++)
            if (_srv.clients[i].ws) {
                connection_count++;
                _srv_update_client_socket(&_srv.clients[i]);
            }

        /* tacky ASCII animation ensures you the server is updating */
        char anim = "|\\-/"[tick / 10 % 4];
        printf("\r[%c %d users online %c]     ", anim, connection_count, anim);
        fflush(stdout);

        bqws_pt_sleep_ms(10);
    }

    /* technically unreachable :( */
    // bqws_pt_free_server(sv);
    // bqws_pt_shutdown();
}

void _srv_apply_client_request(_srv_Client *client, NetToServer req);
/* prints text websocket messages to stdout,
   deserializes and applies to binary ones,
   and closes empty sockets. */
void _srv_update_client_socket(_srv_Client *client) {
    bqws_socket *ws = client->ws;

    bqws_update(ws);
    bqws_msg *msg;
    while ((msg = bqws_recv(ws)) != NULL) {
        if (msg->type == BQWS_MSG_TEXT) {
            printf("msg: %.*s\n", (int) msg->size, msg->data);
        } else if (msg->type == BQWS_MSG_BINARY) {
            uint8_t *data = (uint8_t *) msg->data;
            _srv_apply_client_request(client, unpack_net_to_server(data));
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

/* returns true if a player file with the given name already exists.
   in the event of file i/o problems, still returns true, to prevent
   potentially allowing someone to reserve an in-use username. */
bool _srv_pfile_exists(int user_len, char *user) {
    char fpath[40 + MAX_USER_LEN];
    sprintf(fpath, "%s/%.*s.pfile", PFILE_PATH, user_len, user);

    return !(fopen(fpath, "rb") == NULL && errno == ENOENT);
}

/* sends a NetToClient_AccountServerError */
void _srv_send_pfile_io_err(_srv_Client *client) {
    char desc[256], *err_str = strerror(errno);
    int err_str_len = (int) min(235, strlen(err_str));
    sprintf(desc, "pfile i/o: %d (%.*s)", errno, err_str_len, err_str);
    NetToClient_AccountServerError res = { .desc = string_from_nulterm(desc) };
    send_net_to_client_account_server_error(res, client->ws);
}

/* makes a new player file of up to `len` chars, filled with content from `buf`.
   the username becomes the name of the new pfile.
   returns false and sends a NetToClient_AccountServerError
   if file i/o won't work well enough to facilitate this. */
bool _srv_make_pfile(_srv_Client *client, int user_len, char *user, int len, char *buf) {
    char fpath[40 + MAX_USER_LEN];
    sprintf(fpath, "%s/%.*s.pfile", PFILE_PATH, user_len, user);

    FILE* f = fopen(fpath, "w");
    if (f != NULL) {
        fwrite(buf, 1, len, f);
        return true;
    } else {
        _srv_send_pfile_io_err(client);
        return false;
    }
}

/* reads up to `len` chars of a player file into `buf`.
   `user` specifies whose pfile to find.
   returns false and sends a NetToClient_AccountServerError
   if file i/o won't work well enough to facilitate this. */
int _srv_read_pfile(_srv_Client *client, int user_len, char *user, int len, char *buf) {
    char fpath[40 + MAX_USER_LEN];
    sprintf(fpath, "%s/%.*s.pfile", PFILE_PATH, user_len, user);

    FILE* f = fopen(fpath, "r+b");
    if (f != NULL) return (int) fread(buf, 1, len, f);
    else {
        _srv_send_pfile_io_err(client);
        return 0;
    }
}

/* checks if a username is invalid,
   if it is not valid returns false and sends a NetToCilent_AccountBadUser */
bool _srv_user_valid(_srv_Client *client, String *user) {
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

/* checks if a password is invalid,
   if it is not valid returns false and sends a NetToCilent_AccountBadPass */
bool _srv_pass_valid(_srv_Client *client, String *pass) {
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

void _srv_apply_client_request(_srv_Client *client, NetToServer nts) {
    String *user, *pass;
    switch (nts.kind) {
    case (NetToServerKind_AccountExists):;
        user = &nts.account_exists.user;
        /* let them know if the username is invalid and quit if so */
        if (!_srv_user_valid(client, user)) break;

        NetToClient_AccountStatus res = {
            .user = *user,
            .exists = _srv_pfile_exists(unspool(*user)),
        };
        send_net_to_client_account_status(res, client->ws);
        break;
    case (NetToServerKind_AccountTrial):;
        user = &nts.account_trial.user;

        /* error handling */
        if (client->signin == _srv_Login_Full) break;
        if (!_srv_user_valid(client, user)) break;
        if (_srv_pfile_exists(unspool(*user))) break;

        client->signin = _srv_Login_Trial;
        send_net_to_client_account_trial_accept(
            (NetToClient_AccountTrialAccept) { .user = *user },
            client->ws
        );
        break;
    case (NetToServerKind_AccountRegister):;
        user = &nts.account_register.user;
        pass = &nts.account_register.pass;

        /* error handling */
        if (client->signin == _srv_Login_Full) break;
        if (!_srv_user_valid(client, user)) break;
        if (!_srv_pass_valid(client, pass)) break;
        if (_srv_pfile_exists(unspool(*user))) break;

        if (!_srv_make_pfile(client, unspool(*user), unspool(*pass))) break;
        client->signin = _srv_Login_Full;
        send_net_to_client_account_login_accept(
            (NetToClient_AccountLoginAccept) { .user = *user },
            client->ws
        );
        break;
    case (NetToServerKind_AccountLogin):;
        user = &nts.account_login.user;
        pass = &nts.account_login.pass;

        /* error handling */
        if (client->signin == _srv_Login_Full) break;
        if (!_srv_user_valid(client, user)) break;
        if (!_srv_pass_valid(client, pass)) break;
        if (!_srv_pfile_exists(unspool(*user))) break;

        char stored_pass_str[MAX_PASS_LEN];
        int len = _srv_read_pfile(client, unspool(*user), MAX_PASS_LEN, stored_pass_str);
        if (len == 0) break;
        String stored_pass = { .str = stored_pass_str, .len = (uint8_t) len };

        if (string_eq(&stored_pass, pass)) {
            client->signin = _srv_Login_Full;
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

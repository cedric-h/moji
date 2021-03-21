# Networking Messages
The server and the client need to know what kind of messages to expect from each other.

To help with this, a metaprogram found in `formpack/` turns a declarative,
high-level description of networking messages into C code for serializing,
deserializing and verifying messages of the specified layout.

An example of a naive protocol described in `formpack/formpack.dd`
```rs
@tagun NetToServer: {
    RequestMove: { x: float, y: float },
    AttackAt: { x: float, y: float },
}

@tagun NetToClient: {
    SpawnAt: { id: uint16_t, x: float, y: float },
    MoveTo: { id: uint16_t, x: float, y: float },
}
```
The format here is [metadesk](https://github.com/Dion-Systems/metadesk).
The `@tagun` tag denotes a "tagged union," analgous to what Rust calls an `enum`.

The source code for the metaprogram is in `formpack/gen.c`.
It's built using its own CMakeLists.txt which the main project's CMake initiates
whenever `formpack/formpack.dd` or `formpack/gen.c` change.

While code for the serializing the structs and tagged unions is generated, code for serializing/deserializing the basic datatypes (`bool`, `uint16_t`, `String`, etc.) can be found in `common/common.h`.

## Handling the variants
If you had a pointer to a `NetToServer` named `nts` in C, you could use `nts->kind` to access
the tagged union's tag. In this example, for a `NetToServer` message, the only valid tags are
`NetToServerKind_RequestMove`, and `NetToServerKind_AttackAt`. Typically, usage code uses a
switch/case to enumerate the variants.
```c
switch (nts->kind) {
case (NetToServerKind_RequestMove):;
    game_state.ents[nts.request_move.id].x = nts.request_move.x;
    game_state.ents[nts.request_move.id].y = nts.request_move.y;
    break;
case (NetToServerKind_SpawnAt):;
    /* here the message variant is aliased to simplify access */
    NetToServer_SpawnAt *msg = &nts->spawn_at;
    game_state.ents[msg->id] = (Ent) {
        .active = true,
        .x = msg->x,
        .y = msg->y,
    };
    break;
    break;
}
```

## Sending net messages
Unfortunately, the API for this is a bit repetitive. Less verbose alternatives were considered, but without verbosity thorough type checking of the inputs could not be ensured.
```c
    NetToClient_MoveTo mvt = {
        .id = client->character,
        .x = 100.0f,
        .y = 100.0f,
    };
    send_net_to_client_move_to(mvt, client->ws);
```

Often seen alternatively with in-place initialization:
```c
    send_net_to_client_move_to(
        (NetToClient_MoveTo) {
            .id = client->character,
            .x = 100.0f,
            .y = 100.0f,
        },
        client->ws
    );
```

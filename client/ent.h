/* define ENT_STORE, point it toward some entity storage */

INLINE bool has_ent_prop(Ent *ent, EntProp prop) {
    return !!(ent->props[prop/64] & ((u64)1 << (prop%64)));
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

/* Use this function to iterate over all of the Ents in the game.
   ex:
        for (Ent *e = 0; e = ent_game_iter(e); )
            draw_ent(e);
*/
INLINE Ent *ent_game_iter(Ent *ent) {
    if (ent == NULL) ent = ENT_STORE - 1;

    while (!has_ent_prop((++ent), EntProp_Active))
        if ((ent - ENT_STORE) == MAX_ENTS)
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
        if (!has_ent_prop(&ENT_STORE[i], EntProp_Active)) {
            Ent *slot = &ENT_STORE[i];
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

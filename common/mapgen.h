typedef enum {
    _mapgen_Biome_Ocean4,
    _mapgen_Biome_Ocean3,
    _mapgen_Biome_Ocean2,
    _mapgen_Biome_Ocean1,
    _mapgen_Biome_Sand1,
    _mapgen_Biome_Sand2,
    _mapgen_Biome_Sand3,
    _mapgen_Biome_Sand4,
    _mapgen_Biome_Sand5,
    _mapgen_Biome_Plain,
    _mapgen_Biome_Meadow,
    _mapgen_Biome_Wood1,
    _mapgen_Biome_Wood2,
    _mapgen_Biome_Wood3,
    _mapgen_Biome_OOB, // out of bounds
} _mapgen_Biome;

/* maps the output of the fractal brownian motion to one of the biomes */
_mapgen_Biome _mapgen_fbm_to_biome(float fbm) {
    if (fbm < -1.0f ) return _mapgen_Biome_Ocean4;
    if (fbm < -0.8f ) return _mapgen_Biome_Ocean3;
    if (fbm < -0.5f ) return _mapgen_Biome_Ocean2;
    if (fbm <  0.0f ) return _mapgen_Biome_Ocean1;
    if (fbm <  0.02f) return _mapgen_Biome_Sand1;
    if (fbm <  0.04f) return _mapgen_Biome_Sand2;
    if (fbm <  0.06f) return _mapgen_Biome_Sand3;
    if (fbm <  0.08f) return _mapgen_Biome_Sand4;
    if (fbm <  0.10f) return _mapgen_Biome_Sand5;
    if (fbm <  0.4f ) return _mapgen_Biome_Plain;
    if (fbm <  0.6f ) return _mapgen_Biome_Meadow;
    if (fbm <  0.8f ) return _mapgen_Biome_Wood1;
    if (fbm <  1.5f ) return _mapgen_Biome_Wood2;
    if (fbm <  2.0f ) return _mapgen_Biome_Wood3;
    return _mapgen_Biome_OOB;
}

float _mapgen_snoise(Vec2 p) {
    return simplex2(p) * 0.7f;
}

float _mapgen_fbm(Vec2 p) {
    Vec2 step = vec2(1.3f, 1.7f);
    return _mapgen_snoise(p)
           + (1.0f / 2.0f  ) * _mapgen_snoise(sub2(mul2f(p, 2.0f  ), mul2f(step, 1.0f)))
           + (1.0f / 4.0f  ) * _mapgen_snoise(sub2(mul2f(p, 4.0f  ), mul2f(step, 2.0f)))
           + (1.0f / 8.0f  ) * _mapgen_snoise(sub2(mul2f(p, 8.0f  ), mul2f(step, 3.0f)))
           + (1.0f / 16.0f ) * _mapgen_snoise(sub2(mul2f(p, 16.0f ), mul2f(step, 4.0f)))
           + (1.0f / 32.0f ) * _mapgen_snoise(sub2(mul2f(p, 32.0f ), mul2f(step, 5.0f)))
           + (1.0f / 64.0f ) * _mapgen_snoise(sub2(mul2f(p, 64.0f ), mul2f(step, 6.0f)))
           + (1.0f / 128.0f) * _mapgen_snoise(sub2(mul2f(p, 128.0f), mul2f(step, 7.0f)))
           + (1.0f / 256.0f) * _mapgen_snoise(sub2(mul2f(p, 256.0f), mul2f(step, 8.0f)))
           //(1.0f / 512.0f) * sample(sub2(mul2f(p, 512.0f), mul2f(step, 16.0f)))
           ;
}

float _mapgen_island_weighted_fbm(Vec2 p) {
    float fbm = _mapgen_fbm(p);

    /* distance to center, circular distance */
    float cd = mag2(sub2f(p, 0.5));

    #define IN  ( 0.2)
    #define OUT (-0.5)
    #define FAR (-0.8)
    if (cd < 0.3)      fbm += IN;
    else if (cd < 0.4) fbm += lerp( IN, OUT, smoothstep(inv_lerp(0.3, 0.4, cd)));
    else if (cd < 0.5) fbm += lerp(OUT, FAR, smoothstep(inv_lerp(0.4, 0.5, cd)));
    else               fbm += FAR;

    return fbm;
}

void mapgen_ground_img(uint8_t *img, int size) {
    for (int x = 0; x < size; x++)
    for (int y = 0; y < size; y++) {
        typedef struct { uint8_t r; uint8_t g; uint8_t b; } Col;
        Col rgb;
        Vec2 p = div2f(vec2(x, y), size);
        float fbm = _mapgen_island_weighted_fbm(p);
        switch (_mapgen_fbm_to_biome(fbm)) {
        case (_mapgen_Biome_Ocean4): rgb = (Col) {180, 180, 185}; break; // (clouded)
        case (_mapgen_Biome_Ocean3): rgb = (Col) {200, 200, 205}; break; // (clouded)
        case (_mapgen_Biome_Ocean2): rgb = (Col) { 50,  80, 140}; break;
        case (_mapgen_Biome_Ocean1): rgb = (Col) { 60, 100, 160}; break;
        case (_mapgen_Biome_Sand1 ): rgb = (Col) {230, 200, 110}; break;
        case (_mapgen_Biome_Sand2 ): rgb = (Col) {215, 185, 100}; break;
        case (_mapgen_Biome_Sand3 ): rgb = (Col) {202, 170,  95}; break;
        case (_mapgen_Biome_Sand4 ): rgb = (Col) {175, 140,  80}; break;
        case (_mapgen_Biome_Sand5 ): rgb = (Col) {148, 110,  65}; break;
        case (_mapgen_Biome_Plain ): rgb = (Col) {100, 200, 120}; break;
        case (_mapgen_Biome_Meadow): rgb = (Col) { 65, 175, 100}; break;
        case (_mapgen_Biome_Wood1 ): rgb = (Col) { 30, 150,  80}; break;
        case (_mapgen_Biome_Wood2 ): rgb = (Col) {200, 205, 200}; break; // (clouded)
        case (_mapgen_Biome_Wood3 ): rgb = (Col) {180, 185, 180}; break; // (clouded)
        default                    : rgb = (Col) {255,   0, 255}; break; // eye grabbing magenta
        };
        int i = (y * size + x) * 4;
        img[i+0] = rgb.r;
        img[i+1] = rgb.g;
        img[i+2] = rgb.b;
        img[i+3] = 255;
    }
}

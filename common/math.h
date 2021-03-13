typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float    f32;
typedef double   f64;

#define PI32  3.14159265359f
#define TAU32 6.28318530718f
#define INLINE static inline

INLINE f32 signum(f32 x) {
    return (f32) (x > 0) - (x < 0);
}

INLINE u32 rotl(const u32 x, int k) {
    return (x << k) | (x >> (32 - k));
}

static u32 rand_seed[4];

/* source: http://prng.di.unimi.it/xoshiro128plus.c
   NOTE: The state must be seeded so that it is not everywhere zero. */
u32 rand_u32(void) {
    const u32 result = rand_seed[0] + rand_seed[3],
              t = rand_seed[1] << 9;

    rand_seed[2] ^= rand_seed[0];
    rand_seed[3] ^= rand_seed[1];
    rand_seed[1] ^= rand_seed[2];
    rand_seed[0] ^= rand_seed[3];

    rand_seed[2] ^= t;

    rand_seed[3] = rotl(rand_seed[3], 11);

    return result;
}

INLINE void srandf(u32 s0, u32 s1, u32 s2, u32 s3) {
    rand_seed[0] = s0;
    rand_seed[1] = s1;
    rand_seed[2] = s2;
    rand_seed[3] = s3;
}

INLINE f32 randf(void) {
    return (rand_u32() >> 8) * 0x1.0p-24f;
}

INLINE f32 wrap(f32 a, f32 b) {
    return (a > b) ? (a - b) : a;
}

/* Lerps from angle a to b, taking the shortest path */
f32 lerp_rad(f32 a, f32 t, f32 b) {
    f32 difference = fmodf(b - a, TAU32),
        distance = fmodf(2.0f * difference, TAU32) - difference;
    return a + distance * t;
}

INLINE f32 clamp(f32 min, f32 val, f32 max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

INLINE f32 lerp(f32 a, f32 t, f32 b) {
    return (1.0f - t) * a + t * b;
}

INLINE f32 to_radians(f32 degrees) {
    return degrees * (PI32 / 180.0f);
}

INLINE f32 ease_in_expo(f32 t) {
    return t <= 0.0f ? 0.0f : powf(2.0f, 10.0f * t - 10.0f);
}

typedef union {
    struct { f32 x, y; };
    struct { f32 u, v; };
    f32 nums[2];
} Vec2;

INLINE Vec2 print2(Vec2 v) {
    for (int i = 0; i < 2; i++)
        printf("%f ", v.nums[i]);
    printf("\n");
    return v;
}

INLINE Vec2 vec2(f32 x, f32 y) {
    return (Vec2){ .nums = { x, y } };
}

INLINE Vec2 vec2f(f32 f) {
    return vec2(f, f);
}

INLINE Vec2 vec2_x(void) {
    return vec2(1.0, 0.0);
}

INLINE Vec2 vec2_y(void) {
    return vec2(0.0, 1.0);
}

INLINE Vec2 add2(Vec2 a, Vec2 b) {
    return vec2(a.x + b.x,
                a.y + b.y);
}

INLINE Vec2 add2f(Vec2 a, f32 f) {
    return vec2(a.x + f,
                a.y + f);
}

INLINE Vec2 sub2(Vec2 a, Vec2 b) {
    return vec2(a.x - b.x,
                a.y - b.y);
}

INLINE Vec2 sub2f(Vec2 a, f32 f) {
    return vec2(a.x - f,
                a.y - f);
}

INLINE Vec2 div2(Vec2 a, Vec2 b) {
    return vec2(a.x / b.x,
                a.y / b.y);
}

INLINE Vec2 div2f(Vec2 a, f32 f) {
    return vec2(a.x / f,
                a.y / f);
}

INLINE Vec2 mul2(Vec2 a, Vec2 b) {
    return vec2(a.x * b.x,
                a.y * b.y);
}

INLINE Vec2 mul2f(Vec2 a, f32 f) {
    return vec2(a.x * f,
                a.y * f);
}

INLINE f32 dot2(Vec2 a, Vec2 b) {
    return a.x*b.x + a.y*b.y;
}

INLINE Vec2 lerp2(Vec2 a, f32 t, Vec2 b) {
    return add2(mul2f(a, 1.0f - t), mul2f(b, t));
}

INLINE f32 mag2(Vec2 a) {
    return sqrtf(dot2(a, a));
}

INLINE f32 magmag2(Vec2 a) {
    return dot2(a, a);
}

INLINE Vec2 norm2(Vec2 a) {
    return div2f(a, mag2(a));
}

INLINE bool eq2(Vec2 a, Vec2 b) {
    return a.x == b.x &&
           a.y == b.y;
}

INLINE Vec2 rand2(void) {
    f32 theta = randf() * PI32 * 2.0f;
    return vec2(cosf(theta),
                sinf(theta));
}

INLINE Vec2 perp2(Vec2 a) {
    return vec2(a.y, -a.x);
}

/* see https://github.com/SRombauts/SimplexNoise/blob/master/src/SimplexNoise.cpp#L45 */
INLINE i32 fastfloor(f32 fp) {
    i32 i = (i32) (fp);
    return (fp < i) ? (i - 1) : (i);
}

static const uint8_t perm[256] = {
    151, 160, 137, 91, 90, 15,
    131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
    190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
    88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
    77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
    102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
    135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
    5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
    223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
    129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
    251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
    49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};

INLINE u8 hash(i32 i) {
    return perm[(u8) i];
}

static f32 grad(i32 hash, f32 x, f32 y) {
    const i32 h = hash & 0x3F;  // Convert low 3 bits of hash code
    const f32 u = h < 4 ? x : y;  // into 8 simple gradient directions,
    const f32 v = h < 4 ? y : x;
    // and compute the dot product with (x,y).
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0f * v : 2.0f * v);
}

INLINE f32 simplex2(Vec2 pnt) {
    f32 x = pnt.x, y = pnt.y, n0, n1, n2;   // Noise contributions from the three corners

    // Skewing/Unskewing factors for 2D
    static const f32 F2 = 0.366025403f;  // F2 = (sqrt(3) - 1) / 2
    static const f32 G2 = 0.211324865f;  // G2 = (3 - sqrt(3)) / 6   = F2 / (1 + 2 * K)

    // Skew the input space to determine which simplex cell we're in
    const f32 s = (x + y) * F2;  // Hairy factor for 2D
    const f32 xs = x + s;
    const f32 ys = y + s;
    const i32 i = fastfloor(xs);
    const i32 j = fastfloor(ys);

    // Unskew the cell origin back to (x,y) space
    const f32 t = (f32) (i + j) * G2;
    const f32 X0 = i - t;
    const f32 Y0 = j - t;
    const f32 x0 = x - X0;  // The x,y distances from the cell origin
    const f32 y0 = y - Y0;

    // For the 2D case, the simplex shape is an equilateral triangle.
    // Determine which simplex we are in.
    i32 i1, j1;  // Offsets for second (middle) corner of simplex in (i,j) coords
    if (x0 > y0) {   // lower triangle, XY order: (0,0)->(1,0)->(1,1)
        i1 = 1;
        j1 = 0;
    } else {   // upper triangle, YX order: (0,0)->(0,1)->(1,1)
        i1 = 0;
        j1 = 1;
    }

    // A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
    // a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
    // c = (3-sqrt(3))/6

    const f32 x1 = x0 - i1 + G2;            // Offsets for middle corner in (x,y) unskewed coords
    const f32 y1 = y0 - j1 + G2;
    const f32 x2 = x0 - 1.0f + 2.0f * G2;   // Offsets for last corner in (x,y) unskewed coords
    const f32 y2 = y0 - 1.0f + 2.0f * G2;

    // Work out the hashed gradient indices of the three simplex corners
    const int gi0 = hash(i + hash(j));
    const int gi1 = hash(i + i1 + hash(j + j1));
    const int gi2 = hash(i + 1 + hash(j + 1));

    // Calculate the contribution from the first corner
    f32 t0 = 0.5f - x0*x0 - y0*y0;
    if (t0 < 0.0f) {
        n0 = 0.0f;
    } else {
        t0 *= t0;
        n0 = t0 * t0 * grad(gi0, x0, y0);
    }

    // Calculate the contribution from the second corner
    f32 t1 = 0.5f - x1*x1 - y1*y1;
    if (t1 < 0.0f) {
        n1 = 0.0f;
    } else {
        t1 *= t1;
        n1 = t1 * t1 * grad(gi1, x1, y1);
    }

    // Calculate the contribution from the third corner
    f32 t2 = 0.5f - x2*x2 - y2*y2;
    if (t2 < 0.0f) {
        n2 = 0.0f;
    } else {
        t2 *= t2;
        n2 = t2 * t2 * grad(gi2, x2, y2);
    }

    // Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the interval [-1,1].
    return 45.23065f * (n0 + n1 + n2);
}

INLINE f32 line_dist2(Vec2 start, Vec2 end, Vec2 pt) {
    Vec2 line_dir = sub2(end, start);
    Vec2 perp_dir = perp2(line_dir);
    Vec2 to_start_dir = sub2(start, pt);
    return fabsf(dot2(norm2(perp_dir), to_start_dir));
}

typedef union {
    struct { Vec2 xy; f32 _z; };
    struct { f32 x, y, z; };
    f32 nums[3];
} Vec3;

INLINE Vec3 print3(Vec3 v) {
    for (int i = 0; i < 3; i++)
        printf("%f ", v.nums[i]);
    printf("\n");
    return v;
}

INLINE Vec3 vec3(f32 x, f32 y, f32 z) {
    return (Vec3){ .nums = { x, y, z } };
}

INLINE Vec3 vec3f(f32 f) {
    return vec3(f, f, f);
}

INLINE Vec3 vec3_x(void) {
    return vec3(1.0, 0.0, 0.0);
}

INLINE Vec3 vec3_y(void) {
    return vec3(0.0, 1.0, 0.0);
}

INLINE Vec3 vec3_z(void) {
    return vec3(0.0, 0.0, 1.0);
}

INLINE Vec3 add3(Vec3 a, Vec3 b) {
    return vec3(a.x + b.x,
                a.y + b.y,
                a.z + b.z);
}

INLINE Vec3 sub3(Vec3 a, Vec3 b) {
    return vec3(a.x - b.x,
                a.y - b.y,
                a.z - b.z);
}

INLINE Vec3 div3(Vec3 a, Vec3 b) {
    return vec3(a.x / b.x,
                a.y / b.y,
                a.z / b.z);
}

INLINE Vec3 div3f(Vec3 a, f32 f) {
    return vec3(a.x / f,
                a.y / f,
                a.z / f);
}

INLINE Vec3 mul3(Vec3 a, Vec3 b) {
    return vec3(a.x * b.x,
                a.y * b.y,
                a.z * b.z);
}

INLINE Vec3 mul3f(Vec3 a, f32 f) {
    return vec3(a.x * f,
                a.y * f,
                a.z * f);
}

INLINE f32 dot3(Vec3 a, Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

INLINE Vec3 lerp3(Vec3 a, f32 t, Vec3 b) {
    return add3(mul3f(a, 1.0f - t), mul3f(b, t));
}

INLINE Vec3 quad_bez3(Vec3 a, Vec3 b, Vec3 c, f32 t) {
    return lerp3(lerp3(a, t, b), t, lerp3(b, t, c));
}

INLINE f32 mag3(Vec3 a) {
    return sqrtf(dot3(a, a));
}

INLINE f32 magmag3(Vec3 a) {
    return dot3(a, a);
}

INLINE Vec3 norm3(Vec3 a) {
    return div3f(a, mag3(a));
}

INLINE bool eq3(Vec3 a, Vec3 b) {
    return a.x == b.x &&
           a.y == b.y &&
           a.z == b.z;
}

/* source: https://math.stackexchange.com/a/44691 */
INLINE Vec3 rand3(void) {
    f32 theta = randf() * PI32 * 2.0f,
            z = 1.0f - randf() * 2.0f,
           cz = sqrtf(1.0f - powf(z, 2.0f));

    return vec3(cz * cosf(theta),
                cz * sinf(theta),
                z               );
}

INLINE Vec3 cross3(Vec3 a, Vec3 b) {
    return vec3((a.y * b.z) - (a.z * b.y),
                (a.z * b.x) - (a.x * b.z),
                (a.x * b.y) - (a.y * b.x));
}

INLINE Vec3 ortho3(Vec3 v) {
    f32 x = fabsf(v.x),
        y = fabsf(v.y),
        z = fabsf(v.z);

    Vec3 other;
    if (x < y) {
        if (x < z) { other = vec3_x(); }
        else       { other = vec3_z(); }
    } else {
        if (y < z) { other = vec3_y(); }
        else       { other = vec3_z(); }
    }
    return norm3(cross3(v, other));
}

INLINE f32 line_dist3(Vec3 start, Vec3 end, Vec3 pt) {
    Vec3 ab = sub3(end, start);
    Vec3 ac = sub3(pt, start);
    f32 area = mag3(cross3(ab, ac));
    return area / mag3(ab);
}

INLINE Vec3 project_plane_vec3(Vec3 n, Vec3 bd) {
    return sub3(bd, mul3f(n, dot3(bd, n)));
}

typedef struct {
    Vec3 origin, vector;
} Ray;

INLINE Vec3 ray_hit_plane(Ray ray, Ray plane) {
    f32 d = dot3(sub3(plane.origin, ray.origin), plane.vector)
               / dot3(ray.vector, plane.vector);
    return add3(ray.origin, mul3f(ray.vector, d));
}

INLINE bool ray_in_plane_circle(Ray ray, Ray plane, f32 radius2) {
    Vec3 hit = ray_hit_plane(ray, plane);
    return magmag3(sub3(plane.origin, hit)) < radius2;
}

INLINE bool ray_in_plane_rect(Ray ray, Ray plane, Vec3 tl, Vec3 br) {
    f32 minx = fminf(tl.x, br.x),
        miny = fminf(tl.y, br.y),
        minz = fminf(tl.z, br.z),
        maxx = fmaxf(tl.x, br.x),
        maxy = fmaxf(tl.y, br.y),
        maxz = fmaxf(tl.z, br.z);
    Vec3 hit = ray_hit_plane(ray, plane);
    return (minx < hit.x && maxx > hit.x) &&
           (miny < hit.y && maxy > hit.y) &&
           (minz < hit.z && maxz > hit.z);
}
static Vec3 icosa_verts[12];
const u16 icosa_indices[][3] = {
     { 0, 11,  5}, { 0,  5,  1}, { 0,  1,  7},
     { 0,  7, 10}, { 0, 10, 11}, { 1,  5,  9},
     { 5, 11,  4}, {11, 10,  2}, {10,  7,  6},
     { 7,  1,  8}, { 3,  9,  4}, { 3,  4,  2},
     { 3,  2,  6}, { 3,  6,  8}, { 3,  8,  9},
     { 4,  9,  5}, { 2,  4, 11}, { 6,  2, 10},
     { 8,  6,  7}, { 9,  8,  1}, 
};

#define SPHERE_POINTS_LEN(detail) (((1 << detail) + 1) * ((1 << detail) + 2) * LEN(icosa_indices) / 2)
size_t fill_sphere_points(size_t detail, Vec3 *out) {
    Vec3 *start_out = out;
    int size = 1 << detail;
    for (int tri = 0; tri < LEN(icosa_indices); tri++) {
        Vec3 a = icosa_verts[icosa_indices[tri][0]],
             b = icosa_verts[icosa_indices[tri][1]],
             c = icosa_verts[icosa_indices[tri][2]];
        for (int x = 0; x <= size; x++) {
            Vec3 ay = lerp3(a, (f32) x / (f32) size, c),
                 by = lerp3(b, (f32) x / (f32) size, c);
            int rows = size - x;
            for (int y = 0; y <= rows; y++) {
                Vec3 v = (y == 0 && x == size) ? ay : lerp3(ay, (f32) y / (f32) rows, by);
                v = norm3(v);

                for (int i = 0; i < (out - start_out); i++)
                    if (mag3(sub3(start_out[i], v)) < 0.01)
                        goto SKIP;
                *out++ = v;
                SKIP:;
            }
        }
    }
    return (out - start_out);
}

typedef union {
    struct {
        union {
            Vec3 xyz;
            struct { f32 x, y, z; };
        };

        f32 w;
    };

    f32 nums[4];
} Vec4;

INLINE Vec4 print4(Vec4 v) {
    for (int i = 0; i < 4; i++)
        printf("%f ", v.nums[i]);
    printf("\n");
    return v;
}

INLINE Vec4 vec4(f32 x, f32 y, f32 z, f32 w) {
    return (Vec4){ .nums = { x, y, z, w } };
}

INLINE Vec4 vec4f(f32 f) {
    return vec4(f, f, f, f);
}

INLINE Vec4 vec4_x(void) {
    return vec4(1.0, 0.0, 0.0, 0.0);
}

INLINE Vec4 vec4_y(void) {
    return vec4(0.0, 1.0, 0.0, 0.0);
}

INLINE Vec4 vec4_z(void) {
    return vec4(0.0, 0.0, 1.0, 0.0);
}

INLINE Vec4 vec4_w(void) {
    return vec4(0.0, 0.0, 0.0, 1.0);
}

INLINE Vec4 add4(Vec4 a, Vec4 b) {
    return vec4(a.x + b.x,
                a.y + b.y,
                a.z + b.z,
                a.w + b.w);
}

INLINE Vec4 sub4(Vec4 a, Vec4 b) {
    return vec4(a.x - b.x,
                a.y - b.y,
                a.z - b.z,
                a.w - b.w);
}

INLINE Vec4 div4(Vec4 a, Vec4 b) {
    return vec4(a.x / b.x,
                a.y / b.y,
                a.z / b.z,
                a.w / b.w);
}

INLINE Vec4 div4f(Vec4 a, f32 f) {
    return vec4(a.x / f,
                a.y / f,
                a.z / f,
                a.w / f);
}

INLINE Vec4 mul4(Vec4 a, Vec4 b) {
    return vec4(a.x * b.x,
                a.y * b.y,
                a.z * b.z,
                a.w * b.w);
}

INLINE Vec4 mul4f(Vec4 a, f32 f) {
    return vec4(a.x * f,
                a.y * f,
                a.z * f,
                a.w * f);
}

INLINE f32 dot4(Vec4 a, Vec4 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
}

INLINE Vec4 lerp4(Vec4 a, f32 t, Vec4 b) {
    return add4(mul4f(a, 1.0f - t), mul4f(b, t));
}

INLINE f32 mag4(Vec4 a) {
    return sqrtf(dot4(a, a));
}

INLINE f32 magmag4(Vec4 a) {
    return dot4(a, a);
}

INLINE Vec4 norm4(Vec4 a) {
    return div4f(a, mag4(a));
}

INLINE bool eq4(Vec4 a, Vec4 b) {
    return a.x == b.x &&
           a.y == b.y &&
           a.z == b.z &&
           a.w == b.w;
}

typedef union {
    struct {
        union {
            Vec3 xyz;
            struct { f32 x, y, z; };
        };

        f32 w;
    };

    Vec4 xyzw;
    f32 nums[4];
} Quat;

INLINE Quat quat(f32 x, f32 y, f32 z, f32 w) {
    return (Quat){ .nums = { x, y, z, w } };
}

INLINE Quat vec4Q(Vec4 v) {
    return quat(v.x, v.y, v.z, v.w);
}

INLINE Vec4 quat4(Quat q) {
    return vec4(q.x, q.y, q.z, q.w);
}

INLINE Quat identQ(void) {
    return quat(0.0, 0.0, 0.0, 1.0);
}

INLINE Quat printQ(Quat q) {
    for (int i = 0; i < 4; i++)
        printf("%f ", q.nums[i]);
    printf("\n");
    return q;
}

INLINE Quat mulQ(Quat a, Quat b) {
    return quat(( a.x * b.w) + (a.y * b.z) - (a.z * b.y) + (a.w * b.x),
                (-a.x * b.z) + (a.y * b.w) + (a.z * b.x) + (a.w * b.y),
                ( a.x * b.y) - (a.y * b.x) + (a.z * b.w) + (a.w * b.z),
                (-a.x * b.x) - (a.y * b.y) - (a.z * b.z) + (a.w * b.w));
}

INLINE Vec3 mulQ3(Quat q, Vec3 v) {
    Vec3 t = mul3f(cross3(q.xyz, v), 2.0);
    return add3(add3(v, mul3f(t, q.w)), cross3(q.xyz, t));
}

Quat slerpQ(Quat l, f32 time, Quat r) {
    if (eq4(l.xyzw, r.xyzw))
        return l;

    f32 cos_theta = dot4(l.xyzw, r.xyzw),
        angle = acosf(cos_theta);

    f32 s1 = sinf((1.0f - time) * angle),
        s2 = sinf(time * angle),
        is = 1.0f / sinf(angle);

    Vec4 ql = mul4f(l.xyzw, s1),
         qr = mul4f(r.xyzw, s2);

    return vec4Q(mul4f(add4(ql, qr), is));
}

INLINE Quat axis_angleQ(Vec3 axis, f32 angle) {
    Vec3 axis_norm = norm3(axis);
    f32 rot_sin = sinf(angle / 2.0f);

    Quat res;
    res.xyz = mul3f(axis_norm, rot_sin);
    res.w = cosf(angle / 2.0f);
    return res;
}

INLINE Quat yprQ(f32 yaw, f32 pitch, f32 roll) {
    f32 y0 = sinf(yaw   * 0.5f), w0 = cosf(yaw   * 0.5f),
        x1 = sinf(pitch * 0.5f), w1 = cosf(pitch * 0.5f),
        z2 = sinf(roll  * 0.5f), w2 = cosf(roll  * 0.5f);

    f32 x3 = w0 * x1,
        y3 = y0 * w1,
        z3 = -y0 * x1,
        w3 = w0 * w1;

    f32 x4 = x3 * w2 + y3 * z2,
        y4 = -x3 * z2 + y3 * w2,
        z4 = w3 * z2 + z3 * w2,
        w4 = w3 * w2 - z3 * z2;

    return quat(x4, y4, z4, w4);
}

typedef union {
    struct { Vec3 x, y, z; };
    Vec3 cols[3];
    f32 nums[3][3];
} Mat3;

/* Multiplies two Mat3s, returning a new one */
INLINE Mat3 mul3x3(Mat3 a, Mat3 b) {
    Mat3 out;
    i8 k, r, c;
    for (c = 0; c < 3; ++c)
        for (r = 0; r < 3; ++r) {
            out.nums[c][r] = 0.0f;
            for (k = 0; k < 3; ++k)
                out.nums[c][r] += a.nums[k][r] * b.nums[c][k];
        }
    return out;
}

/* Rotates the given rotation matrix so that the Y basis vector
    points to `new_y`. The X basis vector is orthogonalized with
    the new Y and old Z basis vector projected onto the Y plane.
*/
INLINE void rotated_up_indefinite_basis(Mat3 *rot, Vec3 up) {
    rot->cols[1] = up;
    rot->cols[2] = norm3(project_plane_vec3(up, rot->cols[2]));
    rot->cols[0] = cross3(rot->cols[1], rot->cols[2]);
}

INLINE Mat3 ortho_bases3x3(Vec3 v) {
    Vec3     n = norm3(v),
           tan = ortho3(n),
         bitan = cross3(n, tan);
    return (Mat3) { .cols = { bitan, n, tan } };
}

INLINE Mat3 transpose3x3(Mat3 a) {
    Mat3 res;
    for(int c = 0; c < 3; ++c)
        for(int r = 0; r < 3; ++r)
            res.nums[r][c] = a.nums[c][r];
    return res;
}

INLINE Mat3 invert3x3(Mat3 a) {
    Vec3 tmp0 = cross3(a.y, a.z),
         tmp1 = cross3(a.z, a.x),
         tmp2 = cross3(a.x, a.y);
    f32     det = dot3(a.z, tmp2),
        inv_det = 1.0f / det;
    return transpose3x3((Mat3) {
        .x = mul3f(tmp0, inv_det),
        .y = mul3f(tmp1, inv_det),
        .z = mul3f(tmp2, inv_det),
    });
}

typedef union {
    struct { Vec4 x, y, z, w; };
    Vec4 cols[4];
    f32 nums[4][4];
} Mat4;

INLINE Mat4 mul4x4(Mat4 a, Mat4 b) {
    Mat4 out = { .nums = { 0 } };
    i8 k, r, c;
    for (c = 0; c < 4; ++c)
        for (r = 0; r < 4; ++r) {
            out.nums[c][r] = 0.0f;
            for (k = 0; k < 4; ++k)
                out.nums[c][r] += a.nums[k][r] * b.nums[c][k];
        }
    return out;
}

INLINE Vec4 mul4x44(Mat4 m, Vec4 v) {
    Vec4 res;
    for(int x = 0; x < 4; ++x) {
        f32 sum = 0;
        for(int y = 0; y < 4; ++y)
            sum += m.nums[y][x] * v.nums[y];

        res.nums[x] = sum;
    }
    return res;
}

INLINE Mat4 mat3_translation4x4(Mat3 basis_vectors, Vec3 pos) {
    Mat4 res;
    res.nums[0][0] =  basis_vectors.x.x;
    res.nums[0][1] =  basis_vectors.y.x;
    res.nums[0][2] = -basis_vectors.z.x;
    res.nums[0][3] =  0.0;

    res.nums[1][0] =  basis_vectors.x.y;
    res.nums[1][1] =  basis_vectors.y.y;
    res.nums[1][2] = -basis_vectors.z.y;
    res.nums[1][3] =  0.0;

    res.nums[2][0] =  basis_vectors.x.z;
    res.nums[2][1] =  basis_vectors.y.z;
    res.nums[2][2] = -basis_vectors.z.z;
    res.nums[2][3] =  0.0;

    res.nums[3][0] = -dot3(basis_vectors.x, pos);
    res.nums[3][1] = -dot3(basis_vectors.y, pos);
    res.nums[3][2] =  dot3(basis_vectors.z, pos);
    res.nums[3][3] =  1.0;
    return res;
}

INLINE Mat4 mat34x4(Mat3 bases) {
    Mat4 res;
    res.nums[0][0] = bases.nums[0][0];
    res.nums[0][1] = bases.nums[0][1];
    res.nums[0][2] = bases.nums[0][2];
    res.nums[0][3] = 0.0;

    res.nums[1][0] = bases.nums[1][0];
    res.nums[1][1] = bases.nums[1][1];
    res.nums[1][2] = bases.nums[1][2];
    res.nums[1][3] = 0.0;

    res.nums[2][0] = bases.nums[2][0];
    res.nums[2][1] = bases.nums[2][1];
    res.nums[2][2] = bases.nums[2][2];
    res.nums[2][3] = 0.0;

    res.nums[3][0] = 0.0;
    res.nums[3][1] = 0.0;
    res.nums[3][2] = 0.0;
    res.nums[3][3] = 1.0;
    return res;
}

INLINE Vec3 mat3_rel3(Mat3 bases, Vec3 v) {
    return vec3(dot3(bases.x, v),
                dot3(bases.y, v),
                dot3(bases.z, v));
}

INLINE Mat4 mat4x4(void) {
    return (Mat4){0};
}

INLINE Mat4 diag4x4(f32 f) {
    Mat4 res = mat4x4();
    res.nums[0][0] = f;
    res.nums[1][1] = f;
    res.nums[2][2] = f;
    res.nums[3][3] = f;
    return res;
}

INLINE Mat4 ident4x4(void) {
    return diag4x4(1.0);
}

INLINE Vec3 mat4_scale3(Mat4 mat) {
    return vec3(
        ((mat.x.x < 0.0f) ? -1.0f : 1.0f) * mag3(vec3(mat.nums[0][0], mat.nums[1][0], mat.nums[2][0])),
        ((mat.y.y < 0.0f) ? -1.0f : 1.0f) * mag3(vec3(mat.nums[0][1], mat.nums[1][1], mat.nums[2][1])),
        ((mat.x.x < 0.0f) ? -1.0f : 1.0f) * mag3(vec3(mat.nums[0][2], mat.nums[1][2], mat.nums[2][2]))
    );
}

INLINE Mat4 scale4x4(Vec3 scale) {
    Mat4 res = ident4x4();
    res.nums[0][0] = scale.x;
    res.nums[1][1] = scale.y;
    res.nums[2][2] = scale.z;
    return res;
}

INLINE Mat4 translate4x4(Vec3 pos) {
    Mat4 res = diag4x4(1.0);
    res.nums[3][0] = pos.x;
    res.nums[3][1] = pos.y;
    res.nums[3][2] = pos.z;
    return res;
}

INLINE Mat4 pivot4x4(Mat4 m, Vec3 pivot) {
    return mul4x4(mul4x4(translate4x4(pivot), m),
                  translate4x4(mul3f(pivot, -1.0f)));
}

INLINE Mat4 no_translate4x4(Mat4 m) {
    m.nums[3][0] = 0.0;
    m.nums[3][1] = 0.0;
    m.nums[3][2] = 0.0;
    return m;
}

INLINE Quat mat4Q(Mat4 m) {
    f32 t;
    Quat q;

    if (m.nums[2][2] < 0.0f) {
        if (m.nums[0][0] > m.nums[1][1]) {
            t = 1 + m.nums[0][0] - m.nums[1][1] - m.nums[2][2];
            q = quat(
                t,
                m.nums[0][1] + m.nums[1][0],
                m.nums[2][0] + m.nums[0][2],
                m.nums[1][2] - m.nums[2][1]
            );
        } else {
            t = 1 - m.nums[0][0] + m.nums[1][1] - m.nums[2][2];
            q = quat(
                m.nums[0][1] + m.nums[1][0],
                t,
                m.nums[1][2] + m.nums[2][1],
                m.nums[2][0] - m.nums[0][2]
            );
        }
    } else {
        if (m.nums[0][0] < -m.nums[1][1]) {
            t = 1 - m.nums[0][0] - m.nums[1][1] + m.nums[2][2];
            q = quat(
                m.nums[2][0] + m.nums[0][2],
                m.nums[1][2] + m.nums[2][1],
                t,
                m.nums[0][1] - m.nums[1][0]
            );
        } else {
            t = 1 + m.nums[0][0] + m.nums[1][1] + m.nums[2][2];
            q = quat(
                m.nums[1][2] - m.nums[2][1],
                m.nums[2][0] - m.nums[0][2],
                m.nums[0][1] - m.nums[1][0],
                t
            );
        }
    }

    q = vec4Q(mul4f(q.xyzw, 0.5f / sqrtf(t)));

    return q;
}

INLINE Mat4 quat4x4(Quat q) {
    Mat4 res = diag4x4(1.0);
    Quat norm = vec4Q(norm4(q.xyzw));

    f32 xx = norm.x * norm.x,
        yy = norm.y * norm.y,
        zz = norm.z * norm.z,
        xy = norm.x * norm.y,
        xz = norm.x * norm.z,
        yz = norm.y * norm.z,
        wx = norm.w * norm.x,
        wy = norm.w * norm.y,
        wz = norm.w * norm.z;

    res.nums[0][0] = 1.0f - 2.0f * (yy + zz);
    res.nums[0][1] = 2.0f * (xy + wz);
    res.nums[0][2] = 2.0f * (xz - wy);
    res.nums[0][3] = 0.0f;

    res.nums[1][0] = 2.0f * (xy - wz);
    res.nums[1][1] = 1.0f - 2.0f * (xx + zz);
    res.nums[1][2] = 2.0f * (yz + wx);
    res.nums[1][3] = 0.0f;

    res.nums[2][0] = 2.0f * (xz + wy);
    res.nums[2][1] = 2.0f * (yz - wx);
    res.nums[2][2] = 1.0f - 2.0f * (xx + yy);
    res.nums[2][3] = 0.0f;

    res.nums[3][0] = 0.0f;
    res.nums[3][1] = 0.0f;
    res.nums[3][2] = 0.0f;
    res.nums[3][3] = 1.0f;
    return res;
}

INLINE Mat4 axis_angle4x4(Vec3 axis, f32 angle) {
    return quat4x4(axis_angleQ(axis, angle));
}

INLINE Mat4 ypr4x4(f32 yaw, f32 pitch, f32 roll) {
    return quat4x4(yprQ(yaw, pitch, roll));
}

INLINE Mat4 print4x4(Mat4 a) {
    for(int c = 0; c < 4; ++c) {
        for(int r = 0; r < 4; ++r) {
            printf("%f ", a.nums[c][r]);
        }
        printf("\n");
    }
    return a;
}

INLINE Mat4 transpose4x4(Mat4 a) {
    Mat4 res;
    for(int c = 0; c < 4; ++c)
        for(int r = 0; r < 4; ++r)
            res.nums[r][c] = a.nums[c][r];
    return res;
}

INLINE Mat4 invert4x4(Mat4 a) {
    f32 s[6], c[6];
    s[0] = a.nums[0][0]*a.nums[1][1] - a.nums[1][0]*a.nums[0][1];
    s[1] = a.nums[0][0]*a.nums[1][2] - a.nums[1][0]*a.nums[0][2];
    s[2] = a.nums[0][0]*a.nums[1][3] - a.nums[1][0]*a.nums[0][3];
    s[3] = a.nums[0][1]*a.nums[1][2] - a.nums[1][1]*a.nums[0][2];
    s[4] = a.nums[0][1]*a.nums[1][3] - a.nums[1][1]*a.nums[0][3];
    s[5] = a.nums[0][2]*a.nums[1][3] - a.nums[1][2]*a.nums[0][3];

    c[0] = a.nums[2][0]*a.nums[3][1] - a.nums[3][0]*a.nums[2][1];
    c[1] = a.nums[2][0]*a.nums[3][2] - a.nums[3][0]*a.nums[2][2];
    c[2] = a.nums[2][0]*a.nums[3][3] - a.nums[3][0]*a.nums[2][3];
    c[3] = a.nums[2][1]*a.nums[3][2] - a.nums[3][1]*a.nums[2][2];
    c[4] = a.nums[2][1]*a.nums[3][3] - a.nums[3][1]*a.nums[2][3];
    c[5] = a.nums[2][2]*a.nums[3][3] - a.nums[3][2]*a.nums[2][3];
    
    /* Assumes it is invertible */
    f32 idet = 1.0f/( s[0]*c[5]-s[1]*c[4]+s[2]*c[3]+s[3]*c[2]-s[4]*c[1]+s[5]*c[0] );
    
    Mat4 res;
    res.nums[0][0] = ( a.nums[1][1] * c[5] - a.nums[1][2] * c[4] + a.nums[1][3] * c[3]) * idet;
    res.nums[0][1] = (-a.nums[0][1] * c[5] + a.nums[0][2] * c[4] - a.nums[0][3] * c[3]) * idet;
    res.nums[0][2] = ( a.nums[3][1] * s[5] - a.nums[3][2] * s[4] + a.nums[3][3] * s[3]) * idet;
    res.nums[0][3] = (-a.nums[2][1] * s[5] + a.nums[2][2] * s[4] - a.nums[2][3] * s[3]) * idet;

    res.nums[1][0] = (-a.nums[1][0] * c[5] + a.nums[1][2] * c[2] - a.nums[1][3] * c[1]) * idet;
    res.nums[1][1] = ( a.nums[0][0] * c[5] - a.nums[0][2] * c[2] + a.nums[0][3] * c[1]) * idet;
    res.nums[1][2] = (-a.nums[3][0] * s[5] + a.nums[3][2] * s[2] - a.nums[3][3] * s[1]) * idet;
    res.nums[1][3] = ( a.nums[2][0] * s[5] - a.nums[2][2] * s[2] + a.nums[2][3] * s[1]) * idet;

    res.nums[2][0] = ( a.nums[1][0] * c[4] - a.nums[1][1] * c[2] + a.nums[1][3] * c[0]) * idet;
    res.nums[2][1] = (-a.nums[0][0] * c[4] + a.nums[0][1] * c[2] - a.nums[0][3] * c[0]) * idet;
    res.nums[2][2] = ( a.nums[3][0] * s[4] - a.nums[3][1] * s[2] + a.nums[3][3] * s[0]) * idet;
    res.nums[2][3] = (-a.nums[2][0] * s[4] + a.nums[2][1] * s[2] - a.nums[2][3] * s[0]) * idet;

    res.nums[3][0] = (-a.nums[1][0] * c[3] + a.nums[1][1] * c[1] - a.nums[1][2] * c[0]) * idet;
    res.nums[3][1] = ( a.nums[0][0] * c[3] - a.nums[0][1] * c[1] + a.nums[0][2] * c[0]) * idet;
    res.nums[3][2] = (-a.nums[3][0] * s[3] + a.nums[3][1] * s[1] - a.nums[3][2] * s[0]) * idet;
    res.nums[3][3] = ( a.nums[2][0] * s[3] - a.nums[2][1] * s[1] + a.nums[2][2] * s[0]) * idet;
    return res;
}

// See https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPerspective.xml
INLINE Mat4 perspective4x4(f32 fov, f32 aspect, f32 near, f32 far) {
    f32 cotangent = 1.0f / tanf(fov * (PI32 / 360.0f));

    Mat4 res = mat4x4();
    res.nums[0][0] = cotangent / aspect;
    res.nums[1][1] = cotangent;
    res.nums[2][3] = -1.0f;
    res.nums[2][2] = (near + far) / (near - far);
    res.nums[3][2] = (2.0f * near * far) / (near - far);
    res.nums[3][3] = 0.0f;
    return res;
}

INLINE Mat4 ortho4x4(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far) {
    Mat4 res = ident4x4();

    res.nums[0][0] = 2.0f / (right - left);
    res.nums[1][1] = 2.0f / (top - bottom);
    res.nums[2][2] = 2.0f / (near - far);
    res.nums[3][3] = 1.0f;

    res.nums[3][0] = (left + right) / (left - right);
    res.nums[3][1] = (bottom + top) / (bottom - top);
    res.nums[3][2] = (far + near) / (near - far);

    return res;
}

INLINE Mat4 look_at4x4(Vec3 eye, Vec3 center, Vec3 up) {
    Mat4 res;

    Vec3 f = norm3(sub3(center, eye));
    Vec3 s = norm3(cross3(f, up));
    Vec3 u = cross3(s, f);

    res.nums[0][0] = s.x;
    res.nums[0][1] = u.x;
    res.nums[0][2] = -f.x;
    res.nums[0][3] = 0.0f;

    res.nums[1][0] = s.y;
    res.nums[1][1] = u.y;
    res.nums[1][2] = -f.y;
    res.nums[1][3] = 0.0f;

    res.nums[2][0] = s.z;
    res.nums[2][1] = u.z;
    res.nums[2][2] = -f.z;
    res.nums[2][3] = 0.0f;

    res.nums[3][0] = -dot3(s, eye);
    res.nums[3][1] = -dot3(u, eye);
    res.nums[3][2] = dot3(f, eye);
    res.nums[3][3] = 1.0f;

    return res;
}

/* This is super inefficient and innaccurate,
    it could be made much more efficient by multiplying the ray into
    local space instead of bringing key points into world space,
    and by using line-point distance for checks against body of the cylinder.
    this works well enough for selecting parts of the tree, though,
    so for now it stays.

    The test against the center disc of the cylinder could be done
    instead for the top and bottom discs for a higher fidelity check.
*/
INLINE bool ray_hit_cylinder(Ray ray, Mat4 mat, Vec3* hit, f32 scale) {
    Vec3     bottom = mat.w.xyz,
                top = mul4x44(mat, vec4(0.0f, 1.0f, 0.0f, 1.0f)).xyz,
             center = div3f(add3(bottom, top), 2.0f),
            b_side0 = mul4x44(mat, vec4(1.0f, 0.0f, 0.0f, 1.0f)).xyz,
         side0_norm = norm3(sub3(bottom, b_side0)),
           top_norm = norm3(sub3(bottom, top)),
         side1_norm = cross3(side0_norm, top_norm);
    Mat4 inv = invert4x4(mat);
    Ray planes[] = {
        { .origin = bottom, .vector = side0_norm },
        { .origin = bottom, .vector = side1_norm },
        { .origin = center, .vector = top_norm },
    };

    for (int i = 0; i < LEN(planes); i++) {
        Vec3     p = ray_hit_plane(ray, planes[i]),
             local = mul4x44(inv, (Vec4) { .xyz = p, .w = 1.0f }).xyz;
        f32 dist = mag2(vec2(local.x, local.z));
        if (local.y > 0.0 && local.y < 1.0 && dist < scale) {
            *hit = p;
            return true;
        }
    }
    return false;
}

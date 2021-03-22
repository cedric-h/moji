#ifdef _MSC_VER
/* non-constant aggregate initializers */
#pragma warning(disable:4204)
/* anonymous struct/unions */
#pragma warning(disable:4201)
/* padding bytes are added */
#pragma warning(disable:4324)
#pragma warning(disable:4820)
/* unknown preprocessor macros */
#pragma warning(disable:4668)
/* assignment within conditional expression */
#pragma warning(disable:4706)
/* initialization using address of automatic variable */
#pragma warning(disable:4221)
#endif

#define LEN(arr) ((int) (sizeof arr / sizeof arr[0]))

#include <string.h>
#include "math.h"
#include "mapgen.h"

typedef struct {
    uint8_t *data;
    uint16_t size;
} Pack;

typedef struct {
    char *str;
    uint8_t len;
} String;
#define unspool(s) (int)(s).len, (s).str
#define string_lit(s) ((String) { str: (s), len: sizeof(s)-1 })
INLINE String string_from_nulterm(char *str) {
    return (String) { .str = str, .len = (uint8_t) strlen(str) };
}

INLINE bool string_eq(String *a, String *b) {
    if (a->len == b->len) {
        for (int i = 0; i < a->len; i++)
            if (a->str[i] != b->str[i])
                return false;
    } else return false;
    return true;
}

int16_t encoded_size_string(String *s) {
    return 1 + s->len;
}
uint8_t *encode_string(uint8_t *data, String *s) {
    *data++ = s->len;
    for (uint8_t i = 0; i < s->len; i++) *data++ = s->str[i];
    return data;
}
String decode_string(uint8_t **src) {
    uint8_t len = *(*src)++;
    char *data = (char *) *src;
    *src += len;
    return (String) { .len = len, .str = data };
}

uint16_t encoded_size_uint16_t(uint16_t *u) {
    (void) u;
    return 2;
}
uint8_t *encode_uint16_t(uint8_t *data, uint16_t *u) {
    *data++ = (uint8_t) *u;
    *data++ = (uint8_t) (*u >> 8);
    return data;
}
uint16_t decode_uint16_t(uint8_t **data) {
    return (uint8_t) *(*data)++ | (uint8_t)(*(*data)++ << 8);
}

uint16_t encoded_size_bool(bool *b) {
    (void) b;
    return 1;
}
uint8_t *encode_bool(uint8_t *data, bool *b) {
    *data++ = (uint8_t) *b;
    return data;
}
bool decode_bool(uint8_t **data) {
    return (bool) *(*data)++;
}

#include "../formpack/build/formpack.h"

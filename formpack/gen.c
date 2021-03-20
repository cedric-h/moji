// Sample program that takes C-like information specified in the Metadesk format
// and generates valid C code from it.

#include "./metadesk/source/md.h"
#include "./metadesk/source/md.c"
#include <inttypes.h>

/* Generates C code for a serializable struct. */
void gen_struct(MD_Node *node);

/* Generates C code for a (de)serializable tagged union.

   Each variant becomes a serializable struct.

   An enum tag is generated to identify the variants.

   Finally, a struct is created for the tagged union itself,
   which has just two fields: the tag enum, and a union which
   could hold any of the possible variants.
   */
void gen_tagun(MD_Node *node);

static FILE *f;
int main(int argument_count, char **arguments) {
    MD_Node *code = MD_ParseWholeFile(MD_S8Lit("../formpack/formpack.dd"));
    f = fopen("../formpack/build/formpack.h", "wb");
    
    for (MD_EachNode(node, code->first_child))
        if (MD_NodeHasTag(node, MD_S8Lit("struct")))
            gen_struct(node);
        else if (MD_NodeHasTag(node, MD_S8Lit("tagun")))
            gen_tagun(node);
    
    return 0;
}

void gen_struct_encoded_size(MD_Node *_struct);
void gen_struct_encode(MD_Node *_struct);
void gen_struct_decode(MD_Node *_struct);
void gen_struct(MD_Node *_struct) {
    MD_OutputTree_C_Struct(f, _struct);
    gen_struct_encoded_size(_struct);
    gen_struct_encode(_struct);
    gen_struct_decode(_struct);
}

MD_String8 variant_struct_name(MD_Node *tagun, MD_Node *variant) ;
void gen_tagun_decls(MD_Node *tagun);
void gen_tagun_encoded_size(MD_Node *tagun);
void gen_tagun_encode(MD_Node *tagun);
void gen_tagun_decode(MD_Node *tagun);
void gen_tagun(MD_Node *tagun) {
    /* each field in our tagged union is also a struct, we generate
       (de)serialization code for those struct fields here */
    for (MD_EachNode(variant, tagun->first_child)) {
        MD_Node variant_struct = *variant;
        variant_struct.string = variant_struct_name(tagun, variant);
        gen_struct(&variant_struct);
    }

    /* and a tag enum + decl for the tagged union itself */
    gen_tagun_decls(tagun);
    /* then code to make all that (de)serializable */
    gen_tagun_encoded_size(tagun);
    gen_tagun_encode(tagun);
    gen_tagun_decode(tagun);
}


/* snake case impl, we want NetMsg_AccountMake to become
   "net_msg_account_make", which is different than the default Metadesk
   behavior (which creates "netmsg_accountmake") */
MD_String8 snake_case(MD_String8 s) {
    MD_String8List result = {0},
                   words = MD_SplitStringByCharacter(s, '_');
    for (MD_String8Node *s8n = words.first; s8n != NULL; s8n = s8n->next) {
        MD_String8 new = MD_StyledStringFromString(s8n->string,
                                                   MD_WordStyle_LowerCase,
                                                   MD_S8Lit("_"));
        MD_PushStringToList(&result, new);
    }
    return MD_JoinStringListWithSeparator(result, MD_S8Lit("_"));
}

void assert_field_just_type(MD_Node *field) {
    int kid_count = MD_ChildCountFromNode(field);
    if (kid_count != 1) {
        MD_NodeErrorF(field,
                      "expected 1 type for field, "
                      "found %" PRIu64 ".",
                      kid_count);
        abort();
    }
}

MD_String8 variant_struct_name(MD_Node *tagun, MD_Node *variant) {
    return MD_PushStringF("%.*s_%.*s",
                          MD_StringExpand(tagun->string),
                          MD_StringExpand(variant->string));
}

MD_String8 variant_kind(MD_Node *tagun, MD_Node *variant) {
    return MD_PushStringF("%.*sKind_%.*s",
                          MD_StringExpand(tagun->string),
                          MD_StringExpand(variant->string));
}


// -------------------------- DECLS ----------------------------------
void gen_tagun_decls(MD_Node *node) {
    /*  here we generate the tag enum */
    fprintf(f, "typedef enum {\n");
    for (MD_EachNode(variant, node->first_child)) {
        fprintf(f, "%.*sKind_%.*s,\n",
               MD_StringExpand(node->string),
               MD_StringExpand(variant->string));
    }
    fprintf(f, "} %.*sKind;\n\n", MD_StringExpand(node->string));

    /* the struct which contains the tag and union */
    fprintf(f, "typedef struct {\n");
    fprintf(f, "%.*sKind kind;\n", MD_StringExpand(node->string));
    fprintf(f, "union {\n");
    for (MD_EachNode(variant, node->first_child)) {
        fprintf(f, "%.*s %.*s;\n",
               MD_StringExpand(variant_struct_name(node, variant)),
               MD_StringExpand(snake_case(variant->string)));
    }
    fprintf(f, "};\n"); // end union
    fprintf(f, "} %.*s;\n", MD_StringExpand(node->string)); // end struct

    /* constructor macros for instances of the tagged union of given variant */
    for (MD_EachNode(variant, node->first_child)) {
        MD_String8 variant_struct = variant_struct_name(node, variant);
        fprintf(f, "#define send_%.*s(mdk, ws) do {\\\n",
               MD_StringExpand(snake_case(variant_struct)));
        fprintf(f, " Pack _p = pack_%.*s(\\\n", MD_StringExpand(snake_case(node->string)));
        fprintf(f, "  &(%.*s) {\\\n", MD_StringExpand(node->string));
        fprintf(f, "   .kind = %.*s,\\\n", MD_StringExpand(variant_kind(node, variant)));
        fprintf(f, "   .%.*s = (mdk),\\\n", MD_StringExpand(snake_case(variant->string)));
        fprintf(f, "  });\\\n"); // end call
        fprintf(f, " bqws_send_binary((ws), _p.data, _p.size);\\\n"); // end call
        fprintf(f, " free(_p.data);\\\n"); // end call
        fprintf(f, "} while (false)\\\n\n\n"); // end constructor macro
    }
}


// -------------------------- ENCODED SIZE ----------------------------------
void gen_struct_encoded_size(MD_Node *stru) {
    fprintf(f, "uint16_t encoded_size_%.*s(%.*s *v) {\n",
           MD_StringExpand(snake_case(stru->string)),
           MD_StringExpand(stru->string));
    fprintf(f, "return 0");
    for (MD_EachNode(field, stru->first_child)) {
        assert_field_just_type(field);
        MD_Node *type = field->first_child;
        fprintf(f, "\n + encoded_size_%.*s(&v->%.*s)",
               MD_StringExpand(snake_case(type->string)),
               MD_StringExpand(field->string));
    }
    fprintf(f, ";\n}\n\n"); // end encoded_size_<stru> function
}

void gen_tagun_encoded_size(MD_Node *node) {
    /* returns size of the entire tagged union */
    fprintf(f, "uint16_t encoded_size_%.*s(%.*s *d) {\n", 
           MD_StringExpand(snake_case(node->string)),
           MD_StringExpand(node->string));
    /*  one byte for the variant */
    fprintf(f, "uint16_t nbytes = 1;\n");
    fprintf(f, "switch (d->kind) {\n");
    for (MD_EachNode(variant, node->first_child)) {
        fprintf(f, "case (%.*s):\n", MD_StringExpand(variant_kind(node, variant)));
        fprintf(f, " nbytes += encoded_size_%.*s(&d->%.*s);\n break;\n",
               MD_StringExpand(snake_case(variant_struct_name(node, variant))),
               MD_StringExpand(snake_case(variant->string)));
    }
    fprintf(f, "}\n"); // end switch
    fprintf(f, "return nbytes;\n}\n\n"); // end encoded_size<tagged union>
}


// -------------------------- ENCODE ----------------------------------
void gen_struct_encode(MD_Node *stru) {
    fprintf(f, "uint8_t *encode_%.*s(uint8_t *data, %.*s *v) {\n",
           MD_StringExpand(snake_case(stru->string)),
           MD_StringExpand(stru->string));
    for (MD_EachNode(field, stru->first_child)) {
        assert_field_just_type(field);
        MD_Node *type = field->first_child;
        fprintf(f, "data = encode_%.*s(data, &v->%.*s);\n",
               MD_StringExpand(snake_case(type->string)),
               MD_StringExpand(field->string));
    }
    fprintf(f, "return data;\n}\n\n"); // end encode<stru> function
}

void gen_tagun_encode(MD_Node *node) {
    /* serialization entry point, returns entire tagged union encoded */
    fprintf(f, "Pack pack_%.*s(%.*s *d) {\n", 
           MD_StringExpand(snake_case(node->string)),
           MD_StringExpand(node->string));
    fprintf(f, "uint16_t size = encoded_size_%.*s(d);\n"
           "uint8_t *data = (uint8_t *) malloc(size);\n",
           MD_StringExpand(snake_case(node->string)));
    fprintf(f, "uint8_t *writer = data;\n");
    /* next is the variant of the message */
    fprintf(f, "*writer++ = (uint8_t) d->kind;\n");
    fprintf(f, "switch (d->kind) {\n");
    for (MD_EachNode(variant, node->first_child)) {
        fprintf(f, "case (%.*s):\n",
               MD_StringExpand(variant_kind(node, variant)));
        fprintf(f, " writer = encode_%.*s(writer, &d->%.*s);\n break;\n",
               MD_StringExpand(snake_case(variant_struct_name(node, variant))),
               MD_StringExpand(snake_case(variant->string)));
    }
    fprintf(f, "}\n"); // end switch
    fprintf(f, "return (Pack) { .data = data, .size = size };\n}\n\n"); // end pack<tagged union>
}


// -------------------------- DECODE ----------------------------------
void gen_struct_decode(MD_Node *stru) {
    fprintf(f, "%.*s decode_%.*s(uint8_t **data) {\n",
           MD_StringExpand(stru->string),
           MD_StringExpand(snake_case(stru->string)));
    fprintf(f, "return (%.*s) {\n", MD_StringExpand(stru->string));
    for (MD_EachNode(field, stru->first_child)) {
        assert_field_just_type(field);
        MD_Node *type = field->first_child;
        fprintf(f, ".%.*s = decode_%.*s(data),\n",
               MD_StringExpand(field->string),
               MD_StringExpand(snake_case(type->string)));
    }
    fprintf(f, "};\n}\n\n"); // end encode<variant> function
}

void gen_tagun_decode(MD_Node *node) {
    /* deserialization entry point, returns entire tagged union decoded */
    fprintf(f, "%.*s unpack_%.*s(uint8_t *data) {\n", 
           MD_StringExpand(node->string),
           MD_StringExpand(snake_case(node->string)));

    fprintf(f, "%.*sKind kind = (%.*sKind) *data++;\n",
          MD_StringExpand(node->string),
          MD_StringExpand(node->string));
    /* next is the variant of the message */
    fprintf(f, "switch (kind) {\n");
    for (MD_EachNode(variant, node->first_child)) {
        fprintf(f, "case (%.*s):\n", MD_StringExpand(variant_kind(node, variant)));
        fprintf(f, " return (%.*s) {\n", MD_StringExpand(node->string));
        fprintf(f, "  .kind = kind,\n");
        fprintf(f, "  .%.*s = decode_%.*s(&data),\n",
               MD_StringExpand(snake_case(variant->string)),
               MD_StringExpand(snake_case(variant_struct_name(node, variant))));
        fprintf(f, " };\n"); // end return
    }
    fprintf(f, "default: abort();\n");
    fprintf(f, "}\n"); // end switch
    fprintf(f, "}\n\n"); // end unpack<tagged union>
}

//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "buf.h"

buf_t *buf_new() {
    buf_t *b;
    b = malloc(sizeof(buf_t));
    b->buf = malloc(BUF_INITIAL_SIZE);
    b->allocated = BUF_INITIAL_SIZE;
    b->len = 0;

    return b;
}

void buf_free(buf_t *b) {
    free(b->buf);
    free(b);
}

void buf_dump(buf_t *b) {
    int i, j;

    for (i = 0; i < b->len; i+=16) {
        printf("%.4x ", i);
        for (j = 0; j < 16; j ++) {
            if (i+j < b->len) {
                printf("%.2x ", b->buf[i+j]);
            } else {
                printf("   ");
            }
        }

        for (j = 0; j < 16; j ++) {
            int ch = ' ';
            if (i+j < b->len) {
                ch = b->buf[i+j];
            }

            if (!isprint(ch)) {
                ch = '.';
            }
            printf("%c", ch);
        }
        printf("\n");
    }
}
static void buf_check_realloc(buf_t *b, int sz) {
    if (b->len + sz <= b->allocated) {
        return;
    }
    int new_sz;
    int required_sz;

    required_sz = b->len + sz;
    for (new_sz = b->allocated + BUF_INITIAL_SIZE; new_sz < required_sz;) {
        new_sz += BUF_INITIAL_SIZE;
    }

    b->buf = realloc(b->buf, new_sz);
}

void buf_add_uint8(buf_t *b, uint8_t v) {
    buf_check_realloc(b, 1);
    b->buf[b->len] = v;
    b->len++;
}

void buf_add_uint16(buf_t *b, uint16_t v) {
    buf_check_realloc(b, 2);

    *((uint16_t *)&(b->buf[b->len])) = htons(v);
    b->len += 2;
}

void buf_add_uint32(buf_t *b, uint32_t v) {
    buf_check_realloc(b, 4);
    *((uint32_t *)&(b->buf[b->len])) = htonl(v);
    b->len += 4;
}

void buf_add_int32(buf_t *b, int32_t v) {
    buf_check_realloc(b, 4);
    *((int32_t *)&(b->buf[b->len])) = htonl(v);
    b->len += 4;
}

void buf_add_bytes(buf_t *b, int len, const uint8_t *bytes) {
    buf_check_realloc(b, len);
    memcpy(b->buf+b->len, bytes, len);
    b->len += len;
}

void buf_add_string(buf_t *b, const char *str) {
    int len = strlen(str);
    buf_add_bytes(b, len, (const uint8_t *)str);
}


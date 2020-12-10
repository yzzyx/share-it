//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef SHAREIT_BUF_H
#define SHAREIT_BUF_H
#include <stdint.h>

#define BUF_INITIAL_SIZE 32

typedef struct {
    uint8_t *buf;
    int len;
    int allocated;
} buf_t;

buf_t *buf_new();
void buf_free(buf_t *);
void buf_dump(buf_t *);
void buf_add_uint8(buf_t *, uint8_t);
void buf_add_uint16(buf_t *, uint16_t);
void buf_add_uint32(buf_t *, uint32_t);
void buf_add_int32(buf_t *, int32_t);
void buf_add_bytes(buf_t *, int len, const uint8_t *bytes);
void buf_add_string(buf_t *, const char *str);
#endif

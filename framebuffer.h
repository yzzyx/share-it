// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef GRAB_FRAMEBUFFER_H
#define GRAB_FRAMEBUFFER_H
#include "shareit.h"

enum framebuffer_encoding_type {
    framebuffer_encoding_type_raw = 0,
    framebuffer_encoding_type_copyrect = 1,
    framebuffer_encoding_type_zrle = 16,

    /* Note, these are not implemented
    framebuffer_encoding_type_rre = 2,
    framebuffer_encoding_type_hextile = 5,
    framebuffer_encoding_type_zlib = 6,
    framebuffer_encoding_type_trle = 15,
    framebuffer_encoding_type_zlib_hex = 8,
    framebuffer_encoding_type_cursor = -239,
    framebuffer_encoding_type_desktop_size = -223,
    */
};

typedef struct {
    uint16_t source_x;
    uint16_t source_y;
} framebuffer_encoding_copyrect_t;

enum rle_encoding_type {
    rle_encoding_type_raw = 0,
    rle_encoding_type_solid = 1,
    rle_encoding_type_packed_palette = 2, // 2 - 16 are packed palette types,
    rle_encoding_type_plain_rle = 128,
    rle_encoding_type_palette_rle = 130, // 130 - 255 are palette rle's
};

typedef struct {
    uint32_t *data;
} rle_encoding_raw;

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rle_encoding_solid;

typedef struct {
    enum rle_encoding_type sub_encoding;
    union {
        rle_encoding_raw raw;
        rle_encoding_solid solid;
    };
} framebuffer_encoding_zrle_t;

typedef struct {
    uint16_t xpos;
    uint16_t ypos;
    uint16_t width;
    uint16_t height;
    enum framebuffer_encoding_type encoding_type;

    union {
        framebuffer_encoding_copyrect_t copyrect;
        framebuffer_encoding_zrle_t zrle;
    } enc;
} framebuffer_rect_t;

typedef struct {
    uint16_t n_rects;
    framebuffer_rect_t **rects;
} framebuffer_update_t;

void free_framebuffer_update(framebuffer_update_t *update);
void copy_block(shareit_app_t *app, uint32_t *block, int x, int y, int w, int h);
int compare_parts(shareit_app_t *app, int x, int y, int w, int h);
int compare_screens(shareit_app_t *app, framebuffer_update_t **update);
int draw_update(viewinfo_t *view, framebuffer_update_t *update);
#endif
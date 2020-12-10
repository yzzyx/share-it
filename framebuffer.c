//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#include <stdint.h>
#include <string.h>
#include "shareit.h"
#include "framebuffer.h"

#define BLOCK_WIDTH 64
#define BLOCK_HEIGHT 64

// Allocate in chunks of 20
#define RECT_LIST_ALLOC_SZ 20

int min(int a, int b) {
    if (a < b) {
        return a;
    }
    return b;
}

void free_zrle_enc(framebuffer_encoding_zrle_t *zrle) {
    switch (zrle->sub_encoding) {
    case rle_encoding_type_solid:
        // nothing allocated
        break;
    case rle_encoding_type_raw:
        free(zrle->raw.data);
        break;
    default:
        fprintf(stderr, "%s: encoding type %d not implemented!\n", __FUNCTION__, zrle->sub_encoding);
        break;
    }
}

void free_framebuffer_rect(framebuffer_rect_t *rect) {
    switch (rect->encoding_type) {
    case framebuffer_encoding_type_zrle:
        free_zrle_enc(&rect->enc.zrle);
        break;
    default:
        fprintf(stderr, "%s: encoding type %d not implemented!\n", __FUNCTION__, rect->encoding_type);
        break;
    }
    free(rect);
}

void free_framebuffer_update(framebuffer_update_t *update) {
    if (update == NULL) {
        return;
    }

    for (int i = 0; i < update->n_rects; i ++) {
        framebuffer_rect_t *rect = update->rects[i];
        free_framebuffer_rect(rect);
    }
    free(update);
}

/**
 * copy a block from the current app screen into 'block'
 *
 * @param[in]  app    the main application
 * @param[out] block  buffer to copy the contents to
 * @param[in]  x      x position to start copying from
 * @param[in]  y      y position to start copying from
 * @param[in]  w      width of block to copy
 * @param[in]  h      height of block to copy
 */
void copy_block(shareit_app_t *app, uint32_t *block, int x, int y, int w, int h) {
    int row;
    int max_x = min(app->width, x+w);
    int max_y = min(app->height, y+h);
    int sz;

    sz = (max_x - x) * (int)sizeof(uint32_t);
    for (row = 0; row < h; row ++, y++) {
        if (y >= max_y || max_x < app->width) {
            // make sure we don't have any old data in the buffer
            memset((uint8_t *)(block+w*row), 0x00, w*sizeof(uint32_t));
            if (y >= max_y) {
                continue;
            }
        }

        int startpos = y*app->width + x;
        memcpy((uint8_t *)(block+row*w), (uint8_t *)(app->current_screen+startpos), sz);
    }
}

/**
 * compare current screen with prev screen and check if the specified block differs
 *
 * @param app the main application
 * @param x   x position of block to compare
 * @param y   y position of block to compare
 * @param w   width of block
 * @param h   height of block
 * @return  1 if blocks differ, and 0 if they're equal
 */
int compare_parts(shareit_app_t *app, int x, int y, int w, int h) {
    int max_x, max_y;
    int sz;

    if (app->prev_screen == NULL) {
        return 1;
    }

    max_x = min(x+w, app->width);
    max_y = min(y+h, app->height);

    sz = (max_x - x) * (int)sizeof(uint32_t);
    for (; y < max_y; y++) {
        int startpos = x + y*app->width;
        if (memcmp((uint8_t *)(app->current_screen+startpos), (uint8_t *)(app->prev_screen+startpos), sz) != 0) {
            return 1;
        }
    }

    return 0;
}

/**
 * Count the number of colours in a rect
 * @param app
 * @param x
 * @param y
 * @param w
 * @param h
 * @return returns TRUE if rect consists of only one colour, otherwise FALSE
 */
int rect_palette(shareit_app_t *app, int x, int y, int w, int h, uint32_t **output_palette) {
    uint32_t palette[32];
    uint8_t n_colours = 0;
    uint32_t pixel;
    int i;
    int has_colour;
    int max_x = min(app->width, x+w);
    int max_y = min(app->height, y+h);

    int sx, sy;
    for (sy = y; sy < max_y; sy ++) {
        for (sx = x; sx < max_x; sx ++) {
            pixel = app->current_screen[sx+sy*app->width] & 0xffffff;
            has_colour = 0;
            for (i = 0; i < n_colours; i++) {
                if (palette[i] == pixel) {
                    has_colour = 1;
                    break;
                }
            }

            if (has_colour) {
                continue;
            }

            if (n_colours == 32) {
                return 0;
            }
            palette[n_colours] = pixel;
            n_colours ++;
        }
    }

    if (output_palette != NULL) {
        *output_palette = palette;
    }
    return n_colours;
}

/**
 * Create a new framebuffer rect from app->current_screen
 *
 * @param app  The main application
 * @param x    X-position of the rect
 * @param y    Y-position of the recgt
 * @param w    Width
 * @param h    Height
 * @return     Returns a newly allocated framebuffer_rect_t
 */
framebuffer_rect_t *create_rect(shareit_app_t *app, int x, int y, int w, int h) {
    framebuffer_rect_t  *rect;

    rect = malloc(sizeof(framebuffer_rect_t));
    rect->xpos = x;
    rect->ypos = y;
    rect->width = w;
    rect->height = h;

    uint32_t *palette;
    int colour_count = rect_palette(app, x, y, w, h, &palette);

    if (colour_count == 1) {
        rect->encoding_type = framebuffer_encoding_type_zrle;
        rect->enc.zrle.sub_encoding = rle_encoding_type_solid;
        uint32_t pixel = palette[0];
        rect->enc.zrle.solid.red = pixel & 0xff;
        rect->enc.zrle.solid.green = (pixel >> 8) & 0xff;
        rect->enc.zrle.solid.blue = (pixel >> 16) & 0xff;

        return rect;
    }

    // No other types matched, go with raw ZRLE
    rect->encoding_type = framebuffer_encoding_type_zrle;
    rect->enc.zrle.sub_encoding = rle_encoding_type_raw;

    rect->enc.zrle.raw.data = malloc(sizeof(uint32_t) * w * h);
    copy_block(app, rect->enc.zrle.raw.data, x, y, w, h);
    return rect;
}

/**
 * Check for changes between current screen and our previous buffer
 *
 * @param[in]  app    the main application
 * @param[out] update if the screen has changed, this will create a framebuffer update that can be
 *                    sent to the server.
 * @return returns TRUE if screen has been changed, otherwise FALSE
 */
int compare_screens(shareit_app_t *app, framebuffer_update_t **output) {
    int n_rects = 0;
    framebuffer_update_t *update;
    framebuffer_rect_t *rect;
    framebuffer_rect_t **rect_list = NULL;
    int rect_list_sz = 0;
    int x, y;
    int ret;

    // Split the image into 64x64 parts while checking if they've been updated
    for (y = 0; y < app->height; y+=BLOCK_HEIGHT) {
        for (x = 0; x < app->width; x+=BLOCK_WIDTH) {
            ret = compare_parts(app, x, y, BLOCK_WIDTH, BLOCK_HEIGHT);
            if (ret) {
                rect = create_rect(app, x, y, BLOCK_WIDTH, BLOCK_HEIGHT);
                if (n_rects == rect_list_sz) {
                    rect_list_sz += RECT_LIST_ALLOC_SZ;
                    rect_list = realloc(rect_list, rect_list_sz*sizeof(framebuffer_rect_t *));
                }
                rect_list[n_rects] = rect;
                n_rects ++;
            }
        }
    }

    if (n_rects > 0) {
        update = malloc(sizeof(framebuffer_update_t));
        update->n_rects = n_rects;
        update->rects = rect_list;
        *output = update;
        return TRUE;
    }

    return FALSE;
}


/**
 * blit/draw a block of a specific color to position x,y
 *
 * @param app  main application - information will be written to app->view_pixels
 * @param x    x position of block
 * @param y    y position of block
 * @param w    width of block
 * @param h    height of block
 * @param r    red
 * @param g    green
 * @param b    blur
 */
void view_blit_solid(shareit_app_t *app, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    int sx, sy;

    for (sy = 0; sy < h && (y+sy) < app->view_height; sy ++) {
        for (sx = 0; sx < w && (x+sx) < app->view_width; sx ++) {
            int pos = (x+sx)*3 + (y+sy)*app->view_row_stride;
            app->view_pixels[pos] = r;
            app->view_pixels[pos+1] = g;
            app->view_pixels[pos+2] = b;
        }
    }
}

/**
 * blit/draw contents of 'raw' to position x,y
 *
 * @param app   main application - information will be written to app->view_pixels
 * @param x     x position of block
 * @param y     y position of block
 * @param w     width of block
 * @param h     height of block
 * @param raw   source data
 */
void view_blit_raw(shareit_app_t *app, int x, int y, int w, int h, const uint32_t *raw) {
    int sy, sx;

    for (sy = 0; sy < h && y+sy < app->view_height; sy ++) {
        for (sx = 0; sx < w  && x+sx < app->view_width; sx ++) {
            uint8_t r, g, b;
            r = raw[sx+sy*w] & 0xff;
            g = (raw[sx+sy*w] >> 8) & 0xff;
            b = (raw[sx+sy*w] >> 16) & 0xff;
            app->view_pixels[(x+sx)*3 + (y+sy) * app->view_row_stride] = r;
            app->view_pixels[(x+sx)*3 + (y+sy) * app->view_row_stride + 1] = g;
            app->view_pixels[(x+sx)*3 + (y+sy) * app->view_row_stride + 2] = b;
        }
    }
}

int draw_update_zrle(shareit_app_t *app, framebuffer_rect_t *rect) {
    int ret = 0;
    framebuffer_encoding_zrle_t *zrle = &rect->enc.zrle;
    switch (zrle->sub_encoding) {
    case rle_encoding_type_raw:
        view_blit_raw(app, rect->xpos, rect->ypos, rect->width, rect->height, zrle->raw.data);
        break;
    case rle_encoding_type_solid:
        view_blit_solid(app, rect->xpos, rect->ypos, rect->width, rect->height,
                        zrle->solid.red, zrle->solid.green, zrle->solid.blue);
        break;
    default:
        fprintf(stderr, "sub-encoding %d not implemented\n", zrle->sub_encoding);
        return -1;
    }

    return ret;
}

int draw_update(shareit_app_t *app, framebuffer_update_t *update) {
    int i, ret;

    for (i = 0; i < update->n_rects; i++) {
        framebuffer_rect_t *rect = update->rects[i];

        switch (rect->encoding_type) {
        case framebuffer_encoding_type_zrle:
            ret = draw_update_zrle(app, rect);
            break;
        default:
            fprintf(stderr, "unhandled encoding type %d\n", rect->encoding_type);
            return -1;
        }

        if (ret != 0) {
            return ret;
        }
    }

    return 0;
}
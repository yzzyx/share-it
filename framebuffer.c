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

void free_framebuffer_rect(framebuffer_rect_t *rect) {
    switch (rect->encoding_type) {
    case framebuffer_encoding_type_raw:
        free(rect->enc.raw.data);
    case framebuffer_encoding_type_solid:
        /* noop */
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
    free(update->rects);
    free(update);
}

/**
 * copy a block from the current app screen into a raw data segment
 *
 * @param[in]  app    the main application
 * @param[out] block  buffer to copy the contents to
 * @param[in]  x      x position to start copying from
 * @param[in]  y      y position to start copying from
 * @param[in]  w      width of block to copy
 * @param[in]  h      height of block to copy
 */
void copy_screen_to_raw(shareit_app_t *app, uint8_t *block, int x, int y, int w, int h) {
    int row;
    int max_x = min(app->width, x+w);
    int max_y = min(app->height, y+h);

    for (row = 0; row < h; row ++, y++) {
        uint8_t *output_row = block + w*row*3;
        if (y >= max_y || max_x < app->width) {
            // make sure we don't have any old data in the buffer
            memset(output_row, 0x00, w*3);
            if (y >= max_y) {
                continue;
            }
        }

        for (int col = 0; col < w && x+col < app->width; col ++) {
            uint8_t *pixel = block + (row * w + col) * 3;
            uint8_t *source = ((uint8_t *)(app->current_screen)) + (y*app->width + x + col)*sizeof(uint32_t);
            pixel[0] = source[0];
            pixel[1] = source[1];
            pixel[2] = source[2];
        }
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
        rect->encoding_type = framebuffer_encoding_type_solid;
        uint32_t pixel = palette[0];
        rect->enc.solid.red = pixel & 0xff;
        rect->enc.solid.green = (pixel >> 8) & 0xff;
        rect->enc.solid.blue = (pixel >> 16) & 0xff;
        return rect;
    }

    // No other types matched, go with raw ZRLE
    rect->encoding_type = framebuffer_encoding_type_raw;
    rect->enc.raw.data = malloc(w * h * 3);
    copy_screen_to_raw(app, rect->enc.raw.data, x, y, w, h);
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
void view_blit_solid(viewinfo_t *view, int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    int sx, sy;

    for (sy = 0; sy < h && (y+sy) < view->height; sy ++) {
        for (sx = 0; sx < w && (x+sx) < view->width; sx ++) {
            int pos = (x+sx)*4 + (y+sy)*view->row_stride;
            view->pixels[pos+0] = r;
            view->pixels[pos+1] = g;
            view->pixels[pos+2] = b;
            // Alpha channel is unused
            // view->pixels[pos+3] = alpha;
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
 * @param raw   source data in RGB format (24bits per pixel)
 */
void view_blit_raw(viewinfo_t *view, int x, int y, int w, int h, const uint8_t *raw) {
    int sy, sx;

    for (sy = 0; sy < h && y+sy < view->height; sy ++) {
        for (sx = 0; sx < w  && x+sx < view->width; sx ++) {
            const uint8_t *pixel = raw + (sx + sy*w) * 3;
            view->pixels[(x+sx)*4 + (y+sy) * view->row_stride + 0] = pixel[0];
            view->pixels[(x+sx)*4 + (y+sy) * view->row_stride + 1] = pixel[1];
            view->pixels[(x+sx)*4 + (y+sy) * view->row_stride + 2] = pixel[2];
            // view->pixels[(x+sx)*4 + (y+sy) * view->row_stride + 0 ] = 0; // alpha (unused)
        }
    }
}

/**
 * draw framebuffer update to specified view
 *
 * @param view   view to draw update to
 * @param update update to draw
 * @return 0 on success
 */
int draw_update(viewinfo_t *view, framebuffer_update_t *update) {
    for (int i = 0; i < update->n_rects; i++) {
        framebuffer_rect_t *rect = update->rects[i];

        switch (rect->encoding_type) {
        case framebuffer_encoding_type_raw:
            view_blit_raw(view, rect->xpos, rect->ypos, rect->width, rect->height, rect->enc.raw.data);
            break;
        case framebuffer_encoding_type_solid:
            view_blit_solid(view, rect->xpos, rect->ypos, rect->width, rect->height,
                            rect->enc.solid.red, rect->enc.solid.green, rect->enc.solid.blue);
            break;
        default:
            fprintf(stderr, "%s: unhandled encoding type %d\n", __FUNCTION__, rect->encoding_type);
            return -1;
        }
    }

    return 0;
}
//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#include <xcb/xcb.h>
#include <xcb/xfixes.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>
#include <malloc.h>
#include <zlib.h>

#define CHUNK 16384

static int check_xfixes(xcb_connection_t *conn) {
    xcb_xfixes_query_version_cookie_t cookie;
    xcb_xfixes_query_version_reply_t *reply;

    // We need atleast version 1 to get cursor
    cookie = xcb_xfixes_query_version(conn, 1, 0);
    reply  = xcb_xfixes_query_version_reply(conn, cookie, NULL);

    if (reply) {
        free(reply);
        return 1;
    }
    return 0;
}

typedef struct grab_xcb_t {
    xcb_connection_t *conn;
    xcb_drawable_t win;
    int can_grab_cursor;
    int width;
    int height;
} grab_xcb_t;

void *grab_initialize() {
    grab_xcb_t *info;
    info = malloc(sizeof(grab_xcb_t));

    info->conn = xcb_connect(NULL, NULL);
    if (info->conn == NULL) {
        fprintf(stderr, "could not connect\n");
        return NULL;
    }
    printf("connected\n");

    info->can_grab_cursor = check_xfixes(info->conn);
    if (!info->can_grab_cursor) {
        printf("cannot grab cursor, no xfixes\n");
    } else {
        printf("CAN grab cursor\n");

    }

    const xcb_setup_t *setup = xcb_get_setup (info->conn);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator (setup);
    xcb_screen_t *screen = iter.data;
    info->win = screen->root;

    xcb_get_geometry_reply_t *geom;
    geom = xcb_get_geometry_reply (info->conn, xcb_get_geometry (info->conn, info->win), NULL);
    if (geom == NULL) {
        printf("no geom returned\n");
        return NULL;
    }

    info->width = geom->width;
    info->height = geom->height;
    return info;
}


int grab_window_size(grab_xcb_t *info, int *width, int *height) {
    *width = info->width;
    *height = info->height;
    return 0;
}

int grab_window(grab_xcb_t *info, uint8_t *output) {
    xcb_get_image_cookie_t cookie;
    xcb_get_image_reply_t *reply;
    uint8_t format = XCB_IMAGE_FORMAT_Z_PIXMAP;
    uint32_t plane_mask = ~0;

    if (output == NULL) {
        return 0;
    }

    cookie = xcb_get_image(info->conn, format, info->win, 0, 0, info->width, info->height, plane_mask);
    reply = xcb_get_image_reply(info->conn, cookie, NULL);
    if (reply == NULL) {
        printf("could not grab image\n");
        return -1;
    }

    uint8_t *img = xcb_get_image_data(reply);
    int len = xcb_get_image_data_length(reply);

    // ZPixmap images are in BGRA format, but we want RGBA for our output buffer
    for ( ; len>0 ; len-=4) {
        output[0] = img[2];
        output[1] = img[1];
        output[2] = img[0];
        output[3] = img[3];
        output += 4;
        img += 4;
    }
    free(reply);
    return 0;
}

void grab_shutdown(grab_xcb_t *info) {
    xcb_disconnect(info->conn);
}

/*
 * grab_cursor_position()
 *
 * returns which cursor is used and where it's located
 */
void grab_cursor_position(grab_xcb_t *info, int *x, int *y) {
    xcb_query_pointer_reply_t *cur;

    *x = 0;
    *y = 0;
    cur = xcb_query_pointer_reply(info->conn, xcb_query_pointer(info->conn, info->win), NULL);
    if (cur == NULL) {
        return;
    }
    *x = cur->root_x;
    *y = cur->root_y;
    free(cur);
}

void grab_cursor(xcb_connection_t *conn, uint32_t *image, int img_x, int img_y, int img_w, int img_h) {
    xcb_xfixes_get_cursor_image_cookie_t cur_cookie;
    xcb_xfixes_get_cursor_image_reply_t *cur;
    uint32_t *cursor;

    cur_cookie = xcb_xfixes_get_cursor_image(conn);
    cur = xcb_xfixes_get_cursor_image_reply(conn, cur_cookie, NULL);
    if (cur == NULL) {
        return;
    }

    cursor = xcb_xfixes_get_cursor_image_cursor_image(cur);
    if (cursor == NULL) {
        return;
    }

    uint8_t alpha, r, g, b, ir, ig, ib, ia;
    int x, y, end_x, end_y;

    // FIXME - calculate start if cursor is located just before the window
    end_x = cur->width;
    if ((cur->x + cur->width) > (img_x + img_w)) {
        end_x = (img_w - img_x) - cur->x;
    }

    end_y = cur->height;
    if ((cur->y + cur->height) > img_h) {
        end_y = (img_h - img_y) - cur->y;
    }

    for (y = 0; y < end_y; y++) {
        for (x = 0; x < end_x; x++) {
            alpha = (cursor[y * cur->width + x] >> 24) & 0xff;
            switch (alpha) {
            case 0:
                continue;
            case 255:
                image[(cur->y + y) * img_w + (cur->x + x)] = cursor[y*cur->width+x];
            default:
                r = (cursor[y * cur->width + x]) & 0xff;
                g = (cursor[y * cur->width + x] >> 8) & 0xff;
                b = (cursor[y * cur->width + x] >> 16) & 0xff;
                ir = (image[(cur->y + y) * img_w + (cur->x + x)]) & 0xff;
                ig = (image[(cur->y + y) * img_w + (cur->x + x)] >> 8) & 0xff;
                ib = (image[(cur->y + y) * img_w + (cur->x + x)] >> 16) & 0xff;
                ia = (image[(cur->y + y) * img_w + (cur->x + x)] >> 24) & 0xff;

                image[(cur->y + y) * img_w + (cur->x + x)] = (ir + (r*(255-alpha)+127)/255) |
                        (ig + (g*(255-alpha)+127)/255) >> 8 |
                        (ib + (b*(255-alpha)+127)/255) >> 16 |
                        ia >> 24;
            }
        }
    }
    free(cur);
    return;
}

int print_window_info(xcb_connection_t *conn, xcb_drawable_t win) {
    //xcb_get_window_attributes_reply_t *attr;
    xcb_get_geometry_reply_t *geom;
    //
    //attr = xcb_get_window_attributes_reply(conn, xcb_get_window_attributes (conn, win), NULL);

    xcb_get_property_reply_t *prop;
    prop = xcb_get_property_reply(conn, xcb_get_property(conn, 0, win, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 0, 8), NULL);
    if (prop == NULL) {
        printf("no prop returned\n");
        return -1;
    }
    int len = xcb_get_property_value_length(prop);
    if (len == 0) {
        /* printf("skipping noname-window\n"); */
        return 0;

    }

    xcb_get_window_attributes_reply_t *attr;
    attr = xcb_get_window_attributes_reply (conn, xcb_get_window_attributes (conn, win), NULL);
    if (attr == NULL ) {
        printf("no attr retunred\n");
        return -1;
    }

    if (attr->map_state == 0) {
        /* printf("not mapped\n"); */
        return 0;
    }
    free(attr);

    printf("WM_NAME is %.*s\n", len, (char *)xcb_get_property_value(prop));
    free(prop);


    geom = xcb_get_geometry_reply (conn, xcb_get_geometry (conn, win), NULL);
    if (geom == NULL) {
        printf("no geom returned\n");
        return -1;
    }
    /* Do something with the fields of geom */
    printf(" |- x      : %d\n"
           " |- y      : %d\n"
           " |- depth  : %d\n"
           " |- length : %d\n"
           " |- width  : %d\n"
           " |- height : %d\n"
           " |- seq    : %d\n"
           " |- root   : %x\n"
           , geom->x, geom->y,
           geom->depth, geom->length,
           geom->width, geom->height,
           geom->sequence, geom->root);

    free (geom);

    return 0;
}

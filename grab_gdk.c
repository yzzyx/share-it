//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#include <gtk/gtk.h>
#include <string.h>
#include <inttypes.h>
#include <malloc.h>

typedef struct {
    GdkWindow *root;
    int width;
    int height;
} grab_gdk_t;

void *grab_initialize() {
    gint x, y;

    grab_gdk_t *info;
    info = malloc(sizeof(grab_gdk_t));

    info->root = gdk_get_default_root_window();
    gdk_window_get_geometry(info->root, &x, &y, &info->width, &info->height);

    return info;
}

int grab_window_size(grab_gdk_t *info, int *width, int *height) {
    *width = info->width;
    *height = info->height;
    return 0;
}

int grab_window(grab_gdk_t *info, uint8_t *output) {
    GdkPixbuf *px = gdk_pixbuf_get_from_window(info->root, 0, 0, info->width, info->height);
    if (px == NULL) {
        return -1;
    }

    gboolean has_alpha = gdk_pixbuf_get_has_alpha(px);
    uint8_t *pixels = gdk_pixbuf_get_pixels(px);
    int stride = gdk_pixbuf_get_rowstride(px);
    int n_channels = gdk_pixbuf_get_n_channels(px);

    for (int y = 0; y < info->height; y ++) {
        for (int x = 0; x < info->width; x ++) {
            uint8_t *target = output + (y * info->width + x) * 4;
            uint8_t *source = pixels + y * stride + x * n_channels;

            target[2] = source[0];
            target[1] = source[1];
            target[0] = source[2];
            if (has_alpha && n_channels == 4) {
                target[3] = source[3];
            }
        }
    }

    g_object_unref(px);
    return 0;
}

void grab_shutdown(grab_gdk_t *info) {
    free(info);
}

/*
 * grab_cursor_position()
 *
 * returns which cursor is used and where it's located
 */
void grab_cursor_position(grab_gdk_t *info, int *x, int *y) {
    GdkDevice *device;
    gint cx, cy;
    *x = 0;
    *y = 0;

    GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
    device = gdk_seat_get_pointer(seat);
    gdk_window_get_device_position (info->root, device, &cx, &cy, NULL);

    if (cx > 0 && cx < info->width  &&
        cy > 0 && cy <= info->height) {
        *x = cx;
        *y = cy;
    }
}
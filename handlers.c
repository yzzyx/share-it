//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#include <errno.h>
#include "shareit.h"
#include "framebuffer.h"
#include "handlers.h"
#include "packet.h"

int app_handle_join_response(shareit_app_t *app) {
    pkt_session_join_response_t pkt;
    int err;

    if ((err = pkt_recv_session_join_response(app->conn->socket, &pkt)) != 0) {
        show_error(app, "error while reading session join response: %s", strerror(err));
        return -1;
    }

    switch (pkt.status) {
    case SESSION_JOIN_CLIENT_JOINED:
        printf("client %s joined session\n", pkt.client_name);
        free(pkt.client_name);
        break;
    case SESSION_JOIN_CLIENT_LEFT:
        printf("client %s left session\n", pkt.client_name);
        free(pkt.client_name);
        break;
    case SESSION_JOIN_OK:
        printf("session joined!\n");
        break;
    default:
        printf("unknown status %d\n", pkt.status);
        break;

    }

    return 0;
}

int app_handle_cursor_info(shareit_app_t *app) {
    uint16_t x, y;
    uint8_t cursor;

    if (pkt_recv_cursorinfo(app->conn->socket, &x, &y, &cursor)) {
        show_error(app, "error while reading cursor info: %s");
        return -1;
    }
    printf("cursor: %d,%d\n", x, y);
    return 0;
}

int app_handle_screenshare_start(shareit_app_t *app) {
    uint16_t width, height;
    if (pkt_recv_session_screenshare_start_request(app->conn->socket, &width, &height)) {
        show_error(app, "error while reading screenshare info");
        return -1;
    }

    if (app->view == NULL) {
        app->view = calloc(1, sizeof(viewinfo_t));
        if (app->view == NULL) {
            perror("calloc(app->view)");
            return -1;
        }
    }

    if (app->view->pixels != NULL) {
        free(app->view->pixels);
    }

    app->view->pixels = calloc(width*height, sizeof(uint32_t));
    app->view->row_stride = width*sizeof(uint32_t);
    app->view->width = width;
    app->view->height = height;

    gtk_widget_set_size_request(GTK_WIDGET(app->screen_share_area), width, height);
    gtk_widget_show_all(app->screen_share_window);
    return 0;
}

int app_handle_framebuffer_update(shareit_app_t *app) {
    framebuffer_update_t *update;
    int err;

    if ((err = pkt_recv_framebuffer_update(app->conn->socket, &update)) != 0) {
        show_error(app, "error while reading screendata: %s", strerror(err));
        return -1;
    }

    // FIXME - check that we're actually in a session as a viewer
    if (app->view != NULL) {
        draw_update(app->view, update);
    }

    gtk_widget_queue_draw(GTK_WIDGET(app->screen_share_area));
    free_framebuffer_update(update);
    return 0;
}

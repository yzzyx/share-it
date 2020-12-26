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

    printf("joined session with size %d x %d\n", pkt.width, pkt.height);
    return 0;
}

int app_handle_framebuffer_update(shareit_app_t *app) {
    framebuffer_update_t *update;
    int err;

    if ((err = pkt_recv_framebuffer_update(app->conn->socket, app->input_stream, &update)) != 0) {
        show_error(app, "error while reading screendata: %s", strerror(err));
        return -1;
    }

    // FIXME - check that we're actually in a session as a viewer
    if (app->view != NULL) {
        draw_update(app->view, update);
    }

    free_framebuffer_update(update);
    return 0;
}

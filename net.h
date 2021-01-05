//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef SHAREIT_NET_H
#define SHAREIT_NET_H

enum signal {
    SIGNAL_CONNECTING,
    SIGNAL_DISCONNECTED,
    SIGNAL_CONNECTED,
    SIGNAL_ERROR,
    SIGNAL_SESSION_JOINED,
    SIGNAL_SESSION_CLIENT_JOINED,
    SIGNAL_SESSION_CLIENT_LEFT,
    SIGNAL_SESSION_LEFT,

    // Signals used to share screen
    SIGNAL_SCREEN_SHARE_START,
    SIGNAL_CURSOR_UPDATE,
    SIGNAL_FRAMEBUFFER_UPDATE,

    SIGNAL_UNUSED_MAX,
};

typedef struct {
    char *hostname;
    char *port;

    struct addrinfo *addr;
    int socket;

    GIOChannel *channel;
    GList *signal_handlers[SIGNAL_UNUSED_MAX];
} connection_t;

typedef struct {
    uint16_t width;
    uint16_t height;
}
screensize_t;


typedef struct {
    uint16_t x;
    uint16_t y;
    uint8_t cursor;
}
cursorinfo_t;

// handlefunc_t desribes a handler for a network signal
//  - if the handlefunc returns FALSE (0), it will be removed from the list
typedef gboolean (*handlefunc_t) (void *signalinfo, void *data);


connection_t *net_new();
int net_connect(connection_t *conn, const char *url, char **error);
int net_disconnect(connection_t *conn);
int net_join_session(connection_t *conn, const char *session_name, char **error);
int net_leave_session(connection_t *conn);
int net_start_screenshare(connection_t *conn, int width, int height);
void net_free(connection_t *conn);

gpointer net_signal_connect(connection_t *conn, enum signal sig, handlefunc_t fn, void*data);
void net_signal_disconnect(connection_t *conn, enum signal sig, gpointer handler);
void net_signal_emit(connection_t *conn, enum signal sig, gpointer info);
#endif
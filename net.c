//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "net.h"
#include "packet.h"


static int recv_join_response(connection_t *conn) {
    pkt_session_join_response_t pkt;
    char *err;

    if (pkt_recv_session_join_response(conn->socket, &pkt) != 0) {
        err = g_strdup_printf("error receiving session join information: %s", strerror(errno));
        net_signal_emit(conn, SIGNAL_ERROR, err);
        g_free(err);
        return -1;
    }

    switch (pkt.status) {
        case SESSION_JOIN_CLIENT_JOINED:
            net_signal_emit(conn, SIGNAL_SESSION_CLIENT_JOINED, pkt.client_name);
            free(pkt.client_name);
            break;
        case SESSION_JOIN_CLIENT_LEFT:
            net_signal_emit(conn, SIGNAL_SESSION_CLIENT_LEFT, pkt.client_name);
            free(pkt.client_name);
            break;
        case SESSION_JOIN_OK:
            net_signal_emit(conn, SIGNAL_SESSION_JOINED, pkt.client_name);
            break;
        default:
            err = g_strdup_printf("error receiving session join information: unknown status %d\n", pkt.status);
            net_signal_emit(conn, SIGNAL_ERROR, err);
            g_free(err);
            break;

    }

    return 0;
}

static int recv_cursor_info(connection_t *conn) {
    uint16_t x, y;
    uint8_t cursor;
    char *err;

    if (pkt_recv_cursorinfo(conn->socket, &x, &y, &cursor)) {
        err = g_strdup_printf("error receiving cursor information: %s", strerror(errno));
        net_signal_emit(conn, SIGNAL_ERROR, err);
        g_free(err);
        return -1;
    }
    pkt_cursor_info_t info = {
        .x = x,
        .y = y,
        .cursor = cursor,
    };
    net_signal_emit(conn, SIGNAL_CURSOR_UPDATE, &info);
    return 0;
}

static int recv_screenshare_start(connection_t *conn) {
    uint16_t width, height;
    char *err;
    if (pkt_recv_session_screenshare_start_request(conn->socket, &width, &height)) {
        err = g_strdup_printf("error receiving screenshare start: %s", strerror(errno));
        net_signal_emit(conn, SIGNAL_ERROR, err);
        g_free(err);
        return -1;
    }

    screensize_t rect = {
            .width = width,
            .height = height,
    };

    net_signal_emit(conn, SIGNAL_SCREEN_SHARE_START, &rect);
    return 0;
}

static int recv_framebuffer_update(connection_t *conn) {
    framebuffer_update_t *update;
    char *err;

    if (pkt_recv_framebuffer_update(conn->socket, &update) != 0) {
        err = g_strdup_printf("error receiving framebuffer update: %s", strerror(errno));
        net_signal_emit(conn, SIGNAL_ERROR, err);
        g_free(err);
        return -1;
    }
    return 0;
}

static gboolean conn_reconnect(connection_t *conn) {



    return FALSE;
}

static gboolean data_available(GIOChannel *source, GIOCondition condition, connection_t *conn) {
    size_t nb;
    if (condition & G_IO_ERR) {
        printf("error!\n");
        net_signal_emit(conn, SIGNAL_ERROR, "error reading from socket");
        return FALSE;
    }

    uint8_t type;
    nb = recv(conn->socket, &type, sizeof(type), 0);
    if (nb != sizeof(type)) {
        printf("could not read type: %s\n", strerror(errno));
        return FALSE;
    }

    int ret;

    switch (type) {
        case packet_type_session_join_response:
            ret = recv_join_response(conn);
            break;
        case packet_type_cursor_info:
            ret = recv_cursor_info(conn);
            break;
        case packet_type_session_screenshare_start:
            ret = recv_screenshare_start(conn);
            break;
        case packet_type_framebuffer_update:
            ret = recv_framebuffer_update(conn);
            break;
        default:
            printf("unknown packet type: %d!\n", type);
            ret = -1;
            break;
    }

    if (ret != 0) {
        // Some kind of network error occurred, so we'll remove this handler
        return FALSE;
    }
    return TRUE;
}

connection_t *net_new() {
    connection_t *conn;
    conn = calloc(1, sizeof(connection_t));
    if (conn == NULL) {
        return NULL;
    }

    return conn;
}

void net_free(connection_t *conn) {
    if (conn->hostname != NULL) {
        free(conn->hostname);
    }

    if (conn->port != NULL) {
        free(conn->port);
    }

    if (conn->addr != NULL) {
        freeaddrinfo(conn->addr);
    }

    if (conn->socket != 0) {
        close(conn->socket);
    }

    if (conn->channel != NULL) {
        g_io_channel_shutdown(conn->channel, FALSE, NULL);
    }

    for (int i = 0; i < SIGNAL_UNUSED_MAX; i ++) {
        GList *entry = conn->signal_handlers[i];
        while (entry != NULL) {
            GList *next = entry->next;
            g_free(entry->data);
            conn->signal_handlers[i] = g_list_remove_link(conn->signal_handlers[i], entry);
            entry = next;
        }
    }

    free(conn);
}

/**
 * setup connection to remote host
 *
 * @param[in] conn    connection to connect
 * @param[in] url     url to connect to (hostname:port)
 * @param[out] error  on error, the error string will be saved in this variable
 * @return 0 on success
 */
int net_connect(connection_t *conn, const char *url, char **error) {
    char *ptr;
    int ret;

    conn->hostname = strdup(url);
    if ((ptr = strchr(conn->hostname, ':')) != NULL) {
        *ptr = '\0';
        conn->port = strdup(++ptr);
    } else {
        conn->port = strdup("8999");
    }

    struct addrinfo hints;
    struct addrinfo *p, *res;  // will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    if ((ret = getaddrinfo(conn->hostname, conn->port, &hints, &res)) != 0) {
        if (error != NULL) {
            *error = (char *)gai_strerror(ret);
        }
        return -1;
    }

    void *addr;
    char ipstr[INET6_ADDRSTRLEN];
    conn->addr = res;
    p = res;

    // FIXME - loop through the returned addresses if necessary
    if (p->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        addr = &(ipv4->sin_addr);
    } else if (p->ai_family == AF_INET6) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
        addr = &(ipv6->sin6_addr);
    }
    inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);

    conn->socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    ret = connect(conn->socket, res->ai_addr, res->ai_addrlen);
    if (ret != 0) {
        if (error != NULL) {
            *error = strerror(errno);
        }
        return -1;
    }

    if (conn->channel != NULL) {
        g_io_channel_shutdown(conn->channel, TRUE, NULL);
    }

    conn->channel = g_io_channel_unix_new(conn->socket);
    g_io_channel_set_encoding(conn->channel, NULL, NULL);
    g_io_channel_set_buffered(conn->channel, FALSE);
    g_io_add_watch(conn->channel, G_IO_IN | G_IO_HUP | G_IO_ERR, (GIOFunc)data_available, conn);

    return 0;
}

/**
 * disconnect from remote host
 *
 * @param[in] conn  connection to disconnect from
 * @return 0 on success
 */
int net_disconnect(connection_t *conn) {
    close(conn->socket);
    freeaddrinfo(conn->addr);
    free(conn->hostname);
    free(conn->port);

    return 0;
}

typedef struct {
    int id;
    handlefunc_t fn;
    void *data;
}handler_t;

gpointer net_signal_connect(connection_t *conn, enum signal sig, handlefunc_t fn, void*data) {
    int id = 0;

    if (conn->signal_handlers[sig] != NULL) {
        handler_t *handler = g_list_last(conn->signal_handlers[sig])->data;
        id = handler->id++;
    }

    handler_t *handler = g_new0(handler_t, 1);
    handler->id = id;
    handler->fn = fn;
    handler->data = data;

    if (conn->signal_handlers[sig] == NULL) {
        conn->signal_handlers[sig] = g_list_alloc();
        conn->signal_handlers[sig]->data = handler;
    } else {
        conn->signal_handlers[sig] = g_list_prepend(conn->signal_handlers[sig], handler);
    }

    return handler;
}

void net_signal_disconnect(connection_t *conn, enum signal sig, gpointer handler) {
    conn->signal_handlers[sig] = g_list_remove(conn->signal_handlers[sig], handler);
    g_free(handler);
}

void net_signal_emit(connection_t *conn, enum signal sig, gpointer info) {
    if (conn->signal_handlers[sig] == NULL) {
        return;
    }

    for (GList *entry = conn->signal_handlers[sig]; entry != NULL;) {
        GList *next = entry->next;
        handler_t *handler = (handler_t*)entry->data;

        int ret = handler->fn(info, handler->data);
        if (ret == FALSE) {
            conn->signal_handlers[sig] = g_list_delete_link(conn->signal_handlers[sig], entry);
            g_free(handler);
        }
        entry = next;
    }
}
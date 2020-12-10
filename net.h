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

typedef struct {
    char *hostname;
    char *port;

    struct addrinfo *addr;
    int socket;
} connection_t;

connection_t *net_connect(const char *url, char **error);
int net_disconnect(connection_t *conn);
#endif
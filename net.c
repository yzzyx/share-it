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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "net.h"

/**
 * setup connection to remote host
 *
 * @param[in] url     url to connect to (hostname:port)
 * @param[out] error  on error, the error string will be saved in this variable
 * @return a new connection, or NULL on error
 */
connection_t *net_connect(const char *url, char **error) {
    connection_t *conn;
    char *ptr;
    int ret;

    conn = calloc(1, sizeof(connection_t));
    if (conn == NULL) {
        if (error != NULL) {
            *error = strerror(errno);
        }
        return NULL;
    }

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
        free(conn->hostname);
        free(conn->port);
        free(conn);
        return NULL;
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
        free(conn->hostname);
        free(conn->port);
        free(conn);
        return NULL;
    }

    return conn;
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
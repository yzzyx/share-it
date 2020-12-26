//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef SHAREIT_PACKET_H
#define SHAREIT_PACKET_H
#include <stdint.h>
#include "framebuffer.h"

enum packet_type {
    packet_type_session_join_request = 1,
    packet_type_session_join_response = 2,
    packet_type_session_screenshare_start = 3,
    packet_type_cursor_info = 4,
    packet_type_framebuffer_update = 5,
};

typedef struct {
    uint16_t width;
    uint16_t height;
} screenshare_info_t;

typedef struct {
    uint8_t type;
    uint16_t x;
    uint16_t y;
    uint8_t cursor;
} cursorinfo_t;

enum {
    SESSION_JOIN_OK = 1,
    SESSION_JOIN_NOT_FOUND = 2,
    SESSION_JOIN_INVALID_PASSWORD = 3,
    SESSION_JOIN_CLIENT_JOINED = 4, // A new client has joined the session
    SESSION_JOIN_CLIENT_LEFT = 5, // A client has left the session
} session_join_status;

typedef struct  __attribute__ ((__packed__)) {
    uint8_t status;
    char *client_name;  // Only set if 'status' is SESSION_JOIN_CLIENT_ADDED or SESSION_JOIN_CLIENT_LEFT
}
pkt_session_join_response_t;

int pkt_send_session_screenshare_request (int s, uint16_t width, uint16_t height);
int pkt_recv_session_screenshare_start_request(int s, u_int16_t *width, u_int16_t *height);

int pkt_send_framebuffer_update(int sockfd, z_stream *zlib_send_stream, framebuffer_update_t *update);
int pkt_recv_framebuffer_update(int sockfd, z_stream *zlib_recv_stream, framebuffer_update_t **output);

int pkt_send_cursorinfo(int s, uint16_t x, uint16_t y, uint8_t cursor);
int pkt_recv_cursorinfo(int s, uint16_t *x, uint16_t *y, uint8_t *cursor);

int pkt_send_session_join_request(int s, const char *session_name, const char *password);
int pkt_recv_session_join_request(int s, char **session_name, char **password);

int pkt_send_session_join_response(int s, pkt_session_join_response_t *pkt);
int pkt_recv_session_join_response(int s, pkt_session_join_response_t *pkt);
#endif

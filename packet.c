//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "framebuffer.h"
#include "packet.h"
#include "buf.h"

/**
 * write all data in 'ptr' to socket, in chunks if we have to
 *
 * @param sockfd  socket to send data on
 * @param ptr     pointer to data
 * @param sz      size of data to send
 * @return  < 0 on error, otherwise the number of bytes sent
 */
int send_all(int sockfd, void *ptr, size_t sz) {
    size_t sent = 0;
    size_t ret;
    while (sent < sz) {
        ret = send(sockfd, ptr+sent, sz-sent, 0);
        if (ret < 0) {
            return ret;
        }
        sent += ret;
    }
    return sent;
}

/**
 * read the expected number of bytes from socket, in chunks if we have to
 *
 * @param sockfd  socket to read data from
 * @param ptr     pointer to save data to
 * @param sz      number of bytes to read
 * @return  < 0 on error, otherwise the number of bytes read
 */

int recv_all(int sockfd, void *ptr, size_t sz) {
    size_t read = 0;
    size_t ret;
    while (read < sz) {
        ret = recv(sockfd, ptr+read, sz-read, 0);
        if (ret < 0) {
            return ret;
        }
        read += ret;
    }
    return read;
}

/**
 * read a length-encoded string from socket
 * NOTE! expects the first u8 to be the length of the string
 *
 * @param sockfd  socket to read from
 * @return newly allocated, null terminated string on success, or NULL on failure
 */
char *recv_str(int sockfd) {
    uint8_t len;
    char *str;

    if (recv_all(sockfd, &len, sizeof(len)) < 0) {
        perror("recv_all(len)");
        return NULL;
    }


    str = malloc(len + 1);
    if (str == NULL) {
        perror("malloc()");
        return NULL;
    }

    if (len > 0) {
        if (recv_all(sockfd, str, len) < 0) {
            perror("recv_all(session_name)");
            free(str);
            return NULL;
        }
    }

    str[len] = '\0';
    return str;
}

/**
 * inform server that we want to share our screen in the current session
 *
 * @param s      socket to write to
 * @param width  width of session screen
 * @param height height of session screen
 * @return -1 on error
 */
int pkt_send_session_screenshare_request(int s, uint16_t width, uint16_t height) {
    size_t sz;
    buf_t *b;
    int ret = 0;

    b = buf_new();
    buf_add_uint8(b, packet_type_session_screenshare_start);
    buf_add_uint16(b, width);
    buf_add_uint16(b, height);

    sz = send(s, b->buf, b->len, 0);
    if (sz != b->len) {
        printf("%s: could not send, errno: %s\n", __FUNCTION__, strerror(errno));
        ret = -1;
    }
    buf_free(b);
    return ret;
}

/**
 * read session screenshare start information from socket
 * NOTE! This function expects that the 'type' has already been read from the socket.
 *
 * @param[in] s        socket to read from
 * @param[out] width   width of screen to be displayed
 * @param[out] height  height of screen to be displayed
 * @return  -1 on error
 */
int pkt_recv_session_screenshare_start_request(int s, u_int16_t *width, u_int16_t *height) {
    struct __attribute__ ((__packed__)) {
        uint16_t width;
        uint16_t height;
    }
    pkt;

    if (recv_all(s, &pkt, sizeof(pkt)) <= 0) {
        printf("%s: could not send, errno: %s\n", __FUNCTION__, strerror(errno));
        return -1;
    }
    if (width != NULL) *width = ntohs(pkt.width);
    if (height != NULL) *height = ntohs(pkt.height);
    return 0;
}


/**
 * Send framebuffer update to server
 *
 * @param sockfd  socket to send update on
 * @param update update to send to server
 * @return -1 on error, otherwise 0
 */
int pkt_send_framebuffer_update(int sockfd, framebuffer_update_t *update) {
    buf_t *b;
    framebuffer_rect_t *rect;
    int ret = 0;
    uint8_t *compressed_data = NULL;
    int i;

    b = buf_new();
    buf_add_uint8(b, packet_type_framebuffer_update);
    buf_add_uint8(b, update->n_rects);

    for (i = 0; i < update->n_rects; i ++) {
        rect = update->rects[i];
        buf_add_uint16(b, rect->xpos);
        buf_add_uint16(b, rect->ypos);
        buf_add_uint16(b, rect->width);
        buf_add_uint16(b, rect->height);
        buf_add_uint8(b, rect->encoding_type);

        switch (rect->encoding_type) {
        case framebuffer_encoding_type_raw:
            // 3 bytes per pixel (RGB)
            buf_add_bytes(b,  rect->width * rect->height * 3, (uint8_t *)rect->enc.raw.data);
            break;
        case framebuffer_encoding_type_solid:
            buf_add_uint8(b, rect->enc.solid.red);
            buf_add_uint8(b, rect->enc.solid.green);
            buf_add_uint8(b, rect->enc.solid.blue);
            break;
        default:
            fprintf(stderr, "%s: encoding type %d not implemented!\n", __FUNCTION__, rect->encoding_type);
            buf_free(b);
            return -1;
        }
    }

    if (send_all(sockfd, b->buf, b->len) < 0) {
        ret = -1;
    }
    buf_free(b);
    if (compressed_data) {
        free(compressed_data);
    }
    return ret;
}

/**
 * send the current cursor position and type
 *
 * @param s  socket to write to
 * @param x  x position of cursor
 * @param y  y position of cursor
 * @param cursor id of icon to display as cursor
 * @return -1 on error
 */
int pkt_send_cursorinfo(int s, uint16_t x, uint16_t y, uint8_t cursor) {
    size_t sz;
    buf_t *b;
    int ret = 0;

    b = buf_new();
    buf_add_uint8(b, packet_type_cursor_info);
    buf_add_uint16(b, x);
    buf_add_uint16(b, y);
    buf_add_uint8(b, cursor);

    printf("sending cursor info (%d x %d, %d)\n", x, y, cursor);
    sz = send(s, b->buf, b->len, 0);
    if (sz != b->len) {
        printf("could not send, errno: %s\n", strerror(errno));
        ret = -1;
    }
    buf_free(b);
    return ret;
}

/**
 * read cursor information from socket
 * NOTE! This function expects that the 'type' has already been read from the socket.
 *
 * @param[in] s     socket to read from
 * @param[out] x    x-position of cursor
 * @param[out] y    y-position of cursor
 * @param[out] cursor  icon used for cursor
 * @return -1 on error
 */
int pkt_recv_cursorinfo(int s, uint16_t *x, uint16_t *y, uint8_t *cursor) {
    struct __attribute__ ((__packed__)) {
        uint16_t x;
        uint16_t y;
        uint8_t cursor;
    }
    pkt;

    if (recv_all(s, &pkt, sizeof(pkt)) <= 0) {
        printf("could not send, errno: %s\n", strerror(errno));
        return -1;
    }

    if (x != NULL) *x = ntohs(pkt.x);
    if (y != NULL) *y = ntohs(pkt.y);
    if (cursor != NULL) *cursor = pkt.cursor;
    return 0;
}

/**
 * request to join an existing session
 *
 * @param s  socket to write request to
 * @param session_name  name of session to join
 * @param password      password of session
 * @return  -1 on error
 */
int pkt_send_session_join_request(int s, const char *session_name, const char *password) {
    buf_t *b;
    int ret = 0;

    b = buf_new();
    buf_add_uint8(b, packet_type_session_join_request);
    buf_add_uint8(b, strlen(session_name));
    buf_add_string(b, session_name);
    buf_add_uint8(b, strlen(password));
    buf_add_string(b, password);

    printf("session join:\n");
    buf_dump(b);
    if (send_all(s, b->buf, b->len) <= 0 ) {
        printf("session join: %s\n", strerror(errno));
        ret = -1;
    }
    buf_free(b);
    return ret;
}

/**
 * read session join request from socket
 *
 * @param[in] s   socket to read from
 * @param[out] session_name  name of session to join (must be freed by caller)
 * @param[out] password      password to use (must be freed by caller)
 * @return -1 on error
 */
int pkt_recv_session_join_request(int s, char **session_name, char **password) {
    *session_name = recv_str(s);
    if (*session_name == NULL) {
        return -1;
    }

    *password = recv_str(s);
    if (*password == NULL) {
        return -1;
    }

    return 0;
}


/**
 * send response to session join request
 *
 * @param s   socket to write to
 * @param pkt response information
 * @return
 */
int pkt_send_session_join_response(int s, pkt_session_join_response_t *pkt) {
    buf_t *b = buf_new();

    buf_add_uint8(b, packet_type_session_join_response);
    buf_add_uint8(b, pkt->status);

    if (pkt->status == SESSION_JOIN_CLIENT_JOINED || pkt->status == SESSION_JOIN_CLIENT_LEFT) {
        uint8_t len = strlen(pkt->client_name);
        buf_add_uint8(b, len);
        buf_add_string(b, pkt->client_name);
    }

    int ret = send_all(s, b->buf, b->len);
    buf_free(b);

    if (ret <= 0) {
        return -1;
    }
    return 0;
}

/**
 * read session join response from socket
 * NOTE! This function expects that the 'type' has already been read from the socket.
 *
 * @param s  socket to read from
 * @param pkt  output packet to write to
 * @return -1 on error
 */
int pkt_recv_session_join_response(int s, pkt_session_join_response_t *pkt) {
    uint8_t status;
    if (recv_all(s, &status, sizeof(status)) <= 0) {
        return -1;
    }
    pkt->status = status;

    if (status == SESSION_JOIN_CLIENT_JOINED || status == SESSION_JOIN_CLIENT_LEFT) {
        char *name = recv_str(s);
        if (name == NULL) {
            return -1;
        }
        pkt->client_name = name;
    }

    return 0;
}

/**
 * read framebuffer update from socket
 * NOTE! This function expects that the 'type' has already been read from the socket.
 *
 * @param sockfd            socket to read update from
 * @param zlib_recv_stream  zlib-stream used to recieve information from the other end
 * @param output            will be set to the newly allocated update (must be free'd by caller)
 * @return -1 on error, otherwise 0
 */
int pkt_recv_framebuffer_update(int sockfd, framebuffer_update_t **output) {
    struct __attribute__ ((__packed__)) {
        uint8_t n_rects;
    }
    hdr;

    struct __attribute__ ((__packed__)) {
        uint16_t xpos;
        uint16_t ypos;
        uint16_t width;
        uint16_t height;
        uint8_t encoding_type;
    }
    rect_info;

    if (recv_all(sockfd, &hdr, sizeof(hdr)) < 0) {
        return -1;
    }

    framebuffer_update_t *update = malloc(sizeof(framebuffer_update_t));
    if (update == NULL) {
        return -1;
    }

    update->n_rects = hdr.n_rects;
    update->rects = malloc(sizeof(framebuffer_rect_t *) * update->n_rects);

    int i;
    for (i = 0; i < update->n_rects; i++) {
        if (recv_all(sockfd, &rect_info, sizeof(rect_info)) < 0) {
            return -1;
        }

        framebuffer_rect_t *rect = malloc(sizeof(framebuffer_rect_t));
        if (rect == NULL) {
            return -1;
        }

        rect->xpos = ntohs(rect_info.xpos);
        rect->ypos = ntohs(rect_info.ypos);
        rect->width = ntohs(rect_info.width);
        rect->height = ntohs(rect_info.height);
        rect->encoding_type = rect_info.encoding_type;

        if (rect->encoding_type == framebuffer_encoding_type_raw) {
            size_t sz = rect->width * rect->height * 3;
            uint8_t *data = malloc(sz);
            if (data == NULL) {
                return errno;
            }
            if (recv_all(sockfd, data, sz) < 0) {
                return errno;
            }
            rect->enc.raw.data = data;
        } else if (rect->encoding_type == framebuffer_encoding_type_solid) {
            uint8_t pixel[3];
            if (recv_all(sockfd, pixel, 3) < 0) {
                return errno;
            }
            rect->enc.solid.red = pixel[0];
            rect->enc.solid.green = pixel[1];
            rect->enc.solid.blue = pixel[2];
        } else {
            fprintf(stderr, "%s: unknown encoding %d\n", __FUNCTION__, rect->encoding_type);
            return -1;
        }

        update->rects[i] = rect;
    }

    *output = update;
    return 0;
}
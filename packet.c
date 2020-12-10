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
#include "shareit.h"
#include "framebuffer.h"
#include "packet.h"
#include "compress.h"
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
 * inform server that we're starting a new session
 *
 * @param s      socket to write to
 * @param width  width of session screen
 * @param height height of session screen
 * @return -1 on error
 */
int pkt_send_session_start(int s, uint16_t width, uint16_t height) {
    size_t sz;
    buf_t *b;
    int ret = 0;

    b = buf_new();
    buf_add_uint8(b, packet_type_session_start);
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
 * read session start information from socket
 * NOTE! This function expects that the 'type' has already been read from the socket.
 *
 * @param[in] s        socket to read from
 * @param[out] width   width of screen to be displayed
 * @param[out] height  height of screen to be displayed
 * @return  -1 on error
 */
int pkt_recv_session_start(int s, u_int16_t *width, u_int16_t *height) {
    struct __attribute__ ((__packed__)) {
        uint16_t width;
        uint16_t height;
    }
    pkt;

    if (recv_all(s, &pkt, sizeof(pkt)) <= 0) {
        printf("%s: could not send, errno: %s\n", __FUNCTION__, strerror(errno));
        return -1;
    }
    if (width != NULL) *width = pkt.width;
    if (height != NULL) *height = pkt.height;
    return 0;
}


/**
 * Send framebuffer update to server
 *
 * @param app  main application
 * @param update update to send to server
 * @return -1 on error, otherwise 0
 */
int pkt_send_framebuffer_update(int sockfd, z_stream *zlib_send_stream, framebuffer_update_t *update) {
    buf_t *b;
    framebuffer_rect_t *rect;
    int ret = 0;
    uint8_t *compressed_data;
    size_t compressed_data_sz;
    int i;

    b = buf_new();
    buf_add_uint8(b, packet_type_framebuffer_update);
    buf_add_uint8(b, 0); // padding
    buf_add_uint16(b, update->n_rects);

    for (i = 0; i < update->n_rects; i ++) {
        rect = update->rects[i];
        buf_add_uint16(b, rect->xpos);
        buf_add_uint16(b, rect->ypos);
        buf_add_uint16(b, rect->width);
        buf_add_uint16(b, rect->height);
        buf_add_int32(b, rect->encoding_type);

        switch (rect->encoding_type) {
        case framebuffer_encoding_type_zrle:
            if (compress_zrle(zlib_send_stream, rect, &compressed_data, &compressed_data_sz) == -1) {
                fprintf(stderr, "compress failed!\n");
                return -1;
            }

            if (compressed_data_sz == 0) {
                fprintf(stderr, "compressed size is 0!\n");
                return -1;
            }
            buf_add_uint32(b, compressed_data_sz);
            buf_add_bytes(b, compressed_data_sz, compressed_data);
            break;
        default:
            fprintf(stderr, "encoding type %d not implemented!\n", rect->encoding_type);
            buf_free(b);
            return -1;
        }
    }

    size_t sz;
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

    if (x != NULL) *x = pkt.x;
    if (y != NULL) *y = pkt.y;
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
    size_t sz;
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
    if (send_all(s, pkt, sizeof(pkt_session_join_response_t)) <= 0) {
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
    if (recv_all(s, pkt, sizeof(pkt_session_join_response_t)) <= 0) {
        return -1;
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
int pkt_recv_framebuffer_update(int sockfd, z_stream *zlib_recv_stream, framebuffer_update_t **output) {
    struct __attribute__ ((__packed__)) {
        uint8_t padding;
        uint16_t n_rects;
    }
    hdr;

    struct __attribute__ ((__packed__)) {
        uint16_t xpos;
        uint16_t ypos;
        uint16_t width;
        uint16_t height;
        int32_t encoding_type;
    }
    rect_info;

    if (recv_all(sockfd, &hdr, sizeof(hdr)) < 0) {
        return -1;
    }

    framebuffer_update_t *update = malloc(sizeof(framebuffer_update_t));
    if (update == NULL) {
        return -1;
    }

    update->n_rects = ntohs(hdr.n_rects);
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
        rect->encoding_type = ntohl(rect_info.encoding_type);

        if (rect->encoding_type == framebuffer_encoding_type_zrle) {
            uint32_t compressed_sz;
            if (recv_all(sockfd, &compressed_sz, sizeof(compressed_sz)) < 0) {
                return errno;
            }
            compressed_sz = ntohl(compressed_sz);
            uint8_t *data = malloc(compressed_sz);
            if (data == NULL) {
                return errno;
            }

            if (recv_all(sockfd, data, compressed_sz) < 0) {
                return errno;
            }

            if (decompress_zrle(zlib_recv_stream, rect, data, compressed_sz) != 0) {
                return -1;
            }
        } else {
            fprintf(stderr, "unknown encoding %d\n", rect->encoding_type);
            return -1;
        }

        update->rects[i] = rect;
    }

    *output = update;
    return 0;
}
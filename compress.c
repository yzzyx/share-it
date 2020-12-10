//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#include <stdint.h>
#include <stdlib.h>
#include <zlib.h>
#include <string.h>
#include "shareit.h"
#include "framebuffer.h"
#include "compress.h"
#include "buf.h"

/**
 * initialize the compression for the application.
 *
 * This must be done whenever we have a new connection to the server,
 * because the zlib streams are used throughout the connection.
 *
 * @param app  main application
 * @return returns -1 on error, otherwise 0
 */
int compress_setup(shareit_app_t *app) {
    app->output_stream = calloc(1, sizeof(z_stream));
    int ret = deflateInit(app->output_stream, -1);
    if (ret != Z_OK) {
        return -1;
    }

    app->input_stream = calloc(1, sizeof(z_stream));
    ret = inflateInit(app->input_stream);
    if (ret != Z_OK) {
        return -1;
    }

    return 0;
}

/**
 * performs cleanup of the compression streams
 *
 * @param app  main application
 * @return  returns -1 on error, otherwise 0
 */
int compress_cleanup(shareit_app_t *app) {
    deflateEnd(app->output_stream);
    inflateEnd(app->input_stream);
    return 0;
}

/**
 * compress a ZRLE-encoded rect into output
 *
 * @param[in] compress_stream  stream to use when compressing data
 * @param[in] rect  rect to compress
 * @param[out] output  if successful, output will be updated to point to the compressed data, as a newly allocated buffer
 *                  note that this buffer must be freed after use.
 * @param[out] len  length of compressed data
 * @return  returns -1 on error, otherwise 0
 */
int compress_zrle(z_stream *compress_stream, framebuffer_rect_t *rect, uint8_t **output, size_t *len) {
    uint8_t *compressed_output = NULL;
    size_t compressed_length = 0;
    uint8_t out[CHUNK];
    size_t nb_ready;
    buf_t *b = NULL;
    int ret;

    b = buf_new();
    framebuffer_encoding_zrle_t zrle = rect->enc.zrle;
    buf_add_uint8(b, zrle.sub_encoding);

    // From the documentation:
    // The zlibData when uncompressed represents tiles of 64x64 pixels in left-to-right,
    // top-to-bottom order, similar to hextile. If the width of the rectangle is not an
    // exact multiple of 64 then the width of the last tile in each row is smaller, and
    // if the height of the rectangle is not an exact multiple of 64 then the height of
    // each tile in the final row is smaller

    if (zrle.sub_encoding == rle_encoding_type_solid) {
        buf_add_uint8(b, zrle.solid.red);
        buf_add_uint8(b, zrle.solid.green);
        buf_add_uint8(b, zrle.solid.blue);
        compress_stream->avail_in = b->len;
        compress_stream->next_in = b->buf;
    } else if (zrle.sub_encoding == rle_encoding_type_raw) {
        // FIXME - allow rect to have size != 64x64
        compress_stream->avail_in = rect->width * rect->height * sizeof(uint32_t);
        compress_stream->next_in = (uint8_t *)zrle.raw.data;
    } else {
        fprintf(stderr, "%s: unknown encoding %d\n", __FUNCTION__, zrle.sub_encoding);
        return -1;
    }

    do {
        compress_stream->avail_out = CHUNK;
        compress_stream->next_out = out;
        ret = deflate(compress_stream, Z_SYNC_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            return -1;
        }

        nb_ready = CHUNK - compress_stream->avail_out;
        compressed_output = realloc(compressed_output, compressed_length + nb_ready);
        if (compressed_output == NULL) {
            return -1;
        }

        memcpy(compressed_output + compressed_length, out, nb_ready);
        compressed_length += nb_ready;
    } while (compress_stream->avail_out == 0);

    *output = compressed_output;
    *len = compressed_length;

    if (b != NULL) {
        buf_free(b);
    }
    return 0;
}

int decompress_zrle(z_stream *decompress_stream, framebuffer_rect_t *rect, uint8_t *input, size_t len) {
    uint8_t *uncompressed_output = NULL;
    size_t uncompressed_length = 0;
    uint8_t out[CHUNK];
    size_t nb_ready;
    int ret;

    if (len == 0) {
        return 0;
    }

    decompress_stream->avail_in = len;
    decompress_stream->next_in = input;

    do {
        decompress_stream->avail_out = CHUNK;
        decompress_stream->next_out = out;
        ret = inflate(decompress_stream, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR || ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            fprintf(stderr, "zlib returned error %d\n", ret);
            return -1;
        }

        nb_ready = CHUNK - decompress_stream->avail_out;
        uncompressed_output = realloc(uncompressed_output, uncompressed_length + nb_ready);
        if (uncompressed_output == NULL) {
            perror("malloc()");
            return -1;
        }

        memcpy(uncompressed_output+uncompressed_length, out, nb_ready);
        uncompressed_length += nb_ready;
    } while (decompress_stream->avail_out == 0);


    framebuffer_encoding_zrle_t *zrle = &(rect->enc.zrle);
    zrle->sub_encoding = uncompressed_output[0];

    switch (zrle->sub_encoding) {
    case rle_encoding_type_solid:
        zrle->solid.red = uncompressed_output[1];
        zrle->solid.green = uncompressed_output[2];
        zrle->solid.blue = uncompressed_output[3];
        break;
    case rle_encoding_type_raw:
        zrle->raw.data = malloc(rect->width * rect->height * sizeof(uint32_t));
        if (zrle->raw.data == NULL) {
            perror("malloc(zrle->raw.data)");
            return -1;
        }
        // FIXME - check that we have enough uncompressed output
        memcpy(zrle->raw.data, uncompressed_output, rect->width * rect->height*sizeof(uint32_t));
        break;
    case rle_encoding_type_plain_rle:
    /* fallthrough */
    case rle_encoding_type_palette_rle:
    /* fallthrough */
    case rle_encoding_type_packed_palette:
        fprintf(stderr, "zrle encoding %d is not implemented.", zrle->sub_encoding);
        return -1;
    }

    free(uncompressed_output);
    return 0;
}

int compress_block(uint32_t *block, uint8_t **buf, size_t *len) {
    uint8_t *compressed_output = NULL;
    size_t compressed_length = 0;
    uint8_t out[CHUNK];
    z_stream strm;
    size_t nb_ready;
    int flush;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int ret = deflateInit(&strm, -1);
    if (ret != Z_OK) {
        return -1;
    }

    strm.avail_in = 128*128*4;
    strm.next_in = (uint8_t *)block;
    flush = Z_FINISH;

    do {
        strm.avail_out = CHUNK;
        strm.next_out = out;
        ret = deflate(&strm, flush);
        if (ret == Z_STREAM_ERROR) {
            return -1;
        }

        nb_ready = CHUNK - strm.avail_out;
        compressed_output = realloc(compressed_output, compressed_length + nb_ready);
        if (compressed_output == NULL) {
            return -1;
        }

        memcpy(compressed_output+compressed_length, out, nb_ready);
        compressed_length += nb_ready;
    } while (strm.avail_out == 0);

    deflateEnd(&strm);

    *buf = compressed_output;
    *len = compressed_length;
    return 0;
}

int decompress_block(uint8_t *buf, size_t len, uint32_t **block) {
    uint8_t *uncompressed_output = NULL;
    size_t uncompressed_length = 0;
    uint8_t out[CHUNK];
    z_stream strm;
    size_t nb_ready;
    int flush;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        return -1;
    }

    strm.avail_in = len;
    strm.next_in = buf;
    flush = Z_FINISH;

    do {
        strm.avail_out = CHUNK;
        strm.next_out = out;
        ret = inflate(&strm, flush);
        if (ret == Z_STREAM_ERROR) {
            return -1;
        }

        nb_ready = CHUNK - strm.avail_out;
        uncompressed_output = realloc(uncompressed_output, uncompressed_length + nb_ready);
        if (uncompressed_output == NULL) {
            return -1;
        }

        memcpy(uncompressed_output+uncompressed_length, out, nb_ready);
        uncompressed_length += nb_ready;
    } while (strm.avail_out == 0);

    inflateEnd(&strm);

    *block = (uint32_t *)uncompressed_output;
    return 0;
}

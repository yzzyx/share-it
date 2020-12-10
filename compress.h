//      _                       _ _
//     | |                     (_) |
//  ___| |__   __ _ _ __ ___    _| |_
// / __| '_ \ / _` | '__/ _ \__| | __|
// \__ \ | | | (_| | | |  __/--| | |_
// |___/_| |_|\__,_|_|  \___|  |_|\__|
// Copyright © 2020 Elias Norberg
// Licensed under the GPLv3 or later.
// See COPYING at the root of the repository for details.
#ifndef SHAREIT_COMPRESS_H
#define SHAREIT_COMPRESS_H

#define CHUNK 16384
int compress_setup(shareit_app_t *app);
int compress_cleanup(shareit_app_t *app);

int compress_block(uint32_t *block, uint8_t **buf, size_t *len);
int decompress_block(uint8_t *buf, size_t len, uint32_t **block);

int compress_zrle(z_stream *compress_stream, framebuffer_rect_t *rect, uint8_t **output, size_t *len);
int decompress_zrle(z_stream *decompress_stream, framebuffer_rect_t *rect, uint8_t *input, size_t len);

#endif

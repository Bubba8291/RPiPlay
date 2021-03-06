/**
 * RPiPlay - An open-source AirPlay mirroring server for Raspberry Pi
 * Copyright (C) 2019 Florian Draschbacher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */

/* 
 * H264 renderer using OpenMAX for hardware accelerated decoding
 * on the Raspberry Pi. 
 * Based on the hello_video sample from the Raspberry Pi project.
*/

#ifndef VIDEO_RENDERER_H
#define VIDEO_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "../lib/logger.h"

typedef struct video_renderer_s video_renderer_t;

video_renderer_t *video_renderer_init(logger_t *logger, bool background);
void video_renderer_render_buffer(video_renderer_t *renderer, unsigned char* data, int datalen, uint64_t pts);
void video_renderer_flush(video_renderer_t *renderer);
void video_renderer_destroy(video_renderer_t *renderer);

#ifdef __cplusplus
}
#endif

#endif //VIDEO_RENDERER_H

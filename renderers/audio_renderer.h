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
 * AAC renderer using fdk-aac for decoding and OpenMAX for rendering
*/

#ifndef AUDIO_RENDERER_H
#define AUDIO_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "../lib/logger.h"

typedef enum audio_device_e { AUDIO_DEVICE_HDMI, AUDIO_DEVICE_ANALOG, AUDIO_DEVICE_NONE } audio_device_t;

typedef struct audio_renderer_s audio_renderer_t;

audio_renderer_t *audio_renderer_init(logger_t *logger, audio_device_t device);
void audio_renderer_render_buffer(audio_renderer_t *renderer, unsigned char* data, int datalen);
void audio_renderer_set_volume(audio_renderer_t *renderer, float volume);
void audio_renderer_flush(audio_renderer_t *renderer);
void audio_renderer_destroy(audio_renderer_t *renderer);

#ifdef __cplusplus
}
#endif

#endif //AUDIO_RENDERER_H

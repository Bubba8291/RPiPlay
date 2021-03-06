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

#include "audio_renderer.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "fdk-aac/libAACdec/include/aacdecoder_lib.h"

#include "bcm_host.h"
#include "ilclient.h"
#include "../lib/threads.h"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

struct audio_renderer_s {
    logger_t *logger;

    HANDLE_AACDECODER audio_decoder;

    ILCLIENT_T *client;
    COMPONENT_T *audio_renderer;
    COMPONENT_T *components[2];

    bool first_packet;
};

void audio_renderer_destroy_decoder(audio_renderer_t *renderer) {
    aacDecoder_Close(renderer->audio_decoder);
}

int audio_renderer_init_decoder(audio_renderer_t *renderer) {
    int ret = 0;
    renderer->audio_decoder = aacDecoder_Open(TT_MP4_RAW, 1);
    if (renderer->audio_decoder == NULL) {
        logger_log(renderer->logger, LOGGER_ERR, "aacDecoder open faild!");
        return -1;
    }
    /* ASC config binary data */
    UCHAR eld_conf[] = { 0xF8, 0xE8, 0x50, 0x00 };
    UCHAR *conf[] = { eld_conf };
    static UINT conf_len = sizeof(eld_conf);
    ret = aacDecoder_ConfigRaw(renderer->audio_decoder, conf, &conf_len);
    if (ret != AAC_DEC_OK) {
        logger_log(renderer->logger, LOGGER_ERR, "Unable to set configRaw");
        return -2;
    }
    CStreamInfo *aac_stream_info = aacDecoder_GetStreamInfo(renderer->audio_decoder);
    if (aac_stream_info == NULL) {
        logger_log(renderer->logger, LOGGER_ERR, "aacDecoder_GetStreamInfo failed!");
        return -3;
    }

    logger_log(renderer->logger, LOGGER_DEBUG, "> stream info: channel = %d\tsample_rate = %d\tframe_size = %d\taot = %d\tbitrate = %d",   \
            aac_stream_info->channelConfig, aac_stream_info->aacSampleRate,
           aac_stream_info->aacSamplesPerFrame, aac_stream_info->aot, aac_stream_info->bitRate);
    return 1;
}

void audio_renderer_destroy_renderer(audio_renderer_t *renderer) {
    ilclient_disable_port_buffers(renderer->audio_renderer, 100, NULL, NULL, NULL);

    ilclient_state_transition(renderer->components, OMX_StateIdle);
    ilclient_state_transition(renderer->components, OMX_StateLoaded);
    ilclient_cleanup_components(renderer->components);

    OMX_Deinit();
    ilclient_destroy(renderer->client);
}

int audio_renderer_init_renderer(audio_renderer_t *renderer, audio_device_t device) {
    memset(renderer->components, 0, sizeof(renderer->components));
    renderer->first_packet = true;

    bcm_host_init();

    if ((renderer->client = ilclient_init()) == NULL) {
      return -3;
    }

    if (OMX_Init() != OMX_ErrorNone) {
        ilclient_destroy(renderer->client);
        return -4;
    }

    // Create audio_renderer
    if (ilclient_create_component(renderer->client, &renderer->audio_renderer, "audio_render", 
            ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS) != 0) {
        audio_renderer_destroy_renderer(renderer);
        return -14;
    }
    renderer->components[0] = renderer->audio_renderer;

    // Setup renderer
    // Use PCM
    OMX_AUDIO_PARAM_PORTFORMATTYPE port_format;
    memset(&port_format, 0, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    port_format.nSize = sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE);
    port_format.nVersion.nVersion = OMX_VERSION;
    port_format.nPortIndex = 100;
    port_format.eEncoding = OMX_AUDIO_CodingPCM;
    if (OMX_SetParameter(ilclient_get_handle(renderer->audio_renderer), OMX_IndexParamAudioPortFormat, &port_format) != OMX_ErrorNone) {
        logger_log(renderer->logger, LOGGER_DEBUG, "Could not set pcm format");
        audio_renderer_destroy_renderer(renderer);
        return -13;
    }

    // Fill PCM details
    OMX_AUDIO_PARAM_PCMMODETYPE pcm_mode;
    memset(&pcm_mode, 0, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
    pcm_mode.nSize = sizeof(OMX_AUDIO_PARAM_PCMMODETYPE);
    pcm_mode.nVersion.nVersion = OMX_VERSION;
    pcm_mode.nPortIndex = 100;
    pcm_mode.nChannels = 2;
    pcm_mode.eNumData = OMX_NumericalDataSigned;
    pcm_mode.eEndian = OMX_EndianLittle;
    pcm_mode.nSamplingRate = 44100;
    pcm_mode.bInterleaved = OMX_TRUE;
    pcm_mode.nBitPerSample = 16;
    pcm_mode.ePCMMode = OMX_AUDIO_PCMModeLinear;

    if (OMX_SetConfig(ilclient_get_handle(renderer->audio_renderer), OMX_IndexParamAudioPcm, &pcm_mode) != OMX_ErrorNone) {
        logger_log(renderer->logger, LOGGER_DEBUG, "Could not set pcm config");
        audio_renderer_destroy_renderer(renderer);
        return -13;
    }

    // Set audio device
    const char *device_name = device == AUDIO_DEVICE_HDMI ? "hdmi" : "local";
    OMX_CONFIG_BRCMAUDIODESTINATIONTYPE audio_destination;
    memset(&audio_destination, 0, sizeof(audio_destination));
    audio_destination.nSize = sizeof(OMX_CONFIG_BRCMAUDIODESTINATIONTYPE);
    audio_destination.nVersion.nVersion = OMX_VERSION;
    strcpy((char *)audio_destination.sName, device_name);

    if (OMX_SetConfig(ilclient_get_handle(renderer->audio_renderer), OMX_IndexConfigBrcmAudioDestination, &audio_destination) != OMX_ErrorNone) {
        logger_log(renderer->logger, LOGGER_DEBUG, "Could not set audio device");
        audio_renderer_destroy_renderer(renderer);
        return -14;
    }

    ilclient_change_component_state(renderer->audio_renderer, OMX_StateIdle);
    ilclient_enable_port_buffers(renderer->audio_renderer, 100, NULL, NULL, NULL);
    ilclient_change_component_state(renderer->audio_renderer, OMX_StateExecuting);

    return 1;
}

audio_renderer_t *audio_renderer_init(logger_t *logger, audio_device_t device) {
    audio_renderer_t *renderer;
    renderer = calloc(1, sizeof(audio_renderer_t));
    if (!renderer) {
        return NULL;
    }
    renderer->logger = logger;

    if (audio_renderer_init_decoder(renderer) != 1) {
        free(renderer);
        renderer = NULL;
    }

    if (audio_renderer_init_renderer(renderer, device) != 1) {
        audio_renderer_destroy_decoder(renderer);
        free(renderer);
        renderer = NULL;
    }

    return renderer;
}

#ifdef DUMP_AUDIO
static FILE* file_pcm = NULL;
#endif

void audio_renderer_render_buffer(audio_renderer_t *renderer, unsigned char* data, int datalen) {
    if (datalen == 0) return;

    logger_log(renderer->logger, LOGGER_DEBUG, "Got AAC data of %d bytes", datalen);

    // We assume that every buffer contains exactly 1 frame.

    AAC_DECODER_ERROR error = 0;

    UCHAR *p_buffer[1] = {data};
    UINT buffer_size = datalen;
    UINT bytes_valid = datalen;
    error = aacDecoder_Fill(renderer->audio_decoder, p_buffer, &buffer_size, &bytes_valid);
    if (error != AAC_DEC_OK) {
        logger_log(renderer->logger, LOGGER_ERR, "aacDecoder_Fill error : %x", error);
    }

    INT time_data_size = 4 * 480;
    INT_PCM *p_time_data = malloc(time_data_size); // The buffer for the decoded AAC frames
    error = aacDecoder_DecodeFrame(renderer->audio_decoder, p_time_data, time_data_size, 0);
    if (error != AAC_DEC_OK) {
        logger_log(renderer->logger, LOGGER_ERR, "aacDecoder_DecodeFrame error : 0x%x", error);
    }

#ifdef DUMP_AUDIO
    if (file_pcm == NULL) {
        file_pcm = fopen("/home/pi/Airplay.pcm", "wb");
    }

    fwrite(p_time_data, time_data_size, 1, file_pcm);
#endif


    int offset = 0;
    while (offset < time_data_size) {
        OMX_BUFFERHEADERTYPE *buffer = ilclient_get_input_buffer(renderer->audio_renderer, 100, 1);

        int chunk_size = MIN(time_data_size - offset, buffer->nAllocLen);
        memcpy(buffer->pBuffer, p_time_data, chunk_size);
        offset += chunk_size;

        buffer->nFilledLen = chunk_size;
        buffer->nOffset = 0;
        if (renderer->first_packet) {
            buffer->nFlags = OMX_BUFFERFLAG_STARTTIME;
            renderer->first_packet = 0;
        } else {
            buffer->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;
        }

        if (OMX_EmptyThisBuffer(ILC_GET_HANDLE(renderer->audio_renderer), buffer) != OMX_ErrorNone) {
            logger_log(renderer->logger, LOGGER_ERR, "Audio renderer refused processing buffer");
        }
    }
}

void audio_renderer_set_volume(audio_renderer_t *renderer, float volume) {
    OMX_AUDIO_CONFIG_VOLUMETYPE audio_volume;
    memset(&audio_volume, 0, sizeof(audio_volume));
    audio_volume.nSize = sizeof(OMX_AUDIO_CONFIG_VOLUMETYPE);
    audio_volume.nVersion.nVersion = OMX_VERSION;

    audio_volume.nPortIndex = 100;
    audio_volume.sVolume.nValue = volume * 50.0;

    if (OMX_SetConfig(ilclient_get_handle(renderer->audio_renderer), OMX_IndexConfigAudioVolume, &audio_volume) != OMX_ErrorNone) {
        logger_log(renderer->logger, LOGGER_DEBUG, "Could not set audio volume");
    }
}

void audio_renderer_flush(audio_renderer_t *renderer) {
    // TODO
}

void audio_renderer_destroy(audio_renderer_t *renderer) {
    if (renderer) {
        audio_renderer_flush(renderer);
        audio_renderer_destroy_decoder(renderer);
        audio_renderer_destroy_renderer(renderer);
        free(renderer);
    }
}

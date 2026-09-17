#pragma once
#include <alsa/asoundlib.h>
#include <cstdint>
#include <iostream>
#include "public_lib.cxx"

/*! 对 buffer 平滑进入 */
void effect_smooth_in(PCM_INFO pcm_info, unsigned long buffer_size, char *pcm_buf)
{
    uint8_t sample_size;
    switch (pcm_info.format) {
        case SND_PCM_FORMAT_S8:
            sample_size = 1;
            break;
        case SND_PCM_FORMAT_S16_LE:
            sample_size = 2;
            break;
        case SND_PCM_FORMAT_S24_3LE:
            sample_size = 3;
            break;
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S32_LE:
            sample_size = 4;
            break;
        default:
            std::cerr << "Unfit format! " << std::endl;
            unsigned long buffer_size = pcm_info.rate / 10;
            return;
    }
    unsigned int processed_bytes = 0;
    unsigned int processed_frames = 0;
    unsigned frame_size = pcm_info.channels * sample_size;
    if (sample_size == 1) {
        while (processed_bytes < buffer_size * frame_size) {
            char *p = pcm_buf + processed_bytes;
            for (uint8_t i = 0; i < pcm_info.channels; i++) {
                int8_t val = static_cast<int8_t>(p[0]);
                double d_val = val / 128.0;
                d_val *= (double)processed_frames / (double)buffer_size;
                val = 128 * d_val;
                memcpy(p, &val, 1);
                processed_bytes += 1;
            }
            processed_frames += 1;
        }
    } else if (sample_size == 2) {
        while (processed_bytes < buffer_size * frame_size) {
            char *p = pcm_buf + processed_bytes;
            for (uint8_t i = 0; i < pcm_info.channels; i++) {
                int16_t val = static_cast<int16_t>(p[0] | (p[1] << 8));
                double d_val = val / 32768.0;
                d_val *= (double)processed_frames / (double)buffer_size;
                val = 32768 * d_val;
                memcpy(p, &val, 2);
                processed_bytes += 2;
            }
            processed_frames += 1;
        }
    } else if (sample_size == 3) {
        while (processed_bytes < buffer_size * frame_size) {
            char *p = pcm_buf + processed_bytes;
            for (uint8_t i = 0; i < pcm_info.channels; i++) {
                int32_t tmp = (p[0] | (p[1] << 8) | (p[2] << 16));
                uint32_t val = (tmp << 8) >> 8;
                double d_val = val / 8388608.0;
                d_val *= (double)processed_frames / (double)buffer_size;
                val = 8388608 * d_val;
                memcpy(p, &val, 3);
                processed_bytes += 3;
            }
            processed_frames += 1;
        }
    } else if (sample_size == 4) {
        while (processed_bytes < buffer_size * frame_size) {
            char *p = pcm_buf + processed_bytes;
            for (uint8_t i = 0; i < pcm_info.channels; i++) {
                int32_t val = 0;
                double d_val;
                if (pcm_info.format == SND_PCM_FORMAT_S24_LE) {
                    int32_t tmp = (p[0] | (p[1] << 8) | (p[2] << 16));
                    val = (tmp << 8) >> 8;
                    d_val = val / 8388608.0;
                } else {
                    val = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
                    d_val = val / 2147483648.0;
                }
                d_val *= (double)processed_frames / (double)buffer_size;
                val = 8388608 * d_val;
                memcpy(p, &val, 4);
                processed_bytes += 4;
            }
            processed_frames += 1;
        }
    }
}

/*! 对 buffer 平滑出去 */
void effect_smooth_out(PCM_INFO pcm_info, unsigned long buffer_size, char *pcm_buf)
{
    uint8_t sample_size;
    switch (pcm_info.format) {
        case SND_PCM_FORMAT_S8:
            sample_size = 1;
            break;
        case SND_PCM_FORMAT_S16_LE:
            sample_size = 2;
            break;
        case SND_PCM_FORMAT_S24_3LE:
            sample_size = 3;
            break;
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S32_LE:
            sample_size = 4;
            break;
        default:
            std::cerr << "Unfit format! " << std::endl;
            unsigned long buffer_size = pcm_info.rate / 10;
            return;
    }
    unsigned int processed_bytes = 0;
    unsigned int processed_frames = 0;
    unsigned frame_size = pcm_info.channels * sample_size;
    if (sample_size == 1) {
        while (processed_bytes < buffer_size * frame_size) {
            char *p = pcm_buf + processed_bytes;
            for (uint8_t i = 0; i < pcm_info.channels; i++) {
                int8_t val = static_cast<int8_t>(p[0]);
                double d_val = val / 128.0;
                d_val *= (double)(buffer_size - processed_frames) / (double)buffer_size;
                val = 128 * d_val;
                memcpy(p, &val, 1);
                processed_bytes += 1;
            }
            processed_frames += 1;
        }
    } else if (sample_size == 2) {
        while (processed_bytes < buffer_size * frame_size) {
            char *p = pcm_buf + processed_bytes;
            for (uint8_t i = 0; i < pcm_info.channels; i++) {
                int16_t val = static_cast<int16_t>(p[0] | (p[1] << 8));
                double d_val = val / 32768.0;
                d_val *= (double)(buffer_size - processed_frames) / (double)buffer_size;
                val = 32768 * d_val;
                memcpy(p, &val, 2);
                processed_bytes += 2;
            }
            processed_frames += 1;
        }
    } else if (sample_size == 3) {
        while (processed_bytes < buffer_size * frame_size) {
            char *p = pcm_buf + processed_bytes;
            for (uint8_t i = 0; i < pcm_info.channels; i++) {
                int32_t tmp = (p[0] | (p[1] << 8) | (p[2] << 16));
                uint32_t val = (tmp << 8) >> 8;
                double d_val = val / 8388608.0;
                d_val *= (double)(buffer_size - processed_frames) / (double)buffer_size;
                val = 8388608 * d_val;
                memcpy(p, &val, 3);
                processed_bytes += 3;
            }
            processed_frames += 1;
        }
    } else if (sample_size == 4) {
        while (processed_bytes < buffer_size * frame_size) {
            char *p = pcm_buf + processed_bytes;
            for (uint8_t i = 0; i < pcm_info.channels; i++) {
                int32_t val = 0;
                double d_val;
                if (pcm_info.format == SND_PCM_FORMAT_S24_LE) {
                    int32_t tmp = (p[0] | (p[1] << 8) | (p[2] << 16));
                    val = (tmp << 8) >> 8;
                    d_val = val / 8388608.0;
                } else {
                    val = p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
                    d_val = val / 2147483648.0;
                }
                d_val *= (double)(buffer_size - processed_frames) / (double)buffer_size;
                val = 8388608 * d_val;
                memcpy(p, &val, 4);
                processed_bytes += 4;
            }
            processed_frames += 1;
        }
    }
}

#pragma once
#include <alsa/asoundlib.h>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "public_lib.cxx"

// 淡入曲线
auto in_curve = [](double num) -> double { return std::sin(num * M_PI_2f64); };
// 淡出曲线
auto out_curve = [](double num) -> double { return std::cos(num * M_PI_2f64); };

/*! 对 buffer 平滑进入 */
void effect_smooth_in(char *&pcm_buf, std::size_t width, snd_pcm_format_t format, uint8_t count, uint8_t count_all)
{
    uint8_t bit_width;
    switch (format) {
        case SND_PCM_FORMAT_S8:
            bit_width = 1;
            break;
        case SND_PCM_FORMAT_S16_LE:
            bit_width = 2;
            break;
        case SND_PCM_FORMAT_S24_3LE:
            bit_width = 3;
            break;
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S32_LE:
            bit_width = 4;
            break;
        default:
            std::cerr << "[ERROR - PCM Effect] Wrong format in effect_smooth_in! " << std::endl;
            break;
    }
    double Q_start = 1.0 - static_cast<double>(count) / static_cast<double>(count_all);
    double Q_step = 1.0 / (static_cast<double>(width) / static_cast<double>(bit_width) * static_cast<double>(count_all));
    char *result = memory_pool.get(width);
    char *p_result = result;
    char *p_pcm_buf = pcm_buf;
    switch (bit_width) {
        case 1:
            for (int i = 0; i < width; i++) {
                int8_t val = static_cast<int8_t>(p_pcm_buf[0]);
                double d_val = val / 128.0;
                d_val *= in_curve(Q_start);
                Q_start += Q_step;
                val = 128 * d_val;
                std::memcpy(p_result, &val, 1);
                p_result += 1;
                p_pcm_buf += 1;
            }
            break;
        case 2:
            for (int i = 0; i < width / 2; i++) {
                int16_t val = static_cast<int16_t>(p_pcm_buf[0] | p_pcm_buf[1] << 8);
                double d_val = val / 32768.0;
                d_val *= in_curve(Q_start);
                Q_start += Q_step;
                val = 32768 * d_val;
                std::memcpy(p_result, &val, 2);
                p_result += 2;
                p_pcm_buf += 2;
            }
            break;
        case 3:
            for (int i = 0; i < width / 3; i++) {
                int32_t temp = static_cast<int32_t>(p_pcm_buf[0] << 8 | (p_pcm_buf[1] << 16) | (p_pcm_buf[2] << 24));
                double d_val = temp / 2147483648.0;
                d_val *= in_curve(Q_start);
                Q_start += Q_step;
                temp = 2147483648.0 * d_val;
                uint32_t val = (temp << 8) >> 8;
                std::memcpy(p_result, &val, 3);
                p_result += 3;
                p_pcm_buf += 3;
            }
            break;
        case 4:
            if (format == SND_PCM_FORMAT_S24_LE) {
                for (int i = 0; i < width / 4; i++) {
                    int32_t temp = static_cast<int32_t>(p_pcm_buf[0] << 8 | (p_pcm_buf[1] << 16) | (p_pcm_buf[2] << 24));
                    double d_val = temp / 2147483648.0;
                    d_val *= in_curve(Q_start);
                    Q_start += Q_step;
                    temp = 2147483648.0 * d_val;
                    uint32_t val = (temp << 8) >> 8;
                    std::memcpy(p_result, &val, 4);
                    p_result += 4;
                    p_pcm_buf += 4;
                }
            } else {
                for (int i = 0; i < width / 4; i++) {
                    int32_t val = p_pcm_buf[0] | (p_pcm_buf[1] << 8) | (p_pcm_buf[2] << 16) | (p_pcm_buf[3] << 24);
                    double d_val = val / 2147483648.0;
                    d_val *= in_curve(Q_start);
                    Q_start += Q_step;
                    val = 8388608 * d_val;
                    std::memcpy(p_result, &val, 4);
                    p_result += 4;
                    p_pcm_buf += 4;
                }
            }
            break;
        default:
            break;
    }
    memory_pool.free(result);
}

/*! 对 buffer 平滑出去 */
void effect_smooth_out(char *&pcm_buf, std::size_t width, snd_pcm_format_t format, uint8_t count, uint8_t count_all)
{
    uint8_t bit_width;
    switch (format) {
        case SND_PCM_FORMAT_S8:
            bit_width = 1;
            break;
        case SND_PCM_FORMAT_S16_LE:
            bit_width = 2;
            break;
        case SND_PCM_FORMAT_S24_3LE:
            bit_width = 3;
            break;
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S32_LE:
            bit_width = 4;
            break;
        default:
            std::cerr << "[ERROR - PCM Effect] Wrong format in effect_smooth_in! " << std::endl;
            break;
    }
    double Q_start = static_cast<double>(count) / static_cast<double>(count_all);
    double Q_step = 1.0 / (static_cast<double>(width) / static_cast<double>(bit_width) * static_cast<double>(count_all));
    char *result = memory_pool.get(width);
    char *p_result = result;
    char *p_pcm_buf = pcm_buf;
    switch (bit_width) {
        case 1:
            for (int i = 0; i < width; i++) {
                int8_t val = static_cast<int8_t>(p_pcm_buf[0]);
                double d_val = val / 128.0;
                d_val *= out_curve(Q_start);
                Q_start -= Q_step;
                val = 128 * d_val;
                std::memcpy(p_result, &val, 1);
                p_result += 1;
                p_pcm_buf += 1;
            }
            break;
        case 2:
            for (int i = 0; i < width / 2; i++) {
                int16_t val = static_cast<int16_t>(p_pcm_buf[0] | p_pcm_buf[1] << 8);
                double d_val = val / 32768.0;
                d_val *= out_curve(Q_start);
                Q_start -= Q_step;
                val = 32768 * d_val;
                std::memcpy(p_result, &val, 2);
                p_result += 2;
                p_pcm_buf += 2;
            }
            break;
        case 3:
            for (int i = 0; i < width / 3; i++) {
                int32_t temp = static_cast<int32_t>(p_pcm_buf[0] << 8 | (p_pcm_buf[1] << 16) | (p_pcm_buf[2] << 24));
                double d_val = temp / 2147483648.0;
                d_val *= out_curve(Q_start);
                Q_start -= Q_step;
                temp = 2147483648.0 * d_val;
                uint32_t val = (temp << 8) >> 8;
                std::memcpy(p_result, &val, 3);
                p_result += 3;
                p_pcm_buf += 3;
            }
            break;
        case 4:
            if (format == SND_PCM_FORMAT_S24_LE) {
                for (int i = 0; i < width / 4; i++) {
                    int32_t temp = static_cast<int32_t>(p_pcm_buf[0] << 8 | (p_pcm_buf[1] << 16) | (p_pcm_buf[2] << 24));
                    double d_val = temp / 2147483648.0;
                    d_val *= out_curve(Q_start);
                    Q_start -= Q_step;
                    temp = 2147483648.0 * d_val;
                    uint32_t val = (temp << 8) >> 8;
                    std::memcpy(p_result, &val, 4);
                    p_result += 4;
                    p_pcm_buf += 4;
                }
            } else {
                for (int i = 0; i < width / 4; i++) {
                    int32_t val = p_pcm_buf[0] | (p_pcm_buf[1] << 8) | (p_pcm_buf[2] << 16) | (p_pcm_buf[3] << 24);
                    double d_val = val / 2147483648.0;
                    d_val *= out_curve(Q_start);
                    Q_start -= Q_step;
                    val = 8388608 * d_val;
                    std::memcpy(p_result, &val, 4);
                    p_result += 4;
                    p_pcm_buf += 4;
                }
            }
            break;
        default:
            break;
    }
    memory_pool.free(result);
}

/*! s16le 转换为 s32le，这里 pcm_buf 占容器一半大小(size) */
void super_s16le(char *&pcm_buf, std::size_t size)
{
    char *result = memory_pool.get(size * 2);
    char *p_result = result;
    char *p_pcm_buf = pcm_buf;
    for (std::size_t i = 0; i < size / 2; i++) {
        p_result[2] = p_pcm_buf[0];
        p_result[3] = p_pcm_buf[1];
        p_result += 4;
        p_pcm_buf += 2;
    }
    std::memcpy(pcm_buf, result, size * 2);
    memory_pool.free(result);
}

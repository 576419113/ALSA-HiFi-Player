#pragma once
#include <alsa/asoundlib.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <ostream>
#include <string>
#include <unistd.h>
#include "public_lib.cxx"

/*! xrun 恢复函数 */
static int xrun_recovery(snd_pcm_t *handle, int err)
{
    if (err == -EPIPE) {
        err = snd_pcm_prepare(handle);
        if (err < 0) {
            std::cerr << "Can't recovery from underrun, prepare failed:" << snd_strerror(err) << std::endl;
        }
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN) {
            sleep(1);
        }
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0) {
                std::cerr << "Can't recovery from suspend, prepare failed: " << snd_strerror(err) << std::endl;
            }
        }
        return 0;
    }
    return err;
}

/*! ALSA 播放类 */
class AlsaPlayback
{
private:
    int err;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    void close();
    PCM_INFO pcm_info;
    std::ifstream audio_file;
    unsigned long int buffer_size;
    unsigned long int period_size;
    bool device_opened;
    bool params_filled;
public:
    AlsaPlayback(const std::string &device);
    ~AlsaPlayback();
    void set_params(const PCM_INFO &_pcm_info);
    void playback();
    std::size_t get_period_size();
};

/*! 构造函数，打开音频设备 */
AlsaPlayback::AlsaPlayback(const std::string &device)
{
    device_opened = false;
    params_filled = false;
    if ((err = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) != 0) {
        std::cerr << "[ERROR - Playback] Couldn't open " << device << " device: " << snd_strerror(err) << std::endl;
    } else {
        device_opened = true;
    }
}

/*! 析构函数，关闭音频设备 */
AlsaPlayback::~AlsaPlayback()
{
    close();
}

/*! 安全关闭音频设备 */
void AlsaPlayback::close()
{
    if (handle != nullptr) {
        snd_pcm_drain(handle);
        snd_pcm_hw_free(handle);
        snd_pcm_close(handle);
        handle = nullptr;
    }
    if (audio_file.is_open()) {
        audio_file.close();
    }
    device_opened = false;
    params_filled = false;
    std::cout << "[INFO - Playback] AlsaPlayback destroyed! " << std::endl;
}

/*! 设置参数 */
void AlsaPlayback::set_params(const PCM_INFO &_pcm_info)
{
    if (!device_opened) {
        std::cerr << "[ERROR - Playback] Try to set params, but the device didn't open!" << std::endl;
        return;
    }
    pcm_info = _pcm_info;
    // 分配硬件参数空间
    snd_pcm_hw_params_alloca(&params);
    // 以默认值填充硬件参数
    if ((err = snd_pcm_hw_params_any(handle, params)) < 0) {
        std::cerr << "[ERROR - Playback] Couldn't fill the params: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置访问方式为内存映射
    if ((err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_MMAP_INTERLEAVED)) < 0) {
        std::cerr << "[ERROR - Playback] Failed to set hw_params access: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置采样率
    if ((err = snd_pcm_hw_params_set_rate(handle, params, _pcm_info.rate, 0)) < 0) {
        std::cerr << "[ERROR - Playback] Failed to set hw_params rate: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置通道数
    if ((err = snd_pcm_hw_params_set_channels(handle, params, _pcm_info.channels)) < 0) {
        std::cerr << "[ERROR - Playback] Failed to set hw_params channels: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置格式
    if ((err = snd_pcm_hw_params_set_format(handle, params, _pcm_info.format)) < 0) {
        std::cerr << "[ERROR - Playback] Failed to set hw_params format: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置 buffer size，单位为 frame！
    buffer_size = _pcm_info.rate / 10;
    if ((err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size)) < 0) {
        std::cerr << "[ERROR - Playback] Failed to set hw_params buffer size: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    if ((err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size))) {
        std::cerr << "[ERROR - Playback] Failed to get hw_params buffer size: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置 period size，单位为 frame！
    period_size = buffer_size / 5;
    if ((err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, 0)) < 0) {
        std::cerr << "[ERROR - Playback] Failed to set hw_params period size: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    if ((err = snd_pcm_hw_params_get_period_size(params, &period_size, 0)) < 0) {
        std::cerr << "[ERROR - Playback] Failed to get hw_params period size: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 禁用重采样
    if ((err = snd_pcm_hw_params_set_rate_resample(handle, params, 0)) < 0) {
        std::cerr << "[ERROR - Playback] Failed to disable hw_params resample: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 将参数写入设备
    if ((err = snd_pcm_hw_params(handle, params)) < 0) {
        std::cerr << "[ERROR - Playback] Failed to load hw_params: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    params_filled = true;
}

/*! 播放音乐 */
void AlsaPlayback::playback()
{
    // 检查准备情况
    if (!device_opened) {
        std::cerr << "[ERROR - Playback] Try to play, but the device didn't opened!" << std::endl;
        return;
    }
    if (!params_filled) {
        std::cerr << "[ERROR - Playback] Try to play, but params didn't filled!" << std::endl;
        return;
    }

    // 每一个采样占用字节大小
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
            close();
            return;
    }

    // 输出当前 buffer 与 peroid
    std::cout << "[INFO - Playback] Buffer size: " << buffer_size << " frames. " << std::endl;
    std::cout << "[INFO - Playback] Period size: " << period_size << " frames. " << std::endl;

    // 获取单帧大小
    unsigned long frame_size = snd_pcm_format_physical_width(pcm_info.format) * pcm_info.channels;

    bool started = false;
    while (true) {
        // 从流处理线程获取指针，否则等待 20ms
        char *pcm_buf = nullptr;
        stream2playback.pop(pcm_buf);
        if (!pcm_buf) {
            usleep(20'000);
            continue;
        }

        snd_pcm_uframes_t writed_frames = 0;
        const snd_pcm_channel_area_t *areas;
        snd_pcm_uframes_t offset;
        snd_pcm_uframes_t frames = buffer_size;
        while (writed_frames < frames) {
            // 处理播放前的状态
            snd_pcm_state_t state = snd_pcm_state(handle);
            if (state == SND_PCM_STATE_XRUN) {
                err = xrun_recovery(handle, -EPIPE);
                if (err < 0) {
                    std::cerr << "[ERROR - Playback] XRUN recovery failed: " << snd_strerror(err) << std::endl;
                    close();
                    return;
                }
            } else if (state == SND_PCM_STATE_SUSPENDED) {
                err = xrun_recovery(handle, -ESTRPIPE);
                if (err < 0) {
                    std::cerr << "[ERROR - Playback] SUSPEND recovery failed: " << snd_strerror(err) << std::endl;
                    close();
                    return;
                }
            }
            snd_pcm_sframes_t avail = snd_pcm_avail_update(handle);
            if (avail < 0) {
                err = xrun_recovery(handle, avail);
                if (err < 0) {
                    std::cerr << "[ERROR - Playback] avail update failed: " << snd_strerror(err) << std::endl;
                    close();
                    return;
                }
                continue;
            }
            if (avail == 0) {
                snd_pcm_wait(handle, 1000);
                continue;
            }
            if (avail < period_size) {
                continue;
            }

            // 读取 pcm 到映射内存
            snd_pcm_uframes_t to_write = avail;
            snd_pcm_mmap_begin(handle, &areas, &offset, &to_write);
            if (to_write == 0) {
                std::cerr << "[ERROR - Playback] No frames!" << std::endl;
                continue;
            }
            char *mmap_buf = (char *)areas[0].addr + offset * frame_size;
            memcpy(mmap_buf, pcm_buf + writed_frames * frame_size, to_write * frame_size);
            snd_pcm_mmap_commit(handle, offset, to_write);

            // 仅需启动一次
            if (!started) {
                err = snd_pcm_start(handle);
                if (err < 0) {
                    std::cerr << "[ERROR - Playback] Start error: " << snd_strerror(err) << std::endl;
                    close();
                    return;
                }
                started = true;
            }
            writed_frames += to_write;
        }
    }
}

/*! 向流处理进程返回 period size */
std::size_t AlsaPlayback::get_period_size()
{
    return period_size * snd_pcm_format_physical_width(pcm_info.format) * pcm_info.channels;
}

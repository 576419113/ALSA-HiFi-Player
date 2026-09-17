#pragma once
#include <algorithm>
#include <alsa/asoundlib.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <ostream>
#include <unistd.h>
#include <thread>
#include <vector>
#include "public_lib.cxx"
#include "pcm_effect.cxx"

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
class AlsaPlayback: public std::enable_shared_from_this<AlsaPlayback>
{
private:
    int err;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    void close();
    void _playback();
    PCM_INFO pcm_info;
    std::ifstream audio_file;
    unsigned long int buffer_size;
    unsigned long int period_size;
    bool device_opened;
    bool params_filled;
    bool pcm_opened;
public:
    AlsaPlayback(const std::string &device);
    static std::shared_ptr<AlsaPlayback> create(const std::string &device);
    ~AlsaPlayback();
    void set_params(const PCM_INFO &_pcm_info);
    void open_pcm(std::string file_name);
    void playback();
};

/*! 构造函数，打开音频设备 */
AlsaPlayback::AlsaPlayback(const std::string &device)
{
    device_opened = false;
    params_filled = false;
    pcm_opened = false;
    if ((err = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) != 0) {
        std::cerr << "Couldn't open " << device << " device: " << snd_strerror(err) << std::endl;
    } else {
        device_opened = true;
    }
}

std::shared_ptr<AlsaPlayback> AlsaPlayback::create(const std::string &device)
{
    return std::make_shared<AlsaPlayback>(device);
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
    pcm_opened = false;
    std::cout << "AlsaPlayback destroyed! " << std::endl;
}

/*! 设置参数 */
void AlsaPlayback::set_params(const PCM_INFO &_pcm_info)
{
    if (!device_opened) {
        std::cerr << "Try to set params, but the device didn't open!" << std::endl;
        return;
    }
    pcm_info = _pcm_info;
    // 分配硬件参数空间
    snd_pcm_hw_params_alloca(&params);
    // 以默认值填充硬件参数
    if ((err = snd_pcm_hw_params_any(handle, params)) < 0) {
        std::cerr << "Couldn't fill the params: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置访问方式为内存映射
    if ((err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_MMAP_INTERLEAVED)) < 0) {
        std::cerr << "Failed set hw_params access: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置采样率/*! PCM 信息 */
    struct PCM_INFO
    {
        unsigned int rate;
        unsigned int channels;
        snd_pcm_format_t format;
    };
    if ((err = snd_pcm_hw_params_set_rate(handle, params, _pcm_info.rate, 0)) < 0) {
        std::cerr << "Failed set hw_params rate: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置通道数
    if ((err = snd_pcm_hw_params_set_channels(handle, params, _pcm_info.channels)) < 0) {
        std::cerr << "Failed set hw_params channels: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置格式
    if ((err = snd_pcm_hw_params_set_format(handle, params, _pcm_info.format)) < 0) {
        std::cerr << "Failed set hw_params format: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置 buffer size，单位为 frame！
    buffer_size = _pcm_info.rate / 10;
    if ((err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size)) < 0) {
        std::cerr << "Failed set hw_params buffer size: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    if ((err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size))) {
        std::cerr << "Failed get hw_params buffer size: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 设置 period size，单位为 frame！
    period_size = buffer_size / 5;
    if ((err = snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, 0)) < 0) {
        std::cerr << "Failed set hw_params period size: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    if ((err = snd_pcm_hw_params_get_period_size(params, &period_size, 0)) < 0) {
        std::cerr << "Failed get hw_params period size: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 禁用重采样
    if ((err = snd_pcm_hw_params_set_rate_resample(handle, params, 0)) < 0) {
        std::cerr << "Failed set hw_params resample: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    // 将参数写入设备
    if ((err = snd_pcm_hw_params(handle, params)) < 0) {
        std::cerr << "Failed load hw_params: " << snd_strerror(err) << std::endl;
        close();
        return;
    }
    params_filled = true;
}

/*! 打开 PCM 文件 */
void AlsaPlayback::open_pcm(std::string name)
{
    audio_file.open(name, std::ios::binary | std::ios::in);
    if (!audio_file) {
        std::cerr << "Couldn't open audio file! " << std::endl;
        return;
    }
    pcm_opened = true;
}

/*! 播放音乐 */
void AlsaPlayback::playback()
{
    auto self = shared_from_this();
    std::thread([self]() { self->_playback(); }).detach();
}

void AlsaPlayback::_playback()
{
    // 检查准备情况
    if (!device_opened) {
        std::cerr << "Try to play, but the device didn't opened!" << std::endl;
        return;
    }
    if (!params_filled) {
        std::cerr << "Try to play, but params didn't filled!" << std::endl;
        return;
    }
    if (!pcm_opened) {
        std::cerr << "Try to play, but pcm didn't opened!" << std::endl;
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
    // 获取到的控制序列
    PlayControl geted_control = PlayControl::Nothing;
    // 是否已经启动
    bool is_start = false;
    // 是否在播放中
    bool is_play = false;
    // 对当前 buffer 淡入
    bool smooth_in = false;
    // 对当前 buffer 淡出
    bool smooth_out = false;
    // 当前帧结束后结束播放
    bool to_destroy = false;
    // 单帧大小
    snd_pcm_uframes_t frame_size = pcm_info.channels * sample_size;
    // 文件是否读取完毕
    bool file_over = false;
    // 是否已经开始播放
    bool started = false;

    std::cout << "Buffer size: " << buffer_size << std::endl;
    std::cout << "Period size: " << period_size << std::endl;

    while (!file_over && !to_destroy) {
        // 对控制序列进行处理
        geted_control = PlayControl::Nothing;
        control_queue.pop(geted_control);
        switch (geted_control) {
            case PlayControl::Start:
                is_start = true;
                is_play = true;
                std::cout << "Play Control Info: Start playing. " << std::endl;
                break;
            case PlayControl::Play:
                if (!is_start) {
                    std::cerr << "Please start the playback!" << std::endl;
                    break;
                }
                is_play = true;
                if ((err = snd_pcm_pause(handle, 0)) < 0) {
                    std::cerr << "Couldn't resume: " << snd_strerror(err);
                }
                std::cout << "Play Control Info: Consume playing. " << std::endl;
                break;
            case PlayControl::Pause:
                if (!is_start) {
                    std::cerr << "Please start the playback!" << std::endl;
                    break;
                }
                is_play = false;
                std::cout << "Play Control Info: Pause. " << std::endl;
                if ((err = snd_pcm_pause(handle, 1)) < 0) {
                    std::cerr << "Couldn't pause: " << snd_strerror(err);
                }
                break;
            case PlayControl::Stop:
                is_start = false;
                is_play = false;
                std::cout << "Play Control Info: Stop. " << std::endl;
                goto play_end;
            case PlayControl::Nothing:
                break;
        }
        if (!is_start || !is_play) {
            continue;
        }
        snd_pcm_uframes_t writed_frames = 0;
        const snd_pcm_channel_area_t *areas;
        snd_pcm_uframes_t offset;
        snd_pcm_uframes_t frames = buffer_size;
        std::vector<char> pcm_buf(buffer_size * frame_size);
        audio_file.read(pcm_buf.data(), buffer_size * frame_size);
        // 读取一个 buffer
        if (audio_file.gcount() < buffer_size * frame_size) {
            std::fill_n(pcm_buf.data() + audio_file.gcount(), buffer_size * frame_size - audio_file.gcount(), 0);
            file_over = true;
        }

        // 判断淡入淡出合法情况
        if (smooth_in && smooth_out) {
            smooth_in = false;
            smooth_out = false;
            std::cerr << "Smooth in and out is Wrong! " << std::endl;
        }
        // 对淡入进行处理
        if (smooth_in) {
            smooth_in = false;
            effect_smooth_in(pcm_info, buffer_size, pcm_buf.data());
        }
        // 对淡出进行处理
        if (smooth_out) {
            smooth_out = false;
            effect_smooth_out(pcm_info, buffer_size, pcm_buf.data());
        }

        while (writed_frames < frames) {
            // 处理播放前的状态
            snd_pcm_state_t state = snd_pcm_state(handle);
            if (state == SND_PCM_STATE_XRUN) {
                err = xrun_recovery(handle, -EPIPE);
                if (err < 0) {
                    std::cerr << "XRUN recovery failed: " << snd_strerror(err) << std::endl;
                    close();
                    return;
                }
            } else if (state == SND_PCM_STATE_SUSPENDED) {
                err = xrun_recovery(handle, -ESTRPIPE);
                if (err < 0) {
                    std::cerr << "SUSPEND recovery failed: " << snd_strerror(err) << std::endl;
                    close();
                    return;
                }
            }
            snd_pcm_sframes_t avail = snd_pcm_avail_update(handle);
            if (avail < 0) {
                err = xrun_recovery(handle, avail);
                if (err < 0) {
                    std::cerr << "avail update failed: " << snd_strerror(err) << std::endl;
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
                std::cerr << "No frames!" << std::endl;
                continue;
            }
            char *mmap_buf = (char *)areas[0].addr + offset * frame_size;
            memcpy(mmap_buf, pcm_buf.data() + writed_frames * frame_size, to_write * frame_size);
            snd_pcm_mmap_commit(handle, offset, to_write);
            if (!started) {
                err = snd_pcm_start(handle);
                if (err < 0) {
                    std::cerr << "Start error: " << snd_strerror(err) << std::endl;
                    close();
                    return;
                }
                started = true;
            }
            writed_frames += to_write;
        }
    }
play_end:
    close();
    std::cout << "Successfully close the playback. " << std::endl;
}

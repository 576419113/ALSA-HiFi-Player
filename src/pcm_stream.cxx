#pragma once
#include "public_lib.cxx"
#include "pcm_effect.cxx"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>

/*! 流处理线程 */
class PCMStream: public std::enable_shared_from_this<PCMStream>
{
private:
    std::ifstream audio_file {};
    std::atomic<bool> switch_file = false;
    std::string file_path;
    std::size_t file_size;
    std::size_t period_size;
    snd_pcm_format_t pcm_format;
    void _stream_process();
public:
    PCMStream();
    ~PCMStream();
    void load_pcm(std::string path, std::size_t size, snd_pcm_format_t format);
    void stream_process();
};

/*! 构造函数，初始化向 playback 传递的缓冲区 */
PCMStream::PCMStream()
{
    Stream2Playback::buffer[0] = memory_pool.get();
    Stream2Playback::buffer[1] = memory_pool.get();
    Stream2Playback::buffer[2] = memory_pool.get();
    Stream2Playback::buffer[3] = memory_pool.get();
}

/*! 析构，释放文件 */
PCMStream::~PCMStream()
{
    if (audio_file.is_open()) {
        audio_file.close();
    }
}

/*! 原子的载入文件 */
void PCMStream::load_pcm(std::string path, std::size_t size, snd_pcm_format_t format)
{
    file_path = path;   // 文件路径
    period_size = size; // 一个 period 字节数
    pcm_format = format; // 音频格式
    switch_file.store(true, std::memory_order_release);
    // 重新初始化 Stream2Playback
    memory_pool.resize(Stream2Playback::buffer[0], size);
    memory_pool.resize(Stream2Playback::buffer[1], size);
    memory_pool.resize(Stream2Playback::buffer[2], size);
    memory_pool.resize(Stream2Playback::buffer[3], size);
}

/*! 流处理线程 */
void PCMStream::stream_process()
{
    std::shared_ptr<PCMStream> self = shared_from_this();
    std::thread([self]() {
        self->_stream_process();
    }).detach();
}
void PCMStream::_stream_process()
{
    bool file_opened = false; // 文件是否打开
    bool started = false;     // 是否启动
    bool paused = false;      // 是否暂停
    bool file_end_test = true; // 是否判断即将到达文件末尾，并 smmooth_out
    bool shutdown_process = false; // shutdown 处理
    uint8_t smooth_in_count = 0; // 淡入计数
    uint8_t smooth_in_all = 30; // 淡入总数
    uint8_t smooth_out_count = 0; // 淡出计数
    uint8_t smooth_out_all = 30;  // 淡出总数
    std::size_t end_padding; // 判断即将到达文件尾的标准
    while (true) {
        // 处理文件打开/切换
        if (switch_file.load(std::memory_order_acquire)) {
            if (audio_file.is_open()) {
                audio_file.close();
            }
            audio_file.open(file_path, std::ios::binary | std::ios::in);
            file_opened = true;
            if (!audio_file) {
                std::cerr << "[ERROR - Stream Process] Couldn't open file: " << file_path << ". " << std::endl;
                file_opened = false;
            }
            audio_file.seekg(0, std::ios::end);
            file_size = audio_file.tellg();
            audio_file.seekg(0, std::ios::beg);
            end_padding = period_size * smooth_out_all;
            switch_file.store(false, std::memory_order_release);
        }

        // 处理控制线程发送过来的控制序列
        StreamControl play_control = StreamControl::Nothing;
        control2stream.pop(play_control);
        switch (play_control) {
            case StreamControl::Start:
                started = true;
                smooth_in_count = smooth_in_all;
                std::cout << "[Info - Stream Process] Begin to play. " << std::endl;
                break;
            case StreamControl::Play:
                paused = false;
                smooth_in_count = smooth_in_all;
                std::cout << "[Info - Stream Process] Continue to play. " << std::endl;
                break;
            case StreamControl::Pause:
                paused = true;
                smooth_out_count = smooth_out_all;
                std::cout << "[Info - Stream Process] Paused. " << std::endl;
                break;
            case StreamControl::Stop:
                started = false;
                file_opened = false;
                audio_file.close();
                std::cout << "[Info - Stream Process] Stoped. " << std::endl;
                break;
            case StreamControl::Shutdown:
                paused = true;
                smooth_out_count = smooth_out_all;
                shutdown_process = true;
            default:
                break;
        }

        // 判断当前状态，等待 20ms
        if (!file_opened || !started) {
            usleep(20'000);
            continue;
        }

        // 读取文件的一个 period
        std::size_t w = Stream2Playback::write_index.load(std::memory_order_relaxed);
        while (w - Stream2Playback::read_index.load(std::memory_order_acquire) == 4) {
            // 当前不可写入，等待 20ms
            usleep(20'000);
            continue;
        }
        char *&buf = Stream2Playback::buffer[w & 3];
        if (!paused || smooth_out_count > 0) {
            // 这里处理 16bit -> 32bit 超分
            audio_file.read(buf, period_size / 2);
            super_s16le(buf, period_size / 2);
            if (smooth_in_count > 0) {
                effect_smooth_in(buf, period_size, pcm_format, smooth_in_count, smooth_in_all);
                smooth_in_count--;
            }
            if (smooth_out_count > 0) {
                effect_smooth_out(buf, period_size, pcm_format, smooth_out_count, smooth_out_all);
                smooth_out_count--;
            }
        } else {
            std::fill(buf, buf + period_size, 0);
        }
        Stream2Playback::write_index.store(w + 1, std::memory_order_release);

        // 强制关闭处理
        if (shutdown_process && !smooth_out_count) {
            goto process_end;
        }

        // 文件即将到达结尾，淡出
        if (file_end_test && file_size - audio_file.tellg() < end_padding) {
            smooth_out_count = smooth_out_all;
            file_end_test = false;
        }

        // 文件结束，循环到开头
        if (audio_file.eof()) {
            std::cout << "[INFO - Stream Process] Signle loop. " << std::endl;
            audio_file.clear();
            audio_file.seekg(0, std::ios::beg);
            file_end_test = true;
            smooth_out_count = 0;
            smooth_in_count = smooth_in_all;
        }
    }
process_end:
    std::cout << "[INFO - Stream Process] Exited successfully! " << std::endl;
    stream_process_thread_exit.store(true, std::memory_order_release);
}

#pragma once
#include "public_lib.cxx"
#include "pcm_effect.cxx"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <thread>
#include <unistd.h>
#include <vector>

/*! 流处理线程 */
class PCMStream: public std::enable_shared_from_this<PCMStream>
{
private:
    std::ifstream audio_file {};
    std::atomic<bool> switch_file = false;
    std::string file_path;
    std::size_t period_size;
    void _stream_process();
public:
    PCMStream() {};
    ~PCMStream();
    void load_pcm(std::string path, std::size_t size);
    void stream_process();
};

/*! 原子的载入文件 */
void PCMStream::load_pcm(std::string path, std::size_t size)
{
    file_path = path;   // 文件路径
    period_size = size; // 一个 period 字节数
    switch_file.store(true, std::memory_order_release);
    // 重新初始化 Stream2Playback
    for (auto &vec : Stream2Playback::buffer) {
        vec.resize(period_size);
    }
    Stream2Playback::buffer_size_changed.store(true, std::memory_order_release);
}

/*! 析构，释放文件 */
PCMStream::~PCMStream()
{
    if (audio_file.is_open()) {
        audio_file.close();
    }
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
            switch_file.store(false, std::memory_order_release);
        }

        // 处理控制线程发送过来的控制序列
        StreamControl play_control = StreamControl::Nothing;
        control2stream.pop(play_control);
        switch (play_control) {
            case StreamControl::Start:
                started = true;
                std::cout << "[Info - Stream Process] Begin to play. " << std::endl;
                break;
            case StreamControl::Play:
                paused = false;
                std::cout << "[Info - Stream Process] Continue to play. " << std::endl;
                break;
            case StreamControl::Pause:
                paused = true;
                std::cout << "[Info - Stream Process] Paused. " << std::endl;
                break;
            case StreamControl::Stop:
                started = false;
                std::cout << "[Info - Stream Process] Stoped. " << std::endl;
                break;
            case StreamControl::Shutdown:
                goto process_end;
            default:
                break;
        }

        // 判断当前状态，等待 20ms
        if (!file_opened || !started) {
            usleep(20'000);
            continue;
        }

        // 读取文件的一个 period
        size_t w = Stream2Playback::write_index.load(std::memory_order_relaxed);
        while (w - Stream2Playback::read_index.load(std::memory_order_acquire) == 4) {
            // 缓冲区满，等待 20ms
            usleep(20'000);
            continue;
        }
        if (!paused) {
            // 这里处理 16bit -> 32bit 超分
            audio_file.read(Stream2Playback::buffer[w & 3].data(), period_size);
            //super_s16le(Stream2Playback::buffer[w & 3].data(), period_size / 2);
        } else {
            std::fill(Stream2Playback::buffer[w & 3].begin(), Stream2Playback::buffer[w & 3].end(), 0);
        }
        Stream2Playback::write_index.store(w + 1, std::memory_order_release);
        if (audio_file.gcount() < period_size) {
            std::cout << "[INFO - Stream Process] Signle loop. " << std::endl;
            audio_file.seekg(0, std::ios::beg);
        }
    }
process_end:
    std::cout << "[INFO - Stream Process] Exited successfully! " << std::endl;
    stream_process_thread_exit.store(true, std::memory_order_release);
}

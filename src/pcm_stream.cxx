#pragma once
#include <atomic>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <vector>
#include "public_lib.cxx"

/*! 流处理线程 */
class PCMStream
{
private:
    std::ifstream audio_file {};
    std::atomic<bool> switch_file = false;
    std::string file_path;
    std::size_t period_size;
public:
    PCMStream();
    ~PCMStream();
    void load_pcm(std::string path, std::size_t size);
    void stream_process();
};

/*! 原子的载入文件 */
void PCMStream::load_pcm(std::string path, std::size_t size)
{
    file_path = path; // 文件路径
    period_size = size; // 一个 period 字节数
    switch_file.store(true, std::memory_order_release);
}

/*! 流处理线程 */
void PCMStream::stream_process()
{
    bool file_opened = false; // 文件是否打开
    bool started = false; // 是否启动
    bool paused = false; // 是否暂停
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
        PlayControl play_control = PlayControl::Nothing;
        control2stream.pop(play_control);
        switch (play_control) {
            case PlayControl::Start:
                started = true;
                std::cout << "[Info - Stream Process] Begin to play. " << std::endl;
                break;
            case PlayControl::Play:
                paused = false;
                std::cout << "[Info - Stream Process] Continue to play. " << std::endl;
                break;
            case PlayControl::Pause:
                paused = true;
                std::cout << "[Info - Stream Process] Paused. " << std::endl;
                break;
            case PlayControl::Stop:
                started = false;
                std::cout << "[Info - Stream Process] Stoped. " << std::endl;
                break;
            default:
                break;
        }

        // 判断当前状态，等待 20ms
        if (!file_opened && !started) {
            usleep(20'000);
            continue;
        }

        // 读取文件的一个 period
        std::vector<char> period_buf(period_size, 0);
        if (!paused) {
            audio_file.read(period_buf.data(), period_size);
        }
        // 将一个 period 推送到缓冲区
        stream2playback.push(period_buf.data());
    }
}

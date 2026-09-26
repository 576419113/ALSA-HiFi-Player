#pragma once
#include <alsa/asoundlib.h>
#include <atomic>
#include "spsc_atomic.cxx"
#include "memory_pool.cxx"

// 内存池
MemoryPool memory_pool;

// 主线程向流处理线程发送的控制序列
enum class StreamControl { Nothing, Start, Play, Pause, Stop, Shutdown };
SPSCAtomQueue<StreamControl> control2stream {};
// 流处理线程与播放线程的缓冲区
namespace Stream2Playback
{
alignas(64) std::atomic<size_t> write_index { 0 };
alignas(64) std::atomic<size_t> read_index { 0 };
std::vector<char *> buffer(4);
}

// 主线程确认流处理线程结束
std::atomic<bool> stream_process_thread_exit = false;
// 主线程向播放线程发送退出信号
std::atomic<bool> playback_thread_signal_exit = false;
// 主线程确认播放线程结束
std::atomic<bool> playback_thread_exit = false;

/*! PCM 信息 */
struct PCM_INFO
{
    unsigned int rate;
    unsigned int channels;
    snd_pcm_format_t format;
};

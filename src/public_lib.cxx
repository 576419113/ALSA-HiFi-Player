#pragma once
#include <alsa/asoundlib.h>
#include "spsc_atomic.cxx"

// 主线程向流处理线程发送的控制序列
enum class PlayControl { Nothing, Start, Play, Pause, Stop };
SPSCAtomQueue<PlayControl> control2stream {};
// 流处理线程与播放线程的缓冲区
SPSCAtomBuffer<char *> stream2playback(4);

/*! PCM 信息 */
struct PCM_INFO
{
    unsigned int rate;
    unsigned int channels;
    snd_pcm_format_t format;
};

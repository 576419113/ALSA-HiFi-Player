#pragma once
#include <alsa/asoundlib.h>
#include "spsc_atomic.cxx"

// 主线程向播放线程发送的控制序列
enum PlayControl { Nothing, Start, Play, Pause, Stop };
SPSCAtomQueue<PlayControl> control_queue {};

/*! PCM 信息 */
struct PCM_INFO
{
    unsigned int rate;
    unsigned int channels;
    snd_pcm_format_t format;
};

#include "alsa_playback.cxx"
#include "pcm_stream.cxx"
#include "public_lib.cxx"

#include <alsa/asoundlib.h>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <unistd.h>

bool static sigint = false;

void signalExit(int signum)
{
    std::cout << "[Info - System] Recived SIGINT, send shutdown signal to other threads. " << std::endl;
    sigint = true;
}

int main()
{
    auto playback = std::make_shared<AlsaPlayback>("hw:2,0");
    PCM_INFO pcm_info { 44100, 2, SND_PCM_FORMAT_S32_LE };
    playback->set_params(pcm_info);
    auto pcm_stream = std::make_shared<PCMStream>();
    std::size_t period_size = playback->get_period_size();
    pcm_stream->load_pcm("audio/audio_s16le.pcm", period_size, SND_PCM_FORMAT_S32_LE);
    pcm_stream->stream_process();
    playback->playback();

    // 终端控制
    std::string input = "";
    signal(SIGINT, signalExit);
    while (true) {
        if (sigint) {
            break;
        }
        std::cin >> input;
        if (input.empty()) {
            continue;
        }
        if (input == "start") {
            control2stream.push(StreamControl::Start);
        } else if (input == "play") {
            control2stream.push(StreamControl::Play);
        } else if (input == "pause") {
            control2stream.push(StreamControl::Pause);
        } else if (input == "_stop_test") { // 目前请不要使用此控制
            control2stream.push(StreamControl::Stop);
        } else if (input == "shutdown") {
            goto end;
        } else {
            std::cerr << "[WARN - Control] Wrong control! " << std::endl;
        }
    }
end:
    // 等待流处理线程结束
    control2stream.push(StreamControl::Shutdown);
    std::cout << "[INFO - System] Wait for stream process exit. " << std::endl;
    while (!stream_process_thread_exit.load(std::memory_order_acquire)) {
        usleep(200'000);
    }
    // 等待播放线程结束
    std::cout << "[INFO - System] Wait for playback exit. " << std::endl;
    playback_thread_signal_exit.store(true, std::memory_order_release);
    for (int i = 0; i < 2; i++) {
        size_t w = Stream2Playback::write_index.load(std::memory_order_relaxed);
        while (w - Stream2Playback::read_index.load(std::memory_order_acquire) == 4) {
            // 缓冲区满，等待 20ms
            usleep(20'000);
            continue;
        }
        std::fill(Stream2Playback::buffer[w & 3], Stream2Playback::buffer[w & 3] + period_size, 0);
        Stream2Playback::write_index.store(w + 1, std::memory_order_release);
    }
    while (!playback_thread_exit.load(std::memory_order_acquire)) {
        usleep(200'000);
    }

    return 0;
}

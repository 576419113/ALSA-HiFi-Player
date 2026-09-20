#include <atomic>
#include <cstdlib>
#include <iostream>
#include <csignal>
#include <memory>
#include <unistd.h>
#include "alsa_playback.cxx"
#include "pcm_stream.cxx"
#include "public_lib.cxx"

bool static sigint = false;

void signalExit(int signum)
{
    std::cout << "[Info - System] Recived SIGINT, send shutdown signal to other threads. " << std::endl;
    control2stream.push(StreamControl::Shutdown);
    playback_thread_signal_exit.store(true, std::memory_order_release);
    sigint = true;
}

int main()
{
    auto playback = std::make_shared<AlsaPlayback>("hw:2,0");
    PCM_INFO pcm_info { 44100, 2, SND_PCM_FORMAT_S32_LE };
    playback->set_params(pcm_info);
    auto pcm_stream = std::make_shared<PCMStream>();
    pcm_stream->load_pcm("audio/audio.pcm", playback->get_period_size());
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
        } else if (input == "stop") {
            control2stream.push(StreamControl::Stop);
            break;
        } else if (input == "shutdown") {
            goto end;
        } else {
            std::cerr << "[WARN - Control] Wrong control! " << std::endl;
        }
    }
end:
    // 等待播放线程结束
    std::cout << "[INFO - System] Wait for playback exit. " << std::endl;
    playback_thread_signal_exit.store(true, std::memory_order_release);
    while (!playback_thread_exit.load(std::memory_order_acquire)) {
        usleep(200'000);
    }
    // 等待流处理线程结束
    std::cout << "[INFO - System] Wait for stream process exit. " << std::endl;
    size_t r = Stream2Playback::read_index.load(std::memory_order_relaxed);
    size_t w = Stream2Playback::write_index.load(std::memory_order_acquire);
    Stream2Playback::read_index.store(w, std::memory_order_release);
    control2stream.push(StreamControl::Shutdown);
    while (!stream_process_thread_exit.load(std::memory_order_acquire)) {
        usleep(200'000);
    }
    return 0;
}

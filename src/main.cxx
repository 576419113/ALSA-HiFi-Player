#include <iostream>
#include <csignal>
#include <thread>
#include "alsa_playback.cxx"
#include "pcm_stream.cxx"

bool static sigint = false;

void signalExit(int signum)
{
    std::cout << "[Info - System] Recived SIGINT, send stop signal to playback. " << std::endl;
    control2stream.push(PlayControl::Stop);
    exit(0);
}

int main()
{
    AlsaPlayback playback("hw:2,0");
    PCM_INFO pcm_info { 44100, 2, SND_PCM_FORMAT_S16_LE };
    playback.set_params(pcm_info);
    // 创建流处理线程
    PCMStream pcm_stream;
    pcm_stream.load_pcm("audio/audio.pcm", playback.get_period_size());
    // 创建播放线程
    std::thread playback_thread(&AlsaPlayback::playback, &playback);
    playback_thread.detach();

    std::string input = "";
    signal(SIGINT, signalExit);
    while (true) {
        std::cin >> input;
        if (input.empty()) {
            continue;
        }
        if (input == "start") {
            control2stream.push(PlayControl::Start);
        } else if (input == "play") {
            control2stream.push(PlayControl::Play);
        } else if (input == "pause") {
            control2stream.push(PlayControl::Pause);
        } else if (input == "stop") {
            control2stream.push(PlayControl::Stop);
            break;
        } else {
            std::cout << "Wrong control! " << std::endl;
        }
    }
    return 0;
}

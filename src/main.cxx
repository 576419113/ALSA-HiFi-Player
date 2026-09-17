#include <iostream>
#include <csignal>
#include "alsa_playback.cxx"

bool static sigint = false;

void signalExit(int signum)
{
    std::cout << "Recived SIGINT, send stop signal to playback. " << std::endl;
    control_queue.push(PlayControl::Stop);
    exit(0);
}

int main()
{
    auto playback = AlsaPlayback::create("hw:2,0");
    PCM_INFO pcm_info { 44100, 2, SND_PCM_FORMAT_S16_LE };
    playback->set_params(pcm_info);
    playback->open_pcm("audio/audio.pcm");
    playback->playback();
    std::string input = "";
    signal(SIGINT, signalExit);
    while (true) {
        std::cin >> input;
        if (input.empty()) {
            continue;
        }
        if (input == "start") {
            control_queue.push(PlayControl::Start);
        } else if (input == "play") {
            control_queue.push(PlayControl::Play);
        } else if (input == "pause") {
            control_queue.push(PlayControl::Pause);
        } else if (input == "stop") {
            control_queue.push(PlayControl::Stop);
            break;
        } else {
            std::cout << "Wrong control! " << std::endl;
        }
    }
    return 0;
}

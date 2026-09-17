#pragma once
#include <string>
#include <fstream>
#include "spsc_atomic.cxx"

SPSCAtomQueue<char *> pcm_stream {};

/*! 处理 PCM 数据流的类 */
class PCMStream
{
private:
    std::ifstream pcm_file;
public:
    PCMStream();
    ~PCMStream();
    bool load_file(std::string path);
    bool close_file();
};

/*! 打开 PCM 文件 */
bool PCMStream::load_file(std::string path)
{
    pcm_file.open(path);
    if (!pcm_file) {
        return false;
    }
    return true;
}

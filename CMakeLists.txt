cmake_minimum_required(VERSION 4.0)
project(AlsaPlayer)
# 查找 ALSA
find_package(ALSA REQUIRED)
# 添加源文件
add_executable(alsaplayer src/main.cxx)
add_subdirectory(src)
set(CMAKE_CXX_FLAGS "-g -Og -fstack-protector-all -fstack-check -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC -D_GLIBCXX_ASSERTIONS -fsanitize=address,undefined,leak -fsanitize-address-use-after-scope -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-omit-frame-pointer -fno-optimize-sibling-calls")

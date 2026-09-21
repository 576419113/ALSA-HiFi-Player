cmake_minimum_required(VERSION 4.0)
project(AlsaPlayer)
# 查找 ALSA
find_package(ALSA REQUIRED)
# 添加源文件
add_executable(alsaplayer src/main.cxx)
add_subdirectory(src)
# 编译选项
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -Og -fstack-protector-all -fstack-check -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC -D_GLIBCXX_ASSERTIONS -fsanitize=address,undefined,leak -fsanitize-address-use-after-scope -fsanitize=float-divide-by-zero -fsanitize=float-cast-overflow -fno-omit-frame-pointer -fno-optimize-sibling-calls)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Realse")
    add_compile_options(-O3 -DNDEBUG -fvisibility=hidden -fvisibility-inlines-hidden -fno-rtti -fno-exceptions -fdata-sections -ffunction-sections -flto -Wl,--gc-sections -Wl,--strip-all -s -fmerge-all-constants -fno-unwind-tables -fno-asynchronous-unwind-tables)
endif()

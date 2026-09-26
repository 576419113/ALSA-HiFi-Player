add_library(SPSCAtomic spsc_atomic.cxx)
add_library(AlsaPlayback alsa_playback.cxx)
add_library(PCMEffect pcm_effect.cxx)
add_library(PUBLIC_LIB public_lib.cxx)
add_library(MemoryPool memory_pool.cxx)

target_include_directories(alsaplayer PRIVATE ${ALSA_INCLUDE_DIRS})
target_link_libraries(alsaplayer PRIVATE ${ALSA_LIBRARIES} SPSCAtomic AlsaPlayback PCMEffect PUBLIC_LIB MemoryPool)

cmake_minimum_required(VERSION 3.22)
project(vcam CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# builds libvcam.so ; packaging renames it to zygisk/arm64-v8a.so
add_library(vcam SHARED
    main.cpp
    frame_source.cpp
    camera_hook.cpp
)

target_include_directories(vcam PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Hide everything except the two zygisk_*_entry symbols (marked visible in zygisk.hpp).
target_compile_options(vcam PRIVATE
    -Wall -Wextra -fno-rtti
    -fvisibility=hidden -fvisibility-inlines-hidden
    -ffunction-sections -fdata-sections
)
target_link_options(vcam PRIVATE
    -Wl,--gc-sections
    -Wl,--exclude-libs,ALL
)

find_library(log-lib log)
target_link_libraries(vcam ${log-lib})

# --- NEXT STEP (camera hook): add LSPlant + its inline-hook backend here ---
# git clone the LSPlant + ShadowHook sources in CI, then:
#   add_subdirectory(${LSPLANT_ROOT}/lsplant/src/main/jni lsplant)
#   target_link_libraries(vcam lsplant)
# (not needed for the plumbing milestone — camera_hook.cpp is a stub for now.)

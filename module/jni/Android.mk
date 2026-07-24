LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE     := vcam
LOCAL_SRC_FILES  := main.cpp frame_source.cpp camera_hook.cpp
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_CPPFLAGS   := -std=c++17 -fno-rtti -fvisibility=hidden -Wall -Wextra
LOCAL_LDLIBS     := -llog
# builds libvcam.so ; packaging renames it to zygisk/arm64-v8a.so
include $(BUILD_SHARED_LIBRARY)

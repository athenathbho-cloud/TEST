#pragma once
#include <android/log.h>
#define VLOG_TAG "vcam"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  VLOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  VLOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, VLOG_TAG, __VA_ARGS__)

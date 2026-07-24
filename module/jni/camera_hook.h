#pragma once
#include <jni.h>
namespace vcam {
// Install the camera-frame hook in the target app process (called from postAppSpecialize).
// STUB for now — plumbing milestone first; LSPlant ImageReader hook lands next.
void camera_hook_install(JNIEnv *env);
}

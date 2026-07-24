#include "camera_hook.h"
#include "frame_source.h"
#include "log.h"

namespace vcam {

// ---------------------------------------------------------------------------
// STUB (plumbing milestone). Next step, from the locked recon decision:
//
//   Target : android.media.ImageReader (Java Camera2), YUV_420_888
//   Method : LSPlant hook on Image.getPlanes() (or the OnImageAvailableListener),
//            hooking the FRAMEWORK class (NOT com.facetec.*) to stay off FaceTec's
//            self-integrity radar.
//   Action : on each frame the app pulls, call frame_source_get_latest() and
//            overwrite the plane ByteBuffers, converting NV21 -> YUV_420_888 while
//            honoring each plane's rowStride/pixelStride read at runtime.
//   Deps   : LSPlant (+ its inline-hook backend) added to the build for this step.
//
// For now we just confirm the hook entry point runs and that frames are flowing.
// ---------------------------------------------------------------------------
void camera_hook_install(JNIEnv *env) {
    (void)env;
    LOGI("camera_hook: STUB installed — LSPlant ImageReader hook is the next step");

    // sanity: prove frames are reaching this process from the companion
    static uint8_t probe[640 * 480 * 3 / 2];
    int w = 0, h = 0;
    size_t n = frame_source_get_latest(probe, sizeof(probe), &w, &h);
    if (n) LOGI("camera_hook: have a frame (%dx%d, %zu bytes) ready to inject", w, h, n);
    else   LOGI("camera_hook: no frame yet (companion still warming up) — reader will catch up");
}

} // namespace vcam

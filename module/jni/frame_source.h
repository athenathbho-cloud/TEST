#pragma once
#include <cstdint>
#include <cstddef>

// Frame transport between the root Zygisk companion (reads /data/local/tmp/vcam.nv21)
// and the in-app module (which the camera hook pulls from). Using a companion avoids the
// SELinux/DAC problem: the app process cannot read /data/local/tmp, but the root companion can.
namespace vcam {

// --- module side (runs in the target app process) ---
// Start a background reader that pulls frames from the companion fd into a latest-frame buffer.
void frame_source_start(int companion_fd);
// Copy the latest NV21 frame into dst (up to cap). Returns bytes copied (0 if none yet).
size_t frame_source_get_latest(uint8_t *dst, size_t cap, int *w, int *h);

// --- companion side (runs as root) ---
// Serve frames from vcam.cfg/vcam.nv21 over fd until the client disconnects.
void frame_source_companion_serve(int fd);

} // namespace vcam

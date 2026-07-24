# DESIGN — native module spec

Everything except the **camera hook symbol** is fixed. That one is gated on Stage-0 recon.

## Components (phone side, inside the target-app process)
1. **Zygisk glue** (`main.cpp`) — implements `zygisk::ModuleBase`. In `postAppSpecialize`,
   if the process package == our target, spawn a background thread and install the camera hook.
   Reuse the `zygisk.hpp` from `zygisk-scb`.
2. **frame_source** (`frame_source.cpp`) — supplies the latest raw NV21 frame via a **double-buffered
   ring** (mutex + latest-wins). Two modes (set `source=` in `/data/local/tmp/vcam.cfg`):
   - **`file` (primary, on-device — no PC/adb):** `mmap` `/data/local/tmp/vcam.nv21` (concatenated
     `W*H*3/2`-byte NV21 frames pre-decoded from an mp4 offline) and **loop** it at `fps`. Self-
     contained → nothing tethered during the liveness capture (adb/USB-debug is itself a RASP signal).
   - **`tcp` (live):** connect `127.0.0.1:28080` (adb-reverse tunnel to the PC ffmpeg bridge), read
     frames continuously; reconnect loop; serve last frame if the feed stalls.
   - Frame size (NV21) = `W*H*3/2`; frame count (file mode) = `filesize / framesize`. W/H/fps/mode
     come from `/data/local/tmp/vcam.cfg` so we retune without rebuilding.
3. **camera hook** (`camera_hook.cpp`) — on each frame the app pulls, overwrite the app's buffer
   with the latest ring-buffer frame, converting NV21 → the app's pixel format.

## DECISION — locked by Stage-0 recon on SCB base.apk (2026-07-24)
Recon result (`scratchpad/cam_path_recon.txt`): **no `AImageReader`/`libmediandk`** in any `.so`
→ Candidate A is OUT. DEX shows **Camera2 (Java) + `android.media.ImageReader` + `YUV_420_888`**
(and legacy `android.hardware.Camera` for <9). FaceTec = **NDID** liveness, Production mode.
→ **Target = Candidate B: Java Camera2 `ImageReader` (YUV_420_888) on Android 15**, with legacy
`Camera.onPreviewFrame` (NV21) as a secondary. **Mechanism = Zygisk + LSPlant** (native ART Java
hook, no Frida/gum). **Hook the FRAMEWORK class (`ImageReader`/`Image`), NOT `com.facetec.*`** — so
FaceTec's in-process anti-hook (which guards its OWN classes) has less to catch. Conversion:
ffmpeg **NV21 → YUV_420_888** honoring the plane row/pixel strides read at runtime.

## The hook point — candidates considered (recon picked B)
| Candidate | Where | Hook (native, ShadowHook) | When it's right |
|---|---|---|---|
| **A. NDK AImageReader** | `libmediandk.so` `AImageReader_acquireNextImage` / `_acquireLatestImage` + `AImage_getPlaneData` | pure native — ideal for Zygisk+ShadowHook, no gum threads | app/FaceTec pulls analysis frames via the **NDK** camera |
| **B. Java ImageReader** | `android.media.ImageReader` → native `nativeImageSetup` / the plane `ByteBuffer` | native hook under the Java layer, or LSPlant for the Java method | app uses **Camera2 + Java `ImageReader`** for analysis |
| **C. Preview Surface / SurfaceTexture** | GL producer (`ANativeWindow`/`SurfaceTexture.updateTexImage`) | render our frame into the producer — hardest | frames only go to a **display Surface**, no reader |

**Recon needs (from the client APK + FaceTec SDK):**
- Does it link `libmediandk.so` and call `AImageReader_*`? (→ A)
- Camera2 Java `ImageReader.newInstance(... YUV_420_888 ...)` + `OnImageAvailableListener`? (→ B)
- CameraX `ImageAnalysis`? (wraps ImageReader → B)
- Only a `SurfaceView/TextureView` preview with no analysis reader? (→ C)
- FaceTec SDK version + `initializeInProductionMode` vs `...DevelopmentMode`; the requested capture
  **resolution + format** (sets W/H and the NV21→target conversion).

> Prefer **A** — it's the cleanest native target and matches "no Frida". We confirm against the APK
> before writing the hook so we don't build for the wrong layer.

## Pixel-format conversion
ffmpeg emits **NV21** (Y plane + interleaved VU). The app's `Image` planes are usually
`YUV_420_888` (may be NV21, NV12, or I420 with row/pixel strides). The hook must convert NV21 →
the exact plane layout/stride the app hands us (read `AImage_getPlaneData` strides at runtime;
don't assume tight packing). Keep a `convert_nv21_to(planes, strides)` helper.

## Stealth / detection notes (carry over from SCB)
- Native inline hooks only → **no gum threads**, so ART `SuspendAll` stays cooperative and FaceTec's
  Frida/anti-hook signal is absent.
- Root hidden via **KSU + SUSFS + DenyList** for the target package; SELinux **enforcing**.
- ShadowHook **v1.0.10** on Android 15 (v1.1.1/v2.0.1 = `errno 12 INIT_LINKER`); `module.map` uses
  `/* */` not `//`; setup-ndk `local-cache: false`.
- Honest expectation: FaceTec runs **virtual-camera detection + server-side 3D liveness + attestation**.
  A 2D injected feed is expected to be **rejected** — capturing that rejection *is* the deliverable
  ("reached the camera" ≠ "passed liveness"). Interactive OBS feeds (turn head/blink) only test 2D
  challenge flows, not 3D depth.

## Config file `/data/local/tmp/vcam.cfg` (no rebuild to retune)
```
width=640
height=480
port=28080
format=nv21
```

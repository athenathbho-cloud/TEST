# vcam-zygisk — in-house camera-injection test harness

**Authorised engagement use only.** Open, auditable replacement for the grey-market
`vcamsx.pro` "Video Virtual Camera" — built to validate a client's **FaceTec** liveness
control (PwC Cyber, Bho). Injects an operator-controlled video into a target app's
Camera2 frame path so we can measure whether the SDK detects/rejects it.

## Why we build our own
The vendor tool is a closed-source, root-required binary from a black-market seller
(near-certain telemetry/backdoor) — cannot run it on an engagement device. This harness
reproduces the same injection with tools we control (OBS + ffmpeg + our own Zygisk module).

## Architecture

```
[PC]  OBS ──RTMP──► ffmpeg (rtmp listen, decode → raw NV21) ──rawvideo/TCP──►┐
                                                                             │  adb reverse tcp:28080
[Phone, in target-app process via Zygisk]                                    │
      frame_source (TCP reader) → ring buffer → camera hook → app's Camera2 frames REPLACED
```

- **OBS** = video source (a face clip, an animated/interactive feed for turn-head/blink challenges).
- **ffmpeg** = RTMP server + decoder + raw-frame TCP server. All decoding is on the PC, so the
  injected module stays tiny (no RTMP/codec lib inside the app → less to detect).
- **Zygisk module** = thin in-process hook: read latest frame, convert to the app's pixel format,
  overwrite the camera buffer. Native (ShadowHook) → no Frida/gum threads → beats FaceTec anti-hook.

## Status / staged plan
- [x] **v0 · PC bridge** (`pc-bridge/`) — OBS→ffmpeg→TCP, verifiable today with `test_client.py`.
- [ ] **Stage 0 · Recon** — characterise the client app's camera path (Camera2 `ImageReader` vs
      NDK `AImageReader` vs preview Surface) + FaceTec SDK version/mode. **Locks the hook symbol.**
- [ ] **v0.1 · Static-frame inject** — module replaces camera frames with one test image on a
      throwaway Camera2 app (prove the hook point).
- [ ] **v0.2 · Live frames** — module reads `frame_source` ring buffer → live OBS feed on the test app.
- [ ] **v1.0 · Against the client app** — format/resolution match, root-hide (KSU/SUSFS/denylist),
      run vs FaceTec, document the reaction (expected: **resisted** = control validated).

See `DESIGN.md` for the native-module spec and the recon decision points.

## Quick start — PC bridge (works now, no device build needed)
Prereqs: `ffmpeg` on PATH, `adb`, OBS, Python (`pip install opencv-python numpy pillow` for the test client).

1. Start the bridge (RTMP in :1935 → raw NV21 out :28080, + `adb reverse`):
   ```bash
   python pc-bridge/run_bridge.py --width 640 --height 480 --fps 15
   ```
2. In **OBS** → Settings → Stream → Service *Custom*, Server `rtmp://127.0.0.1:1935/live`,
   Key `test110` → **Start Streaming**.
3. Verify frames arrive (stands in for the phone) — saves `frame.png`:
   ```bash
   python pc-bridge/test_client.py --width 640 --height 480
   ```
   A correct `frame.png` of your OBS scene = the whole PC pipeline works. The Zygisk module
   later connects to the same `127.0.0.1:28080` on the phone via `adb reverse`.

## Build the module (later, via CI — no local NDK)
GitHub Actions (`.github/workflows/build.yml`) builds the arm64 `.so` and packages the KSU/Zygisk
zip, same flow as the SCB `zygisk-scb` module. Native sources land after Stage 0 recon.

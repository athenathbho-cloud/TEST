# -*- coding: utf-8 -*-
"""
PC bridge: OBS --RTMP--> ffmpeg (decode) --raw NV21/TCP--> phone (via adb reverse).
Open replacement for the vendor's lal + VCAM PC side. Authorised engagement use only.

Flow:
  1. `adb reverse tcp:<port> tcp:<port>`  so the phone's 127.0.0.1:<port> tunnels to this PC.
  2. ffmpeg listens for OBS's RTMP push on :1935, decodes, scales to WxH, outputs raw NV21
     frames on a TCP server socket that the phone (or test_client.py) connects to.

Sequence to run:
  python run_bridge.py --width 640 --height 480 --fps 15
  -> then OBS: Server rtmp://127.0.0.1:1935/live  Key test110  -> Start Streaming
  -> then on phone the Zygisk module connects to 127.0.0.1:<port> (or run test_client.py here).
"""
import argparse, subprocess, sys, shutil

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=640)
    ap.add_argument("--height", type=int, default=480)
    ap.add_argument("--fps", type=int, default=15)
    ap.add_argument("--rtmp-port", type=int, default=1935)
    ap.add_argument("--out-port", type=int, default=28080)
    ap.add_argument("--key", default="test110")
    ap.add_argument("--no-adb", action="store_true", help="skip adb reverse (e.g. testing on PC only)")
    a = ap.parse_args()

    if not shutil.which("ffmpeg"):
        sys.exit("[!] ffmpeg not found on PATH. Install a static ffmpeg build first.")

    if not a.no_adb:
        adb = shutil.which("adb") or r"C:\ADB\platform-tools\adb.exe"
        print(f"[*] adb reverse tcp:{a.out_port} tcp:{a.out_port}")
        subprocess.run([adb, "reverse", f"tcp:{a.out_port}", f"tcp:{a.out_port}"])

    rtmp_in = f"rtmp://0.0.0.0:{a.rtmp_port}/live/{a.key}"
    tcp_out = f"tcp://0.0.0.0:{a.out_port}?listen=1"
    cmd = [
        "ffmpeg", "-hide_banner", "-loglevel", "warning",
        "-listen", "1", "-i", rtmp_in,                       # wait for OBS to push
        "-an",
        "-vf", f"scale={a.width}:{a.height},format=nv21",
        "-r", str(a.fps),
        "-f", "rawvideo", "-y", tcp_out,                     # serve raw frames to the phone
    ]
    print("[*] frame size (NV21):", a.width * a.height * 3 // 2, "bytes")
    print("[*] OBS -> Server rtmp://127.0.0.1:%d/live   Key %s" % (a.rtmp_port, a.key))
    print("[*] phone/test_client connects to 127.0.0.1:%d" % a.out_port)
    print("[*] running:", " ".join(cmd))
    print("[*] (ffmpeg waits for OBS first, then for the phone client to connect)")
    try:
        subprocess.run(cmd)
    except KeyboardInterrupt:
        print("\n[*] stopped.")

if __name__ == "__main__":
    main()

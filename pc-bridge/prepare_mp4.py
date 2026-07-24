# -*- coding: utf-8 -*-
"""
Feed an mp4 to the phone for On-Device (file-mode) injection.
Pre-decodes an mp4 to concatenated raw NV21 frames (offline, on PC) and adb-pushes it to
/data/local/tmp/vcam.nv21, plus writes the matching /data/local/tmp/vcam.cfg. The Zygisk module
then mmaps + loops that file into the target app's camera — no PC/adb needed during the actual test.

Authorised engagement use only.

Usage:
  python prepare_mp4.py --in face.mp4 --width 640 --height 480 --fps 15
  python prepare_mp4.py --in face.mp4 --width 640 --height 480 --fps 15 --no-push   # just build .nv21
"""
import argparse, subprocess, sys, shutil, os

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inp", required=True, help="source mp4/mov/etc.")
    ap.add_argument("--width", type=int, default=640)
    ap.add_argument("--height", type=int, default=480)
    ap.add_argument("--fps", type=int, default=15)
    ap.add_argument("--out", default="vcam.nv21")
    ap.add_argument("--no-push", action="store_true", help="build the .nv21 only, don't adb push")
    ap.add_argument("--remote", default="/data/local/tmp/vcam.nv21")
    ap.add_argument("--cfg-remote", default="/data/local/tmp/vcam.cfg")
    a = ap.parse_args()

    if not shutil.which("ffmpeg"):
        sys.exit("[!] ffmpeg not on PATH.")
    if not os.path.exists(a.inp):
        sys.exit(f"[!] input not found: {a.inp}")

    # 1) decode mp4 -> concatenated raw NV21 frames at WxH, fps
    cmd = ["ffmpeg", "-y", "-hide_banner", "-loglevel", "warning",
           "-i", a.inp,
           "-vf", f"scale={a.width}:{a.height},format=nv21",
           "-r", str(a.fps), "-an",
           "-f", "rawvideo", a.out]
    print("[*] decoding:", " ".join(cmd))
    subprocess.run(cmd, check=True)

    fsize = a.width * a.height * 3 // 2
    total = os.path.getsize(a.out)
    frames = total // fsize
    print(f"[+] {a.out}: {total} bytes = {frames} NV21 frames ({a.width}x{a.height}, {a.fps} fps, ~{frames/max(a.fps,1):.1f}s loop)")
    if total % fsize:
        print(f"[!] warning: size not a whole multiple of frame size ({fsize}) — check W/H.")

    if a.no_push:
        print("[*] --no-push: done (build only)."); return

    adb = shutil.which("adb") or r"C:\ADB\platform-tools\adb.exe"
    # 2) push the frames + a matching cfg
    print(f"[*] adb push {a.out} -> {a.remote}")
    subprocess.run([adb, "push", a.out, a.remote], check=True)
    cfg = f"source=file\nfile={a.remote}\nwidth={a.width}\nheight={a.height}\nfps={a.fps}\nformat=nv21\n"
    cfg_local = "vcam.cfg"
    with open(cfg_local, "w", newline="\n") as f:
        f.write(cfg)
    print(f"[*] adb push {cfg_local} -> {a.cfg_remote}")
    subprocess.run([adb, "push", cfg_local, a.cfg_remote], check=True)
    print("[+] done. Enable the module for the target app, reboot, launch — the camera plays this loop.")

if __name__ == "__main__":
    main()

# -*- coding: utf-8 -*-
"""
Stand-in for the phone: connect to the bridge's raw-NV21 TCP socket, read one frame,
save frame.png. If this shows your OBS scene, the entire PC pipeline works and the phone
module (which connects to the same 127.0.0.1:<port> via adb reverse) will get the same frames.
"""
import argparse, socket, sys

def read_exact(sock, n):
    buf = bytearray()
    while len(buf) < n:
        chunk = sock.recv(n - len(buf))
        if not chunk:
            raise IOError("socket closed after %d/%d bytes" % (len(buf), n))
        buf += chunk
    return bytes(buf)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=28080)
    ap.add_argument("--width", type=int, default=640)
    ap.add_argument("--height", type=int, default=480)
    ap.add_argument("--out", default="frame.png")
    a = ap.parse_args()
    fsize = a.width * a.height * 3 // 2  # NV21

    print(f"[*] connecting {a.host}:{a.port}, expecting {fsize}-byte NV21 frames ...")
    s = socket.create_connection((a.host, a.port), timeout=30)
    raw = read_exact(s, fsize)
    s.close()
    print(f"[*] got {len(raw)} bytes")

    try:
        import numpy as np
        yuv = np.frombuffer(raw, np.uint8).reshape((a.height * 3 // 2, a.width))
        try:
            import cv2
            rgb = cv2.cvtColor(yuv, cv2.COLOR_YUV2RGB_NV21)
            cv2.imwrite(a.out, cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR))
            print("[+] saved", a.out, "(cv2)"); return
        except ImportError:
            pass
        # numpy-only NV21 -> RGB (approx)
        y = yuv[:a.height, :].astype(np.int32)
        vu = yuv[a.height:, :].reshape((a.height // 2, a.width // 2, 2))
        v = np.repeat(np.repeat(vu[:, :, 0], 2, 0), 2, 1).astype(np.int32) - 128
        u = np.repeat(np.repeat(vu[:, :, 1], 2, 0), 2, 1).astype(np.int32) - 128
        r = np.clip(y + (91881 * v >> 16), 0, 255)
        g = np.clip(y - ((22554 * u + 46802 * v) >> 16), 0, 255)
        b = np.clip(y + (116130 * u >> 16), 0, 255)
        rgb = np.stack([r, g, b], -1).astype(np.uint8)
        from PIL import Image
        Image.fromarray(rgb, "RGB").save(a.out)
        print("[+] saved", a.out, "(numpy+PIL)")
    except Exception as e:
        with open("frame.nv21", "wb") as f:
            f.write(raw)
        print("[!] no numpy/PIL (%s) -> saved raw frame.nv21 instead" % e)

if __name__ == "__main__":
    main()

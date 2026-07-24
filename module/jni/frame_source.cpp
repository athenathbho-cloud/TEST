#include "frame_source.h"
#include "log.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <thread>
#include <mutex>
#include <vector>
#include <string>

namespace vcam {

// wire header: one per frame (little-endian, native arm64)
struct Hdr { int32_t w, h, fps, size; };

static std::mutex          g_mtx;
static std::vector<uint8_t> g_latest;
static int                 g_w = 0, g_h = 0;

static bool read_full(int fd, void *buf, size_t n) {
    auto *p = static_cast<uint8_t *>(buf); size_t got = 0;
    while (got < n) { ssize_t r = read(fd, p + got, n - got); if (r <= 0) return false; got += (size_t)r; }
    return true;
}
static bool write_full(int fd, const void *buf, size_t n) {
    auto *p = static_cast<const uint8_t *>(buf); size_t put = 0;
    while (put < n) { ssize_t w = write(fd, p + put, n - put); if (w <= 0) return false; put += (size_t)w; }
    return true;
}
static long now_ms() {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

// ------------------------------------------------------------------ module side
static void module_thread(int fd) {
    uint8_t req = 1; long last_log = 0; int cnt = 0;
    for (;;) {
        if (!write_full(fd, &req, 1)) { LOGE("frame_source: request write failed"); break; }
        Hdr h{};
        if (!read_full(fd, &h, sizeof(h))) { LOGE("frame_source: header read failed"); break; }
        if (h.size <= 0 || h.size > 64 * 1024 * 1024) { LOGE("frame_source: bad frame size %d", h.size); break; }
        std::vector<uint8_t> buf((size_t)h.size);
        if (!read_full(fd, buf.data(), (size_t)h.size)) { LOGE("frame_source: frame read failed"); break; }
        {
            std::lock_guard<std::mutex> lk(g_mtx);
            g_latest.swap(buf); g_w = h.w; g_h = h.h;
        }
        cnt++;
        long t = now_ms();
        if (t - last_log > 2000) { LOGI("frame_source: streaming %dx%d, %d frames pulled", h.w, h.h, cnt); last_log = t; }
        int fps = h.fps > 0 ? h.fps : 15;
        usleep((useconds_t)(1000000 / fps));
    }
    close(fd);
}

void frame_source_start(int companion_fd) {
    std::thread(module_thread, companion_fd).detach();
    LOGI("frame_source: module reader started (companion fd=%d)", companion_fd);
}

size_t frame_source_get_latest(uint8_t *dst, size_t cap, int *w, int *h) {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_latest.empty()) return 0;
    size_t n = g_latest.size() < cap ? g_latest.size() : cap;
    memcpy(dst, g_latest.data(), n);
    if (w) *w = g_w; if (h) *h = g_h;
    return n;
}

// -------------------------------------------------------------- companion side
static void read_cfg(int &w, int &h, int &fps, std::string &src, std::string &file) {
    w = 640; h = 480; fps = 15; src = "file"; file = "/data/local/tmp/vcam.nv21";
    FILE *f = fopen("/data/local/tmp/vcam.cfg", "r");
    if (!f) { LOGW("companion: no vcam.cfg -> defaults 640x480@15 file"); return; }
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '='); if (!eq) continue; *eq = 0;
        char *k = line, *v = eq + 1; v[strcspn(v, "\r\n")] = 0;
        if      (!strcmp(k, "width"))  w = atoi(v);
        else if (!strcmp(k, "height")) h = atoi(v);
        else if (!strcmp(k, "fps"))    fps = atoi(v);
        else if (!strcmp(k, "source")) src = v;
        else if (!strcmp(k, "file"))   file = v;
    }
    fclose(f);
}

void frame_source_companion_serve(int fd) {
    int w, h, fps; std::string src, file;
    read_cfg(w, h, fps, src, file);
    if (fps <= 0) fps = 15;
    int framesize = w * h * 3 / 2;                       // NV21
    int ffd = open(file.c_str(), O_RDONLY);
    if (ffd < 0) { LOGE("companion: cannot open %s (%s)", file.c_str(), strerror(errno)); return; }
    struct stat st{}; fstat(ffd, &st);
    size_t total = (size_t)st.st_size;
    void *map = mmap(nullptr, total, PROT_READ, MAP_PRIVATE, ffd, 0);
    if (map == MAP_FAILED) { LOGE("companion: mmap failed"); close(ffd); return; }
    int nframes = framesize > 0 ? (int)(total / framesize) : 0;
    LOGI("companion: serving %s (%zu bytes = %d frames, %dx%d @%dfps)", file.c_str(), total, nframes, w, h, fps);
    if (nframes <= 0) { munmap(map, total); close(ffd); return; }

    const uint8_t *base = static_cast<const uint8_t *>(map);
    Hdr hdr{ w, h, fps, framesize };
    for (;;) {
        uint8_t req;
        if (!read_full(fd, &req, 1)) break;             // client asks for the current frame
        long t = now_ms();
        int idx = (int)((t / (1000 / fps)) % nframes);  // loop by wall-clock -> smooth playback
        if (!write_full(fd, &hdr, sizeof(hdr))) break;
        if (!write_full(fd, base + (size_t)idx * framesize, (size_t)framesize)) break;
    }
    munmap(map, total); close(ffd);
    LOGI("companion: client disconnected");
}

} // namespace vcam

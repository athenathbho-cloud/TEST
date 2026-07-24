#include <jni.h>
#include <cstring>
#include <string>
#include "zygisk.hpp"          // external: drop in the header from your zygisk-scb (see README.txt)
#include "log.h"
#include "frame_source.h"
#include "camera_hook.h"

using namespace zygisk;

// Only act inside these processes. Add your throwaway test app here for bring-up.
static constexpr const char *TARGETS[] = {
    "com.scb.phone_uat_cr",     // SCB Easy UAT (FaceTec / NDID) — the engagement target
    "com.example.vcamtest",     // throwaway Camera2 test app (validate injection first)
};

class VCam : public ModuleBase {
public:
    void onLoad(Api *api, JNIEnv *env) override { api_ = api; env_ = env; }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        is_target_ = false; pkg_.clear();
        if (args && args->nice_name) {
            const char *nm = env_->GetStringUTFChars(args->nice_name, nullptr);
            if (nm) {
                for (auto t : TARGETS) if (std::strcmp(nm, t) == 0) { is_target_ = true; break; }
                if (is_target_) pkg_ = nm;
                env_->ReleaseStringUTFChars(args->nice_name, nm);
            }
        }
        // In every non-target process, unload us so we leave no footprint.
        if (!is_target_) api_->setOption(DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const AppSpecializeArgs *) override {
        if (!is_target_) return;
        LOGI("vcam: injected into target '%s'", pkg_.c_str());
        int fd = api_->connectCompanion();          // root helper that can read /data/local/tmp
        if (fd < 0) { LOGE("vcam: connectCompanion failed"); return; }
        vcam::frame_source_start(fd);
        vcam::camera_hook_install(env_);
    }

private:
    Api *api_ = nullptr;
    JNIEnv *env_ = nullptr;
    bool is_target_ = false;
    std::string pkg_;
};

// Root companion: serves frames from vcam.nv21 (the app process can't read /data/local/tmp itself).
static void vcam_companion(int fd) { vcam::frame_source_companion_serve(fd); }

REGISTER_ZYGISK_MODULE(VCam)
REGISTER_ZYGISK_COMPANION(vcam_companion)

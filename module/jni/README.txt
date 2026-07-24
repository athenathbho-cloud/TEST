External headers/libs this module needs (not vendored here)
===========================================================

1) zygisk.hpp   (PROVIDED — canonical Zygisk API v4, generated 2026-07-24)
   Already in this folder. Compatible with Magisk Zygisk / ZygiskNext / ReZygisk (API 4).
   We only use connectCompanion() + setOption() + the REGISTER macros (stable ABI).
   IF the app crashes on launch or connectCompanion() returns -1, the runtime's api_table
   order differs -> replace zygisk.hpp with the exact one from your zygisk-scb and rebuild.

2) LSPlant   (REQUIRED only for the real camera hook — the NEXT step, not the plumbing milestone)
   Java-method hooking from native (what LSPosed uses). Added as a submodule + its inline-hook
   backend (Dobby or ShadowHook) when we implement camera_hook.cpp against
   android.media.ImageReader / Image.getPlanes().

Build (CI): .github/workflows/build.yml runs ndk-build (arm64) -> libvcam.so, then packages the
module zip (libvcam.so -> zygisk/arm64-v8a.so). Nothing to build locally (no NDK needed).

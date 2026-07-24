// Canonical Zygisk C++ API — version 4.
// Matches topjohnwu's zygisk-module-sample; compatible with Magisk Zygisk, ZygiskNext
// (zygisksu) and ReZygisk (all target API 4). This module only uses connectCompanion(),
// setOption(), and the REGISTER_* macros — all in the stable part of the ABI.
//
// ABI NOTE: the api_table below MUST match the layout your Zygisk implementation fills at
// runtime. If connectCompanion() returns -1 or the app crashes on launch with a bad call,
// the runtime's table order differs — replace this file with the exact zygisk.hpp from your
// Zygisk provider (or your zygisk-scb copy) and rebuild.
#pragma once

#include <jni.h>
#include <sys/types.h>   // dev_t, ino_t
#include <cstdint>

#define ZYGISK_API_VERSION 4

namespace zygisk {

struct Api;
struct AppSpecializeArgs;
struct ServerSpecializeArgs;

class ModuleBase {
public:
    virtual void onLoad([[maybe_unused]] Api *api, [[maybe_unused]] JNIEnv *env) {}
    virtual void preAppSpecialize([[maybe_unused]] AppSpecializeArgs *args) {}
    virtual void postAppSpecialize([[maybe_unused]] const AppSpecializeArgs *args) {}
    virtual void preServerSpecialize([[maybe_unused]] ServerSpecializeArgs *args) {}
    virtual void postServerSpecialize([[maybe_unused]] const ServerSpecializeArgs *args) {}
};

struct AppSpecializeArgs {
    // Required — present on all Android versions.
    jint &uid;
    jint &gid;
    jintArray &gids;
    jint &runtime_flags;
    jobjectArray &rlimits;
    jint &mount_external;
    jstring &se_info;
    jstring &nice_name;
    jstring &instruction_set;
    jstring &app_data_dir;

    // Optional — check for null before dereferencing.
    jintArray *const fds_to_ignore;
    jboolean *const is_child_zygote;
    jboolean *const is_top_app;
    jobjectArray *const pkg_data_info_list;
    jobjectArray *const whitelisted_data_info_list;
    jboolean *const mount_data_dirs;
    jboolean *const mount_storage_dirs;

    AppSpecializeArgs() = delete;
};

struct ServerSpecializeArgs {
    jint &uid;
    jint &gid;
    jintArray &gids;
    jint &runtime_flags;
    jlong &permitted_capabilities;
    jlong &effective_capabilities;

    ServerSpecializeArgs() = delete;
};

namespace internal {
struct api_table;
template <class T> void entry_impl(api_table *, JNIEnv *);
}

enum Option : int {
    FORCE_DENYLIST_UNMOUNT = 0,
    DLCLOSE_MODULE_LIBRARY = 1,
};

enum StateFlag : uint32_t {
    PROCESS_GRANTED_ROOT = (1u << 0),
    PROCESS_ON_DENYLIST  = (1u << 1),
};

struct Api {
    int connectCompanion();
    int getModuleDir();
    void setOption(Option opt);
    uint32_t getFlags();
    bool exemptFd(int fd);
    bool hookJniNativeMethods(JNIEnv *env, const char *className, JNINativeMethod *methods, int numMethods);
    void pltHookRegister(dev_t dev, ino_t inode, const char *symbol, void *newFunc, void **oldFunc);
    bool pltHookCommit();

private:
    internal::api_table *tbl;
    template <class T> friend void internal::entry_impl(internal::api_table *, JNIEnv *);
};

// The entry points MUST be C-linkage + default visibility so Zygisk can dlsym them
// (everything else in the .so stays hidden via -fvisibility=hidden).
#define REGISTER_ZYGISK_MODULE(clazz) \
extern "C" [[gnu::visibility("default")]] [[gnu::used]] \
void zygisk_module_entry(zygisk::internal::api_table *table, JNIEnv *env) { \
    zygisk::internal::entry_impl<clazz>(table, env);                        \
}

#define REGISTER_ZYGISK_COMPANION(func) \
extern "C" [[gnu::visibility("default")]] [[gnu::used]] \
void zygisk_companion_entry(int client) { func(client); }

namespace internal {

struct module_abi {
    long api_version;
    void *impl;

    void (*preAppSpecialize)(void *, AppSpecializeArgs *);
    void (*postAppSpecialize)(void *, const AppSpecializeArgs *);
    void (*preServerSpecialize)(void *, ServerSpecializeArgs *);
    void (*postServerSpecialize)(void *, const ServerSpecializeArgs *);

    module_abi(ModuleBase *module) : api_version(ZYGISK_API_VERSION), impl(module) {
        preAppSpecialize    = [](void *m, AppSpecializeArgs *args)        { static_cast<ModuleBase *>(m)->preAppSpecialize(args); };
        postAppSpecialize   = [](void *m, const AppSpecializeArgs *args)  { static_cast<ModuleBase *>(m)->postAppSpecialize(args); };
        preServerSpecialize = [](void *m, ServerSpecializeArgs *args)     { static_cast<ModuleBase *>(m)->preServerSpecialize(args); };
        postServerSpecialize= [](void *m, const ServerSpecializeArgs *a)  { static_cast<ModuleBase *>(m)->postServerSpecialize(a); };
    }
};

struct api_table {
    // Permanent (never reordered).
    void *impl;
    bool (*registerModule)(api_table *, module_abi *);

    void (*hookJniNativeMethods)(JNIEnv *, const char *, JNINativeMethod *, int);
    void (*pltHookRegister)(dev_t, ino_t, const char *, void *, void **);
    bool (*exemptFd)(int);
    bool (*pltHookCommit)();

    int (*connectCompanion)(void * /* impl */);
    int (*getModuleDir)(void * /* impl */);
    void (*setOption)(void * /* impl */, Option);
    uint32_t (*getFlags)(void * /* impl */);
};

template <class T>
void entry_impl(api_table *table, JNIEnv *env) {
    ModuleBase *module = new T();
    if (!table->registerModule(table, new module_abi(module)))
        return;
    auto *api = new Api();
    api->tbl = table;
    module->onLoad(api, env);
}

} // namespace internal

inline int Api::connectCompanion() {
    return tbl->connectCompanion ? tbl->connectCompanion(tbl->impl) : -1;
}
inline int Api::getModuleDir() {
    return tbl->getModuleDir ? tbl->getModuleDir(tbl->impl) : -1;
}
inline void Api::setOption(Option opt) {
    if (tbl->setOption) tbl->setOption(tbl->impl, opt);
}
inline uint32_t Api::getFlags() {
    return tbl->getFlags ? tbl->getFlags(tbl->impl) : 0;
}
inline bool Api::exemptFd(int fd) {
    return tbl->exemptFd != nullptr && tbl->exemptFd(fd);
}
inline bool Api::hookJniNativeMethods(JNIEnv *env, const char *className, JNINativeMethod *methods, int numMethods) {
    if (tbl->hookJniNativeMethods == nullptr) return false;
    tbl->hookJniNativeMethods(env, className, methods, numMethods);
    return true;
}
inline void Api::pltHookRegister(dev_t dev, ino_t inode, const char *symbol, void *newFunc, void **oldFunc) {
    if (tbl->pltHookRegister) tbl->pltHookRegister(dev, inode, symbol, newFunc, oldFunc);
}
inline bool Api::pltHookCommit() {
    return tbl->pltHookCommit != nullptr && tbl->pltHookCommit();
}

} // namespace zygisk

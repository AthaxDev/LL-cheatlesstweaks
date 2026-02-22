#include <cstdint>
#include <cstring>
#include <sys/mman.h>
#include "pl/Gloss.h"
#include "pl/Signature.h"
#include <android/log.h>
#include "gamerules.h"

#define TAG "CheatlessTweaks"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using GameRules_registerRules_t = void (*)(GameRules*);

static GHook g_registerRules_hook = nullptr;
static GameRules_registerRules_t g_registerRules_orig = nullptr;

static void GameRules_registerRules(GameRules* self) {
    g_registerRules_orig(self);
    self->mGameRules[(int)GameRulesIndex::MobGriefing].mRequiresCheats = false;
    self->mGameRules[(int)GameRulesIndex::PlayerSleepingPercentage].mRequiresCheats = false;
    self->mGameRules[(int)GameRulesIndex::KeepInventory].mRequiresCheats = false;
}

static bool SetupGameRulesHook() {
#if !defined(__aarch64__)
    return false;
#endif

    uintptr_t addr = pl::signature::pl_resolve_signature(
        "?? 7B ?? A9 ?? 03 00 91 ?? ?? ?? A9 ?? ?? ?? A9 ?? ?? ?? A9 ?? ?? ?? A9",
        "libminecraftpe.so"
    );

    if (!addr) {
        addr = pl::signature::pl_resolve_signature(
            "?? 7B ?? A9 ?? 03 00 91 ?? ?? ?? A9 ?? ?? ?? A9 ?? ?? ?? A9",
            "libminecraftpe.so"
        );
    }

    if (!addr) {
        LOGE("GameRules::registerRules signature not found!");
        return false;
    }

    LOGI("GameRules::registerRules addr: %p", (void*)addr);

    g_registerRules_hook = GlossHook(
        reinterpret_cast<void*>(addr),
        reinterpret_cast<void*>(GameRules_registerRules),
        reinterpret_cast<void**>(&g_registerRules_orig)
    );

    if (!g_registerRules_hook || !g_registerRules_orig) {
        LOGE("GlossHook failed!");
        return false;
    }

    LOGI("GameRules::registerRules hooked successfully");
    return true;
}

__attribute__((constructor))
void mod_init() {
    GlossInit(true);

    if (!SetupGameRulesHook())
        LOGE("mod_init: hook setup failed");
    else
        LOGI("mod_init: done");
}

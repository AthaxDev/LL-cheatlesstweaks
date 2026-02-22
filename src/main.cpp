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

void findRequiresCheatsOffset(GameRules* gRules) {
    auto& rules = gRules->mGameRules;

    // requiresCheats = TRUE
    uint8_t* cheat1 = reinterpret_cast<uint8_t*>(&rules[(int)GameRulesIndex::MobGriefing]);   // 14
    uint8_t* cheat2 = reinterpret_cast<uint8_t*>(&rules[(int)GameRulesIndex::KeepInventory]); // 13

    // requiresCheats = FALSE
    uint8_t* safe1  = reinterpret_cast<uint8_t*>(&rules[(int)GameRulesIndex::Pvp]);             // 15
    uint8_t* safe2  = reinterpret_cast<uint8_t*>(&rules[(int)GameRulesIndex::ShowCoordinates]); // 16

    LOGI("=== Scanning for mRequiresCheats offset ===");
    for (int i = 0; i < 0x100; i++) {
        if (cheat1[i] == 0x01 && cheat2[i] == 0x01 &&
            safe1[i]  == 0x00 && safe2[i]  == 0x00) {
            LOGI(">>> MATCH offset: 0x%02X", i);
        }
    }
}

static void GameRules_registerRules(GameRules* self) {
    g_registerRules_orig(self);

    LOGI("mGameRules.size() = %zu", self->mGameRules.size());

    if (self->mGameRules.size() > 16) {
        findRequiresCheatsOffset(self);
    } else {
        LOGE("Rules not populated yet! size=%zu", self->mGameRules.size());
    }
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

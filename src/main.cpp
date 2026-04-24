#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <link.h>
#include <span>
#include <cstddef>
#include <android/log.h>
#include "Gloss.h"
#include <libhat.hpp>
#include <libhat/scanner.hpp>
#include "gamerules.h"
// #include "utils.h"

#define LOG_TAG "CheatlessTweaks"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

using GameRules_registerRules_t = void (*)(GameRules*);
static GameRules_registerRules_t g_registerRules_orig = nullptr;

void GameRules_registerRules(GameRules* self) {
    g_registerRules_orig(self);
    // LOGI("KEENPINVENT BEFORE: %d",self->mGameRules[(int)GameRulesIndex::KeepInventory].mRequiresCheats);
    self->mGameRules[(int)GameRulesIndex::KeepInventory].mRequiresCheats = false;
    self->mGameRules[(int)GameRulesIndex::PlayerSleepingPercentage].mRequiresCheats = false;
    self->mGameRules[(int)GameRulesIndex::MobGriefing].mRequiresCheats = false;

    // LOGI("KEENPINVENT AFTER: %d",self->mGameRules[(int)GameRulesIndex::KeepInventory].mRequiresCheats);
    // LOGI("SHOWCOORD: %d",self->mGameRules[(int)GameRulesIndex::ShowCoordinates].mRequiresCheats);
}

void* main_thread(void*) {
    sleep(3);
    GlossInit(true);

    void* mc = dlopen("libminecraftpe.so", RTLD_NOLOAD);
    if (!mc) {
        LOGW("libminecraftpe.so not found");
        return nullptr;
    }

    std::span<std::byte> text;
    auto locate = [&](dl_phdr_info* info) -> int {
        void* h = dlopen(info->dlpi_name, RTLD_NOLOAD);
        dlclose(h);
        if (h != mc) return 0;
        for (int i = 0; i < info->dlpi_phnum; i++) {
            const auto& phdr = info->dlpi_phdr[i];
            if (phdr.p_type == PT_LOAD && (phdr.p_flags & PF_X)) {
                text = {
                    reinterpret_cast<std::byte*>(info->dlpi_addr + phdr.p_vaddr),
                    phdr.p_memsz
                };
                return 1;
            }
        }
        return 0;
    };

    dl_iterate_phdr([](dl_phdr_info* info, size_t, void* data) -> int {
        return (*static_cast<decltype(locate)*>(data))(info);
    }, &locate);

    if (text.empty()) {
        LOGW("Could not find executable segment");
        return nullptr;
    }

    using namespace hat::literals::signature_literals;
    auto result = hat::find_pattern(
        text,
        "FF 03 07 D1 FD 7B 17 A9 FC 67 18 A9 F8 5F 19 A9 "
        "F6 57 1A A9 F4 4F 1B A9 FD C3 05 91 57 D0 3B D5 "
        "F3 03 00 AA 2B C7 91 D2 E8 16 40 F9 6B 1C A7 F2 "_sig,
        hat::scan_alignment::X1
    );

    void* gRules_addr = result.get();
    if (!gRules_addr) {
        LOGW("Signature not found");
        return nullptr;
    }

    // LOGI("Found registerRules at %p", gRules_addr);
    GlossHook(
        gRules_addr,
        (void*)GameRules_registerRules,
        (void**)&g_registerRules_orig
    );
    LOGI("Mod initialized successfully.");
    return nullptr;
}

__attribute__((constructor))
void init() {
    pthread_t t;
    pthread_create(&t, nullptr, main_thread, nullptr);
}

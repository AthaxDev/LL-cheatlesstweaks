#include <dlfcn.h>
#include <link.h>
#include <span>
#include <cstddef>
#include <safetyhook.hpp>
#include <libhat.hpp>
#include <libhat/scanner.hpp>
#include "gamerules.h"

using GameRules_registerRules_t = void (*)(GameRules*);

static SafetyHookInline g_registerRules_hook;
static GameRules_registerRules_t g_registerRules_orig = nullptr;

void GameRules_registerRules(GameRules* self){
    g_registerRules_orig(self);
    self->mGameRules[(int)GameRulesIndex::MobGriefing].mRequiresCheats = false;
    self->mGameRules[(int)GameRulesIndex::PlayerSleepingPercentage].mRequiresCheats = false;
    self->mGameRules[(int)GameRulesIndex::KeepInventory].mRequiresCheats = false;
}

extern "C" [[gnu::visibility("default")]] void mod_preinit() {}

extern "C" [[gnu::visibility("default")]] void mod_init() {
    using namespace hat::literals::signature_literals;

    auto mc = dlopen("libminecraftpe.so", 0);
    if (!mc)
        return;

    std::span<std::byte> text;

    auto locate = [&](dl_phdr_info* info) {
        if (auto h = dlopen(info->dlpi_name, RTLD_NOLOAD); dlclose(h), h != mc)
            return 0;

        text = { 
            reinterpret_cast<std::byte*>( info->dlpi_addr + info->dlpi_phdr[1].p_vaddr), info->dlpi_phdr[1].p_memsz 
        };
        return 1;
    };

    dl_iterate_phdr( [](dl_phdr_info* info, size_t, void* data){
            return (*static_cast<decltype(locate)*>(data))(info);
        }, &locate
    );

    if (text.empty())
        return;

    auto scan = [text](const auto&... sig){
        void* addr = nullptr;
        ((addr = hat::find_pattern(text, sig, hat::scan_alignment::X16).get()) || ...);
        return addr;
    };

    auto gRules_addr = scan(
    // Try matching mid-function unique byte sequences instead of prologue
    "?? 03 00 AA ?? ?? 40 F9 ?? ?? 40 F9 ?? ?? 40 B9"_sig,  // common ARM64 load chain
    "FD 7B ?? A9 FD 03 00 91 ?? ?? ?? A9 ?? ?? ?? A9"_sig   // prologue fallback
    );

    if (!gRules_addr)
        return;

    g_registerRules_hook = safetyhook::create_inline(gRules_addr, GameRules_registerRules);

    g_registerRules_orig = g_registerRules_hook.original<GameRules_registerRules_t>();
}

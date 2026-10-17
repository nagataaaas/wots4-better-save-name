#include "game_hooks.h"
#include <cstddef>

namespace {
const wots4::HostApi* host = nullptr;
HMODULE module = nullptr;
void log(const char* message) { if (host && host->log) host->log(module, message); }
bool supported(const wots4::ModContext* context) {
    if (!context || context->size < sizeof(*context) || context->version != wots4::modApiVersion ||
        !context->game || !context->host || !context->module) return false;
    const auto& api = *context->host;
    constexpr size_t requiredSize = offsetof(wots4::HostApi, releaseClaims) + sizeof(api.releaseClaims);
    return api.size >= requiredSize && api.version == wots4::modApiVersion && api.claim && api.releaseClaims;
}
}

extern "C" BOOL WINAPI ModInitialize(const wots4::ModContext* context) {
    if (!supported(context)) return FALSE;
    host = context->host;
    module = context->module;
    if (!better_save_name::installGameHooks(*context, log)) return FALSE;
    log("[INFO] Better Save Name initialized: save list rows show the slot, day, time and last event instead of the file name.");
    return TRUE;
}

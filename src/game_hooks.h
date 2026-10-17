#pragma once
#include <wots4/mod_api.h>

namespace better_save_name {
using LogFunction = void (*)(const char* message);

// Redirects the save list's label drawing to the Mod, or changes nothing and releases every claim.
bool installGameHooks(const wots4::ModContext& context, LogFunction log);
}

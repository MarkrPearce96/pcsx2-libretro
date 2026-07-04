// retronest-libretro — shared SET_CORE_OPTIONS_V2 emission.
// CANONICAL COPY: RetroNest-Project/vendor/retronest-libretro/. Do not edit
// vendored copies; see retronest_libretro.h for the sync/drift rules.
#pragma once

#include "libretro.h"

namespace retronest {

// Sends the terminated v2 definition array with categories=nullptr —
// RetroNest's host adapter owns grouping (SettingDef.category), so cores
// never emit categories. Returns the env_cb result: false means the host
// lacks category support, but options ARE still registered and
// GET_VARIABLE works (libretro.h SET_CORE_OPTIONS_V2 contract) — callers
// may log an informational warning with their own logger.
inline bool EmitCoreOptionsV2(retro_environment_t cb,
                              const retro_core_option_v2_definition* defs)
{
    if (!cb || !defs)
        return false;
    retro_core_options_v2 opts{};
    opts.categories = nullptr;
    opts.definitions = const_cast<retro_core_option_v2_definition*>(defs);
    return cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &opts);
}

} // namespace retronest

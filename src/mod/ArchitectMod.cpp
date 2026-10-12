#include "mod/ArchitectMod.h"

#include "ll/api/mod/RegisterHelper.h"

namespace architect {

ArchitectMod& ArchitectMod::getInstance() {
    static ArchitectMod instance;
    return instance;
}

bool ArchitectMod::load() {
    getSelf().getLogger().debug("Loading Architect...");
    // Feature subsystems are initialized here as they land.
    // M1 foundation: no subsystems yet (core model + NBT interface only).
    return true;
}

bool ArchitectMod::enable() {
    getSelf().getLogger().debug("Enabling Architect...");
    return true;
}

bool ArchitectMod::disable() {
    getSelf().getLogger().debug("Disabling Architect...");
    // All render hooks and verifier state must be released here.
    return true;
}

} // namespace architect

LL_REGISTER_MOD(architect::ArchitectMod, architect::ArchitectMod::getInstance());

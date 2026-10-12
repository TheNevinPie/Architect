#pragma once

#include "ll/api/mod/NativeMod.h"

namespace architect {

// Thin LeviLamina entry point. All feature logic lives in
// src/architect/* (pure C++ core) and future game-facing adapters.
// This class must stay free of schematic/rendering logic so the core
// remains buildable and testable without the LeviLamina SDK.
class ArchitectMod {
public:
    static ArchitectMod& getInstance();

    ArchitectMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    /// @return True if the mod is loaded successfully.
    bool load();

    /// @return True if the mod is enabled successfully.
    bool enable();

    /// @return True if the mod is disabled successfully.
    bool disable();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace architect

#pragma once
#include "Models.h"
#include <atomic>
#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ItemSearch
{
    struct AppState
    {
        PluginConfig              config;
        mutable std::shared_mutex configLock;

        std::vector<FoundItem>    items;
        mutable std::shared_mutex itemsLock;
        std::atomic<uint64_t>     itemsVersion{0};

        std::atomic<bool>         fetching{false};
        std::atomic<bool>         showWindow{true};

        std::string               fetchError;
        std::string               accountName;
        mutable std::shared_mutex statusLock;

        // Menomonia UI fonts (ImFont*), delivered async by Nexus. Every px size
        // is registered once under its own identifier ("LIIS_FONT_<px*10>") and
        // cached in fontsById (nullptr = pending delivery). All font bookkeeping
        // is touched from the render/load thread only.
        void*                     fontData     = nullptr; // embedded TTF (module lifetime)
        uint32_t                  fontDataSize = 0;
        std::unordered_map<std::string, void*> fontsById;    // id -> ImFont* (nullptr = pending)
        std::vector<std::string>  addedFontIds;              // for release on unload
    };

    inline PluginConfig GetConfig(const AppState& s)
    {
        std::shared_lock lk(s.configLock);
        return s.config;
    }

    inline void SetConfig(AppState& s, const PluginConfig& cfg)
    {
        std::unique_lock lk(s.configLock);
        s.config = cfg;
    }
}

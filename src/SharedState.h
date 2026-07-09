#pragma once
#include "Models.h"
#include <atomic>
#include <shared_mutex>
#include <string>
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

        // Menomonia at fixed Blish HUD sizes (ImFont*), delivered async by Nexus
        std::atomic<void*>        fontBody{nullptr};
        std::atomic<void*>        fontTitle{nullptr};
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

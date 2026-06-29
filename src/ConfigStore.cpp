#include "ConfigStore.h"
#include "Constants.h"
#include "Lang.h"
#include "SharedState.h"
#include <cstring>
#include <direct.h>
#include <fstream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace LegendaryImpactItemSearch
{
    ConfigStore::ConfigStore() = default;

    void ConfigStore::Load()
    {
        std::ifstream file(Constants::SettingsFile);
        if (!file.is_open()) return;

        try
        {
            json data;
            file >> data;

            PluginConfig config;
            const std::string key = data.value("apiKey", "");
            strncpy_s(config.apiKey, sizeof(config.apiKey), key.c_str(), _TRUNCATE);
            config.language = data.value("language", 1);

            Lang::SetLanguage(static_cast<Lang::Language>(config.language));
            SetConfig(*g_State, config);

            strncpy_s(m_EditApiKey.data(), m_EditApiKey.size(), config.apiKey, _TRUNCATE);
            m_EditLanguage   = config.language;
            m_EditShowWindow = data.value("showWindow", true);

            g_State->showWindow.store(m_EditShowWindow, std::memory_order_relaxed);
        }
        catch (...) {}
    }

    void ConfigStore::Save() const
    {
        _mkdir("addons");
        _mkdir(Constants::SettingsDir);

        const PluginConfig config = GetConfig(*g_State);
        json data;
        data["apiKey"]     = config.apiKey;
        data["language"]   = config.language;
        data["showWindow"] = g_State->showWindow.load(std::memory_order_relaxed);

        std::ofstream file(Constants::SettingsFile);
        if (file.is_open()) file << data.dump(4);
    }

    void ConfigStore::ApplyFromEditBuffer()
    {
        PluginConfig next;
        strncpy_s(next.apiKey, sizeof(next.apiKey), m_EditApiKey.data(), _TRUNCATE);
        next.language = m_EditLanguage;
        Lang::SetLanguage(static_cast<Lang::Language>(next.language));
        SetConfig(*g_State, next);
        g_State->showWindow.store(m_EditShowWindow, std::memory_order_relaxed);
        Save();
    }

    char*    ConfigStore::ApiKeyBuffer() { return m_EditApiKey.data(); }
    int32_t& ConfigStore::Language()     { return m_EditLanguage; }
    bool&    ConfigStore::ShowWindow()   { return m_EditShowWindow; }
}

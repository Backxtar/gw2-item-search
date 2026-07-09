#pragma once
#include "Models.h"
#include "SharedState.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ItemSearch
{
    class ConfigStore
    {
    public:
        ConfigStore();

        void Load(AppState& state);
        void Save(const AppState& state) const;
        void ApplyFromEditBuffer(AppState& state);

        void LoadItemCache(std::vector<FoundItem>& out) const;
        void SaveItemCache(const std::vector<FoundItem>& items) const;

        char*        ApiKeyBuffer();
        int32_t&     Language();
        bool&        ShowWindow();
        float&       FontSize();
        std::string& CachedAccountName();

    private:
        std::array<char, 256> m_EditApiKey{};
        int32_t               m_EditLanguage      = 1;
        bool                  m_EditShowWindow    = true;
        float                 m_EditFontSize      = 16.0f;
        std::string           m_CachedAccountName;
    };
}

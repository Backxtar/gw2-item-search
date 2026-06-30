#pragma once
#include "Models.h"
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

        void Load();
        void Save() const;
        void ApplyFromEditBuffer();

        void LoadItemCache(std::vector<FoundItem>& out) const;
        void SaveItemCache(const std::vector<FoundItem>& items) const;

        char*        ApiKeyBuffer();
        int32_t&     Language();
        bool&        ShowWindow();
        std::string& CachedAccountName();

    private:
        std::array<char, 256> m_EditApiKey{};
        int32_t               m_EditLanguage      = 1;
        bool                  m_EditShowWindow    = true;
        std::string           m_CachedAccountName;
    };
}

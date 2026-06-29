#pragma once
#include <array>
#include <cstdint>

namespace LegendaryImpactItemSearch
{
    class ConfigStore
    {
    public:
        ConfigStore();

        void Load();
        void Save() const;
        void ApplyFromEditBuffer();

        char*    ApiKeyBuffer();
        int32_t& Language();
        bool&    ShowWindow();

    private:
        std::array<char, 256> m_EditApiKey{};
        int32_t               m_EditLanguage   = 1;
        bool                  m_EditShowWindow = true;
    };
}

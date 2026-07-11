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

        // Item cache is stored per account ("items_cache_<key-hash>.json").
        void LoadItemCache(const std::string& apiKey, std::vector<FoundItem>& out) const;
        void SaveItemCache(const std::string& apiKey, const std::vector<FoundItem>& items) const;

        // Options edit buffers: one API-key buffer per configured account.
        using ApiKeyBuf = std::array<char, 256>;
        std::vector<ApiKeyBuf>& ApiKeyBuffers();
        void                    AddAccountBuffer();
        void                    RemoveAccountBuffer(size_t index);

        int32_t&     Language();
        bool&        ShowWindow();
        float&       FontSize();
        float&       HeadingSize();
        float&       ButtonSize();
        float&       TooltipSize();

    private:
        static std::string CacheFileFor(const std::string& apiKey);

        std::vector<ApiKeyBuf> m_EditApiKeys;
        int32_t               m_EditLanguage      = 1;
        bool                  m_EditShowWindow    = true;
        float                 m_EditFontSize      = 16.0f;
        float                 m_EditHeadingSize   = 20.0f;
        float                 m_EditButtonSize    = 16.0f;
        float                 m_EditTooltipSize   = 16.0f;
    };
}

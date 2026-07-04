#pragma once
#include "ConfigStore.h"
#include "Models.h"
#include "SharedState.h"
#include "nexus/Nexus.h"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ItemSearch
{
    class ItemSearchWindow
    {
    public:
        void SetApi(AddonAPI_t* api) { m_Api = api; }

        void Render(AppState& state, bool& outRequestRefresh);
        void RenderOptions(ConfigStore& config, AppState& state, bool& outRequestRefresh);

        static std::string GetLocationStr(const FoundItem& item);

    private:
        static bool        MatchesFilter(const FoundItem& item, const char* filterLower);
        static bool        SplitUrl(const std::string& url, std::string& remote, std::string& endpoint);
        void*              GetOrLoadTexture(const std::string& iconUrl);
        void*              GetClassIcon(const std::string& professionOrSpec); // bundled tango icon
        void               ShowItemTooltip(const FoundItem& item, void* texIcon) const;
        // Draws a 3- (with location) or 2-column item table. Records the hovered
        // row into ioHover/ioHoverTex (only if not already set by an earlier table).
        void               RenderItemTable(const char* id, const std::vector<const FoundItem*>& items,
                                           bool showLocation,
                                           const FoundItem*& ioHover, void*& ioHoverTex,
                                           float height = 0.0f, bool scroll = true);

        AddonAPI_t*                        m_Api = nullptr;
        std::array<char, 256>              m_SearchBuf{};
        std::string                        m_FilterLower;
        std::unordered_map<std::string, void*> m_TexCache; // keyed by icon URL (skin-aware)
        int                                m_HoveredItemId      = 0;
        float                              m_HoverStartTime     = 0.0f;
        float                              m_LastRefreshTime    = -1e9f; // throttle the manual Load button
        std::vector<FoundItem>             m_AggregatedSnapshot;
        std::vector<FoundItem>             m_BankRaw;            // bank items per slot (not aggregated)
        std::vector<FoundItem>             m_EquipRaw;          // equipment per template tab (not aggregated)
        std::unordered_map<std::string, int> m_SelEquipTab;    // character -> selected equipment template idx
        uint64_t                           m_AggregatedVersion  = ~uint64_t(0);
        std::vector<const FoundItem*>      m_FilteredItems;
        std::string                        m_CachedFilter;
        uint64_t                           m_FilteredVersion    = ~uint64_t(0);
        std::vector<std::string>           m_CharNames;          // distinct character names (tabs)
        std::unordered_map<std::string, std::string> m_CharProf;      // name -> profession
        std::unordered_map<std::string, std::string> m_CharEliteName; // name -> elite spec name
        std::unordered_map<std::string, int>         m_CharEliteId;   // name -> elite spec id (0 = core), tab order
        uint64_t                           m_CharTabsVersion    = ~uint64_t(0);
        int                                m_ActiveTab          = 0; // 0 = search, 1.. = character
        std::unordered_map<std::string, void*> m_ProfIcons;      // profession/spec name -> texture
        void*                              m_CoinGold           = nullptr;
        void*                              m_CoinSilver         = nullptr;
        void*                              m_CoinCopper         = nullptr;
        void*                              m_FoodIcon           = nullptr;
        void*                              m_UtilityIcon        = nullptr;
    };
}

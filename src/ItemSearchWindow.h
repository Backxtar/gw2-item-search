#pragma once
#include "ConfigStore.h"
#include "Models.h"
#include "SharedState.h"
#include "mumble/Mumble.h"
#include "nexus/Nexus.h"
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct ImFont;

namespace ItemSearch
{
    class ItemSearchWindow
    {
    public:
        void SetApi(AddonAPI_t* api) { m_Api = api; }
        void SetLinks(NexusLinkData_t* nexus, Mumble::Data* mumble)
        {
            m_NexusLink  = nexus;
            m_MumbleLink = mumble;
        }

        // outSwitchAccount: index of the account picked in the dropdown this
        // frame (-1 = no change); the app loads that account's cache/items.
        // outRequestRefreshAll: fetch every configured account, not just the active one.
        void Render(AppState& state, bool& outRequestRefresh, bool& outRequestRefreshAll,
                    int& outSwitchAccount);
        void RenderOptions(ConfigStore& config, AppState& state, bool& outRequestRefresh);

        static std::string GetLocationStr(const FoundItem& item);

    private:
        static bool        MatchesFilter(const FoundItem& item, const char* filterLower);
        static bool        SplitUrl(const std::string& url, std::string& remote, std::string& endpoint);
        // Menomonia atlas font for the wanted px size; registers the size with
        // Nexus on first use and returns nullptr until it has been delivered.
        ImFont*            EnsureFont(AppState& state, float px);
        void*              GetOrLoadTexture(const std::string& iconUrl);
        // Nexus texture lookup with a member cache slot (resolves once loaded).
        void*              GetTex(const char* id, void*& cache) const;
        // Draws the GW2/Blish-style window chrome (title bar textures, emblem,
        // title text, exit button) and moves the cursor below the bar.
        void               RenderWindowChrome(AppState& state, const char* titleText);
        // GW2-style button drawn from the native button atlas; returns true on click.
        bool               Gw2Button(const char* label, float height = 0.0f, bool disabled = false);
        void*              GetClassIcon(const std::string& professionOrSpec); // bundled tango icon
        void               ShowItemTooltip(const FoundItem& item, void* texIcon) const;
        // Draws a 3- (with location) or 2-column item table. Records the hovered
        // row into ioHover/ioHoverTex (only if not already set by an earlier table).
        void               RenderItemTable(const char* id, const std::vector<const FoundItem*>& items,
                                           bool showLocation,
                                           const FoundItem*& ioHover, void*& ioHoverTex,
                                           float height = 0.0f, bool scroll = true);
        // GW2-inventory-style icon grid (wrapping slot rows, count badges).
        // While searching, non-matching items are dimmed and matches lit gold.
        void               RenderItemGrid(const char* id,
                                          const std::vector<std::pair<const FoundItem*, bool>>& items,
                                          bool searching,
                                          const FoundItem*& ioHover, void*& ioHoverTex);
        // One equipment slot (present or empty) at the current cursor position.
        void               DrawEquipSlot(const FoundItem* item, bool matched, bool searching,
                                         float size, const FoundItem*& ioHover, void*& ioHoverTex);

        AddonAPI_t*                        m_Api        = nullptr;
        NexusLinkData_t*                   m_NexusLink  = nullptr; // shared Nexus data (fonts, UI scaling)
        Mumble::Data*                      m_MumbleLink = nullptr; // game state via Mumble link
        std::array<char, 256>              m_SearchBuf{};
        std::string                        m_FilterLower;
        std::unordered_map<std::string, void*> m_TexCache; // keyed by icon URL (skin-aware)
        int                                m_HoveredItemId      = 0;
        float                              m_HoverStartTime     = 0.0f;
        float                              m_LastRefreshTime    = -1e9f; // throttle the manual Load button
        std::vector<FoundItem>             m_AggregatedSnapshot;
        std::vector<FoundItem>             m_BankRaw;            // bank items per slot (not aggregated)
        std::vector<FoundItem>             m_EquipRaw;          // equipment per template tab (not aggregated)
        std::vector<FoundItem>             m_CharInvRaw;         // character bag items per slot (not aggregated)
        std::vector<FoundItem>             m_SharedRaw;          // shared inventory per slot (not aggregated)
        std::unordered_map<std::string, int> m_SelEquipTab;    // character -> selected equipment template idx
        uint64_t                           m_AggregatedVersion  = ~uint64_t(0);
        std::vector<const FoundItem*>      m_FilteredItems;
        std::string                        m_CachedFilter;
        uint64_t                           m_FilteredVersion    = ~uint64_t(0);
        // Bank-tab view (materials + full 30-slot grids per bank tab, items with
        // search-match flag); rebuilt only when data version or filter changes.
        std::vector<std::pair<const FoundItem*, bool>>              m_MatsAll;
        std::vector<std::vector<std::pair<const FoundItem*, bool>>> m_BankTabGrids;
        uint64_t                           m_BankCacheVersion   = ~uint64_t(0);
        std::string                        m_BankCacheFilter;
        // Character-tab view; rebuilt when data/filter/character change, the
        // equipment slots additionally when the selected template changes.
        // Items carry a search-match flag (grids dim non-matches while searching).
        struct EquipTabInfo { int idx; std::string name; bool active; bool matched; };
        std::vector<std::pair<const FoundItem*, bool>> m_CharInv;    // this character's bag items
        std::vector<std::pair<const FoundItem*, bool>> m_CharShared; // shared inventory
        std::vector<EquipTabInfo>          m_EquipTabs;          // template tabs incl. search-match flag
        std::unordered_map<std::string, std::pair<const FoundItem*, bool>> m_EquipBySlot; // selected template gear
        std::vector<std::pair<const FoundItem*, bool>> m_EquipExtra; // gear in slots the panel doesn't lay out
        uint64_t                           m_CharCacheVersion   = ~uint64_t(0);
        std::string                        m_CharCacheFilter;
        std::string                        m_CharCacheName;
        int                                m_CharCacheSel       = -2; // -2 = dirty (-1 is a valid "no tabs" value)
        // Window title (name + ###id); rebuilt only when the language changes
        std::string                        m_WindowTitle;
        const char*                        m_WindowTitleSrc     = nullptr;
        std::vector<std::string>           m_CharNames;          // distinct character names (tabs)
        std::unordered_map<std::string, std::string> m_CharProf;      // name -> profession
        std::unordered_map<std::string, std::string> m_CharEliteName; // name -> elite spec name
        std::unordered_map<std::string, int>         m_CharEliteId;   // name -> elite spec id (0 = core), tab order
        std::unordered_map<std::string, int>         m_CharLevel;     // name -> character level
        std::unordered_map<std::string, std::string> m_CharRace;      // name -> race (e.g. "Human")
        uint64_t                           m_CharTabsVersion    = ~uint64_t(0);
        int                                m_ActiveTab          = 0; // 0 = search, 1.. = character
        std::unordered_map<std::string, void*> m_ProfIcons;      // profession/spec name -> texture
        void*                              m_CoinGold           = nullptr;
        void*                              m_CoinSilver         = nullptr;
        void*                              m_CoinCopper         = nullptr;
        void*                              m_FoodIcon           = nullptr;
        void*                              m_UtilityIcon        = nullptr;
        // Window chrome textures (Blish HUD ref assets), cached once loaded
        void*                              m_TexTitlebar        = nullptr;
        void*                              m_TexTitlebarActive  = nullptr;
        void*                              m_TexTopRight        = nullptr;
        void*                              m_TexTopRightActive  = nullptr;
        void*                              m_TexExit            = nullptr;
        void*                              m_TexExitActive      = nullptr;
        void*                              m_TexEmblem          = nullptr;
        void*                              m_TexWindowBg        = nullptr;
        mutable void*                      m_TexTooltipBg       = nullptr; // used in const tooltip render
        void*                              m_TexTextbox         = nullptr;
        void*                              m_TexItemHover       = nullptr;
        float                              m_EffFontScale       = 1.0f; // wanted body px / current atlas px
        uint32_t                           m_FrameTick          = 0;    // frame counter (texture poll throttle)
        std::string                        m_CtxWikiName;               // item name for the right-click wiki menu
    };
}

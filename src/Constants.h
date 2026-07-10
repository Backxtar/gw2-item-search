#pragma once

namespace ItemSearch::Constants
{
    inline constexpr const char* AddonName        = "Item Search";
    inline constexpr const char* IconId           = "ICON_ITEM_SEARCH";
    inline constexpr const char* IconHoverId      = "ICON_ITEM_SEARCH_HOVER";
    inline constexpr const char* QuickAccessId    = "QA_ITEM_SEARCH";
    inline constexpr const char* GW2ApiBase       = "https://api.guildwars2.com/v2";
    inline constexpr const char* SettingsDir      = "addons/ItemSearch";
    inline constexpr const char* SettingsFile     = "addons/ItemSearch/settings.json";
    inline constexpr const char* ItemCacheFile    = "addons/ItemSearch/items_cache.json";
    inline constexpr const char* KeybindToggleId  = "KB_ITEM_SEARCH";
    inline constexpr const char* WindowId         = "###ItemSearchWindow";
    inline constexpr int         ApiBatchSize     = 200;
    // Upper bound for simultaneous HTTP requests (batch resolves, per-character
    // equipment): keeps large accounts from opening dozens of parallel
    // connections and tripping the GW2 API rate limit.
    inline constexpr int         MaxParallelRequests = 6;
    inline constexpr const char* CoinGoldId       = "LIIS_COIN_GOLD";
    inline constexpr const char* CoinSilverId     = "LIIS_COIN_SILVER";
    inline constexpr const char* CoinCopperId     = "LIIS_COIN_COPPER";
    inline constexpr const char* FoodIconId       = "LIIS_NOURISH_FOOD";
    inline constexpr const char* UtilityIconId    = "LIIS_NOURISH_UTILITY";
    inline constexpr const char* ProfIconPrefix   = "LIIS_PROF_";
    // GW2-style window chrome (textures from the Blish HUD ref assets)
    inline constexpr const char* WndTitlebarId       = "LIIS_WND_TITLEBAR";
    inline constexpr const char* WndTitlebarActiveId = "LIIS_WND_TITLEBAR_ACTIVE";
    inline constexpr const char* WndTopRightId       = "LIIS_WND_TOPRIGHT";
    inline constexpr const char* WndTopRightActiveId = "LIIS_WND_TOPRIGHT_ACTIVE";
    inline constexpr const char* WndExitId           = "LIIS_WND_EXIT";
    inline constexpr const char* WndExitActiveId     = "LIIS_WND_EXIT_ACTIVE";
    inline constexpr const char* WndBackgroundId     = "LIIS_WND_BACKGROUND";
    inline constexpr const char* TooltipBgId         = "LIIS_TOOLTIP_BG";
    inline constexpr const char* TextboxId           = "LIIS_TEXTBOX";
    inline constexpr const char* ItemHoverId         = "LIIS_ITEM_HOVER";
    // Menomonia UI font. Every requested px size is registered once under
    // "LIIS_FONT_<px*10>" as its own crisp dedicated atlas font (sizes are
    // shared across roles: body/heading/button/tooltip just pick their px).
    inline constexpr const char* FontIdPrefix        = "LIIS_FONT";
    inline constexpr float      FontBodySize         = 16.0f;  // default item/body px (layout-scale reference)
    inline constexpr float      FontHeadingSize      = 20.0f;  // default section-header px
    inline constexpr float      FontTitleScale       = 1.75f;  // window title px = body px * this
    inline constexpr float      FontTipTitleScale    = 1.25f;  // tooltip item-name px = tooltip px * this
}

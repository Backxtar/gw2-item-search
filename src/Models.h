#pragma once
#include <cstdint>
#include <string>

namespace LegendaryImpactItemSearch
{
    struct PluginConfig
    {
        char    apiKey[256] = {};
        int32_t language    = 1; // 0 = German, 1 = English
    };

    enum class ItemLocation : uint8_t
    {
        Bank = 0,
        SharedInventory,
        Materials,
        Character
    };

    struct FoundItem
    {
        int          itemId       = 0;
        int          count        = 0;
        std::string  name;
        ItemLocation locationType = ItemLocation::Bank;
        std::string  characterName; // populated for ItemLocation::Character
    };
}

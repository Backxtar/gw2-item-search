#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ItemSearch
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
        Character,
        Equipment,
        LegendaryArmory
    };

    // One attribute line of a stat combination as returned by /v2/itemstats,
    // used to compute actual values: round(attribute_adjustment * multiplier + value)
    struct StatAttribute
    {
        std::string attribute;
        double      multiplier = 0.0;
        double      value      = 0.0;
    };

    struct StatInfo
    {
        std::string                name;   // localized (e.g. "Vipernhaft")
        std::vector<StatAttribute> attrs;
    };

    // All per-item fields resolved from a single /v2/items entry.
    struct ResolvedItem
    {
        std::string name, icon, desc, type, rarity, subType, weightClass;
        int    level = 0, vendorValue = 0, defense = 0, minPower = 0, maxPower = 0;
        bool   accountBound = false;
        double attrAdjust   = 0.0;            // details.attribute_adjustment
        std::string buffDescription;          // UpgradeComponent: infix_upgrade.buff.description
        std::vector<std::string> bonuses;     // UpgradeComponent: details.bonuses
        std::string consumableDesc;           // Consumable: details.description (food/utility effect)
        int         durationMs = 0;           // Consumable: details.duration_ms
        // Fixed stats baked into the item (details.infix_upgrade.attributes),
        // e.g. exotic gear with a non-selectable prefix like "Berserker's".
        std::vector<std::pair<std::string, int>> infixAttributes;
    };

    // A rune/sigil/infusion slotted into a parent gear item, shown nested in its tooltip.
    struct EmbeddedUpgrade
    {
        int                      itemId = 0;
        std::string              name;
        std::string              rarity;
        std::string              iconUrl;
        std::string              buffDescription;   // infix_upgrade.buff.description (stripped)
        std::vector<std::string> bonuses;           // details.bonuses (set bonuses), stripped
        bool                     isInfusion = false;
    };

    struct FoundItem
    {
        int          itemId       = 0;
        int          count        = 0;
        std::string  name;
        std::string  nameLower;
        std::string  description;
        std::string  iconUrl;
        std::string  type;
        std::string  rarity;
        int          level        = 0;
        bool         accountBound = false;
        int          vendorValue  = 0;
        std::string  subType;       // details.type  (e.g. "Sword" / "Helm")
        std::string  weightClass;   // details.weight_class (armor only, e.g. "Heavy")
        int          statId      = 0;  // selected stat combination id (itemstats endpoint)
        std::string  statName;         // resolved stat prefix (e.g. "Viper's" / "Vipernhaft")
        // Actual computed attribute values from the equipped/stored instance
        // (e.g. {"Power",63},{"Precision",45}). Sorted by value, descending.
        std::vector<std::pair<std::string, int>> attributes;
        int          defense  = 0;     // armor defense (details.defense)
        int          minPower = 0;     // weapon strength (details.min_power)
        int          maxPower = 0;     // weapon strength (details.max_power)
        // If this item itself is an UpgradeComponent (rune/sigil/infusion):
        std::string              buffDescription;
        std::vector<std::string> bonuses;
        // If this item is a Consumable (food / utility): effect + duration
        std::string              consumableDesc;
        int                      durationMs = 0;
        // Runes/sigils/infusions slotted into this gear item (for the tooltip):
        std::vector<EmbeddedUpgrade> upgradeSlots;
        ItemLocation locationType = ItemLocation::Bank;
        std::string  characterName;
        std::string  characterProfession;  // e.g. "Mesmer" (only set for character items)
        std::string  characterEliteSpec;   // active elite spec name, e.g. "Chronomancer" (empty = core)
        int          characterEliteSpecId = 0; // elite specialization id (0 = core); used to order tabs by release
        std::string  characterEliteIcon;   // elite spec icon URL (loaded at runtime)
        std::string  equipSlot;            // equipment slot name (e.g. "Helm", "WeaponA1", "Ring1")
        int          bankSlot   = -1;      // absolute slot index in the bank (for bank-tab paging)
    };
}

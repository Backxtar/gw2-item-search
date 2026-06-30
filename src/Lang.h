#pragma once

namespace ItemSearch::Lang
{
    enum class Language { German = 0, English = 1 };

    struct Strings
    {
        // Window
        const char* windowTitle;
        // Controls
        const char* searchHint;
        const char* refreshBtn;
        const char* refreshing;
        const char* refreshCooldown;
        // Table columns
        const char* colItem;
        const char* colLocation;
        const char* colCount;
        // Tabs
        const char* tabSearch;
        const char* tabBank;
        const char* bankTab;     // prefix for a single bank tab, e.g. "Fach"
        // Equipment sections
        const char* secArmor;
        const char* secWeapons;
        const char* secTrinkets;
        const char* secAquatic;
        const char* secGathering;
        const char* secJadebot;
        // Status / errors
        const char* noResults;
        const char* noApiKey;
        const char* fetchError;
        const char* accountLabel;
        // Options
        const char* optGeneralSection;
        const char* optShowWindow;
        const char* optApiKey;
        const char* optApiKeyHint;
        const char* optApiKeyPerms;
        const char* optLanguage;
        const char* optSave;
        const char* optKeybindHint;
        // Location labels
        const char* locBank;
        const char* locSharedInventory;
        const char* locMaterials;
        const char* locCharacter;
        const char* locInventory;
        const char* locEquipment;
        const char* locLegendaryArmory;
        // Tooltip footer
        const char* tooltipStats;
        const char* tooltipDefense;
        const char* tooltipWeaponStrength;
        const char* tooltipLevel;
        const char* tooltipAccountBound;
        const char* tooltipVendor;
        const char* tooltipGold;
        const char* tooltipSilver;
        const char* tooltipCopper;
    };

    void           SetLanguage(Language lang);
    Language       GetLanguage();
    const Strings& Get();
    const Strings& Get(Language lang); // strings for a specific language, independent of the active one

    // Translate GW2 API enum strings (always returned in English) to the active language
    const char* TranslateRarity(const char* rarity);
    const char* TranslateItemType(const char* type);
    const char* TranslateWeaponType(const char* subType);
    const char* TranslateArmorSlot(const char* subType);
    const char* TranslateWeightClass(const char* weightClass);
    // Translate an itemstats attribute key (e.g. "Power", "CritDamage") to the active language
    const char* TranslateAttribute(const char* attribute);
    // Returns "Einhand"/"One-Handed", "Zweihand"/"Two-Handed", or "Begleithand"/"Off-Hand"
    const char* GetWeaponHandedness(const char* weaponSubType);
}

#pragma once

namespace LegendaryImpactItemSearch::Lang
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
        // Table columns
        const char* colItem;
        const char* colLocation;
        const char* colCount;
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
        const char* optLanguage;
        const char* optSave;
        const char* optKeybindHint;
        // Location labels
        const char* locBank;
        const char* locSharedInventory;
        const char* locMaterials;
    };

    void           SetLanguage(Language lang);
    Language       GetLanguage();
    const Strings& Get();
}

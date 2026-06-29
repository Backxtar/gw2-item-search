#include "Lang.h"

namespace LegendaryImpactItemSearch::Lang
{
    static Language s_Language = Language::English;

    static constexpr Strings s_German
    {
        /* windowTitle       */ "Item Suche",
        /* searchHint        */ "Item suchen...",
        /* refreshBtn        */ "Aktualisieren",
        /* refreshing        */ "Lade Daten...",
        /* colItem           */ "Item",
        /* colLocation       */ "Ort",
        /* colCount          */ "Anzahl",
        /* noResults         */ "Keine Items gefunden.",
        /* noApiKey          */ "Bitte GW2 API-Key in den Einstellungen hinterlegen.",
        /* fetchError        */ "Fehler: %s",
        /* accountLabel      */ "Account: %s",
        /* optGeneralSection */ "Allgemein",
        /* optShowWindow     */ "Fenster anzeigen",
        /* optApiKey         */ "GW2 API-Key",
        /* optApiKeyHint     */ "API-Key mit den Berechtigungen: account, inventories, characters",
        /* optLanguage       */ "Sprache (0=Deutsch, 1=Englisch)",
        /* optSave           */ "Einstellungen speichern",
        /* optKeybindHint    */ "Keybind: bitte in den Nexus Keybind-Einstellungen f\xc3\xbcr Legendary Impact - Item Search setzen.",
        /* locBank           */ "Bank",
        /* locSharedInventory*/ "Geteiltes Inventar",
        /* locMaterials      */ "Materiallager",
    };

    static constexpr Strings s_English
    {
        /* windowTitle       */ "Item Search",
        /* searchHint        */ "Search items...",
        /* refreshBtn        */ "Refresh",
        /* refreshing        */ "Loading...",
        /* colItem           */ "Item",
        /* colLocation       */ "Location",
        /* colCount          */ "Count",
        /* noResults         */ "No items found.",
        /* noApiKey          */ "Please enter your GW2 API key in the settings.",
        /* fetchError        */ "Error: %s",
        /* accountLabel      */ "Account: %s",
        /* optGeneralSection */ "General",
        /* optShowWindow     */ "Show window",
        /* optApiKey         */ "GW2 API Key",
        /* optApiKeyHint     */ "API key with permissions: account, inventories, characters",
        /* optLanguage       */ "Language (0=German, 1=English)",
        /* optSave           */ "Save settings",
        /* optKeybindHint    */ "Keybind: please configure in the Nexus keybind settings for Legendary Impact - Item Search.",
        /* locBank           */ "Bank",
        /* locSharedInventory*/ "Shared Inventory",
        /* locMaterials      */ "Material Storage",
    };

    void SetLanguage(Language lang) { s_Language = lang; }
    Language GetLanguage() { return s_Language; }
    const Strings& Get() { return s_Language == Language::German ? s_German : s_English; }
}

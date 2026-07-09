#include "Lang.h"
#include <cstring>

namespace ItemSearch::Lang
{
    static Language s_Language = Language::English;

    static constexpr Strings s_German
    {
        /* windowTitle       */ "Item Suche",
        /* searchHint        */ "Item suchen...",
        /* refreshBtn        */ "Aktualisieren",
        /* refreshing        */ "L\xc3\xa4" "dt...",
        /* refreshCooldown   */ "Verf\xc3\xbcgbar in",
        /* colItem           */ "Item",
        /* colLocation       */ "Ort",
        /* colCount          */ "Anzahl",
        /* tabSearch         */ "Account",
        /* tabBank           */ "Bank",
        /* bankTab           */ "Fach",
        /* secArmor          */ "R\xc3\xbcstung",
        /* secWeapons        */ "Waffen",
        /* secTrinkets       */ "R\xc3\xbc" "cken & Schmuck",
        /* secAquatic        */ "Unterwasser",
        /* secGathering      */ "Sammelwerkzeuge",
        /* secJadebot        */ "Jade-Bot",
        /* secUpgrades       */ "Infusionen & Aufwertungen",
        /* noResults         */ "Keine Items gefunden.",
        /* noApiKey          */ "Bitte GW2 API-Key in den Einstellungen hinterlegen.",
        /* fetchError        */ "Fehler: %s",
        /* accountLabel      */ "Account: %s",
        /* optGeneralSection */ "Allgemein",
        /* optShowWindow     */ "Fenster anzeigen",
        /* optApiKey         */ "GW2 API-Key",
        /* optApiKeyHint     */ "API-Key hier einf\xc3\xbcgen und Speichern klicken.",
        /* optApiKeyPerms    */ "Ben\xc3\xb6" "tigte Berechtigungen: account, inventories, characters, builds (optional, f\xc3\xbcr Elite-Spec), unlocks (optional, f\xc3\xbcr Legend\xc3\xa4re R\xc3\xbcstkammer)",
        /* optLanguage       */ "Sprache",
        /* optFontScale      */ "UI-Gr\xc3\xb6\xc3\x9f" "e",
        /* optSave           */ "Speichern",
        /* optKeybindHint    */ "Keybind: bitte in den Nexus Keybind-Einstellungen f\xc3\xbcr Item Search setzen.",
        /* locBank           */ "Bank",
        /* locSharedInventory*/ "Geteiltes Inventar",
        /* locMaterials      */ "Materiallager",
        /* locCharacter      */ "Charakter",
        /* locInventory      */ "Inventar",
        /* locEquipment      */ "Ausr\xc3\xbcstung",
        /* locLegendaryArmory*/ "Legend\xc3\xa4re R\xc3\xbcstkammer",
        /* tooltipTransmuted */ "Transmutiert:",
        /* tooltipStats      */ "Werte:",
        /* tooltipDefense    */ "Verteidigung:",
        /* tooltipWeaponStrength */ "Waffenst\xc3\xa4rke:",
        /* tooltipLevel      */ "Stufe",
        /* tooltipAccountBound*/ "Accountgebunden",
        /* tooltipVendor     */ "H\xc3\xa4ndlerwert:",
        /* tooltipGold       */ "G",
        /* tooltipSilver     */ "S",
        /* tooltipCopper     */ "K",
        /* durDay            */ "t",
        /* durHour           */ "h",
        /* durMin            */ "min",
        /* durSec            */ "sek",
    };

    static constexpr Strings s_English
    {
        /* windowTitle       */ "Item Search",
        /* searchHint        */ "Search items...",
        /* refreshBtn        */ "Refresh",
        /* refreshing        */ "Loading...",
        /* refreshCooldown   */ "Available in",
        /* colItem           */ "Item",
        /* colLocation       */ "Location",
        /* colCount          */ "Count",
        /* tabSearch         */ "Account",
        /* tabBank           */ "Bank",
        /* bankTab           */ "Tab",
        /* secArmor          */ "Armor",
        /* secWeapons        */ "Weapons",
        /* secTrinkets       */ "Back & Trinkets",
        /* secAquatic        */ "Aquatic",
        /* secGathering      */ "Gathering",
        /* secJadebot        */ "Jade Bot",
        /* secUpgrades       */ "Infusions & Upgrades",
        /* noResults         */ "No items found.",
        /* noApiKey          */ "Please enter your GW2 API key in the settings.",
        /* fetchError        */ "Error: %s",
        /* accountLabel      */ "Account: %s",
        /* optGeneralSection */ "General",
        /* optShowWindow     */ "Show window",
        /* optApiKey         */ "GW2 API Key",
        /* optApiKeyHint     */ "Paste your API key here and click Save.",
        /* optApiKeyPerms    */ "Required permissions: account, inventories, characters, builds (optional, for elite spec), unlocks (optional, for legendary armory)",
        /* optLanguage       */ "Language",
        /* optFontScale      */ "UI size",
        /* optSave           */ "Save",
        /* optKeybindHint    */ "Keybind: please configure in the Nexus keybind settings for Item Search.",
        /* locBank           */ "Bank",
        /* locSharedInventory*/ "Shared Inventory",
        /* locMaterials      */ "Material Storage",
        /* locCharacter      */ "Character",
        /* locInventory      */ "Inventory",
        /* locEquipment      */ "Equipment",
        /* locLegendaryArmory*/ "Legendary Armory",
        /* tooltipTransmuted */ "Transmuted:",
        /* tooltipStats      */ "Stats:",
        /* tooltipDefense    */ "Defense:",
        /* tooltipWeaponStrength */ "Weapon Strength:",
        /* tooltipLevel      */ "Level",
        /* tooltipAccountBound*/ "Account Bound",
        /* tooltipVendor     */ "Vendor:",
        /* tooltipGold       */ "g",
        /* tooltipSilver     */ "s",
        /* tooltipCopper     */ "c",
        /* durDay            */ "d",
        /* durHour           */ "h",
        /* durMin            */ "min",
        /* durSec            */ "sec",
    };

    void SetLanguage(Language lang) { s_Language = lang; }
    Language GetLanguage() { return s_Language; }
    const Strings& Get() { return s_Language == Language::German ? s_German : s_English; }
    const Strings& Get(Language lang) { return lang == Language::German ? s_German : s_English; }

    const char* TranslateRarity(const char* r)
    {
        if (!r || s_Language != Language::German) return r;
        if (strcmp(r, "Legendary")  == 0) return "Legend\xc3\xa4r";
        if (strcmp(r, "Ascended")   == 0) return "Aufgestiegen";
        if (strcmp(r, "Exotic")     == 0) return "Exotisch";
        if (strcmp(r, "Rare")       == 0) return "Selten";
        if (strcmp(r, "Masterwork") == 0) return "Meisterwerk";
        if (strcmp(r, "Fine")       == 0) return "Fein";
        if (strcmp(r, "Basic")      == 0) return "Einfach";
        if (strcmp(r, "Junk")       == 0) return "Schrott";
        return r;
    }

    const char* TranslateItemType(const char* t)
    {
        if (!t || s_Language != Language::German) return t;
        if (strcmp(t, "Armor")              == 0) return "R\xc3\xbcstung";
        if (strcmp(t, "Back")               == 0) return "R\xc3\xbc" "cken";
        if (strcmp(t, "Bag")                == 0) return "Tasche";
        if (strcmp(t, "Consumable")         == 0) return "Verbrauchsgegenstand";
        if (strcmp(t, "Container")          == 0) return "Beh\xc3\xa4lter";
        if (strcmp(t, "CraftingMaterial")   == 0) return "Handwerksmaterial";
        if (strcmp(t, "Gathering")          == 0) return "Sammelwerkzeug";
        if (strcmp(t, "Gizmo")              == 0) return "Gadget";
        if (strcmp(t, "Key")                == 0) return "Schl\xc3\xbcssel";
        if (strcmp(t, "MiniPet")            == 0) return "Miniatur";
        if (strcmp(t, "Tool")               == 0) return "Werkzeug";
        if (strcmp(t, "Trait")              == 0) return "Eigenschaft";
        if (strcmp(t, "Trinket")            == 0) return "Schmuck";
        if (strcmp(t, "Trophy")             == 0) return "Troph\xc3\xa4" "e";
        if (strcmp(t, "UpgradeComponent")   == 0) return "Aufwertungskomponente";
        if (strcmp(t, "Weapon")             == 0) return "Waffe";
        if (strcmp(t, "PowerCore")          == 0) return "Energiekern";
        if (strcmp(t, "Power Core")         == 0) return "Energiekern";
        if (strcmp(t, "JadeTechModule")     == 0) return "Jadetech-Modul";
        if (strcmp(t, "Relic")              == 0) return "Relikt";
        return t;
    }

    const char* TranslateWeaponType(const char* s)
    {
        if (!s) return s;
        if (s_Language == Language::German)
        {
            if (strcmp(s, "Axe")        == 0) return "Axt";
            if (strcmp(s, "Dagger")     == 0) return "Dolch";
            if (strcmp(s, "Focus")      == 0) return "Fokus";
            if (strcmp(s, "Greatsword") == 0) return "Gro\xc3\x9fschwert";
            if (strcmp(s, "Hammer")     == 0) return "Hammer";
            if (strcmp(s, "Harpoon")    == 0) return "Harpune";
            if (strcmp(s, "LongBow")    == 0) return "Langbogen";
            if (strcmp(s, "Mace")       == 0) return "Keule";
            if (strcmp(s, "Pistol")     == 0) return "Pistole";
            if (strcmp(s, "Rifle")      == 0) return "Gewehr";
            if (strcmp(s, "Scepter")    == 0) return "Zepter";
            if (strcmp(s, "Shield")     == 0) return "Schild";
            if (strcmp(s, "ShortBow")   == 0) return "Kurzbogen";
            if (strcmp(s, "Spear")      == 0) return "Speer";
            if (strcmp(s, "Speargun")   == 0) return "Harpunenschleuder";
            if (strcmp(s, "Staff")      == 0) return "Stab";
            if (strcmp(s, "Sword")      == 0) return "Schwert";
            if (strcmp(s, "Torch")      == 0) return "Fackel";
            if (strcmp(s, "Trident")    == 0) return "Dreizack";
            if (strcmp(s, "Warhorn")    == 0) return "Kriegshorn";
        }
        return s;
    }

    const char* TranslateArmorSlot(const char* s)
    {
        if (!s) return s;
        if (s_Language == Language::German)
        {
            if (strcmp(s, "Helm")         == 0) return "Helm";
            if (strcmp(s, "HelmAquatic")  == 0) return "Unterwasseratmer";
            if (strcmp(s, "Shoulders")    == 0) return "Schultern";
            if (strcmp(s, "Coat")         == 0) return "Brust";
            if (strcmp(s, "Gloves")       == 0) return "Handschuhe";
            if (strcmp(s, "Leggings")     == 0) return "Hose";
            if (strcmp(s, "Boots")        == 0) return "Stiefel";
        }
        return s;
    }

    const char* TranslateWeightClass(const char* s)
    {
        if (!s) return s;
        if (s_Language == Language::German)
        {
            if (strcmp(s, "Heavy")    == 0) return "Schwere";
            if (strcmp(s, "Medium")   == 0) return "Mittlere";
            if (strcmp(s, "Light")    == 0) return "Leichte";
            if (strcmp(s, "Clothing") == 0) return "Kleidung";
        }
        else
        {
            if (strcmp(s, "Heavy")    == 0) return "Heavy";
            if (strcmp(s, "Medium")   == 0) return "Medium";
            if (strcmp(s, "Light")    == 0) return "Light";
            if (strcmp(s, "Clothing") == 0) return "Clothing";
        }
        return s;
    }

    const char* TranslateAttribute(const char* s)
    {
        if (!s) return s;
        // Attribute keys as returned by /v2/itemstats and character equipment
        if (s_Language == Language::German)
        {
            if (strcmp(s, "Power")             == 0) return "Kraft";
            if (strcmp(s, "Precision")         == 0) return "Pr\xc3\xa4zision";
            if (strcmp(s, "Toughness")         == 0) return "Z\xc3\xa4higkeit";
            if (strcmp(s, "Vitality")          == 0) return "Vitalit\xc3\xa4t";
            if (strcmp(s, "ConditionDamage")   == 0) return "Zustandsschaden";
            if (strcmp(s, "ConditionDuration") == 0) return "Fachkenntnis";       // Expertise
            if (strcmp(s, "Expertise")         == 0) return "Fachkenntnis";
            if (strcmp(s, "BoonDuration")      == 0) return "Konzentration";      // Concentration
            if (strcmp(s, "Concentration")     == 0) return "Konzentration";
            if (strcmp(s, "CritDamage")        == 0) return "Wildheit";           // Ferocity
            if (strcmp(s, "Ferocity")          == 0) return "Wildheit";
            if (strcmp(s, "Healing")           == 0) return "Heilkraft";
            if (strcmp(s, "HealingPower")      == 0) return "Heilkraft";
            if (strcmp(s, "AgonyResistance")   == 0) return "Qual-Widerstand";
            return s;
        }
        // English: prefer the in-game display names
        if (strcmp(s, "ConditionDamage")   == 0) return "Condition Damage";
        if (strcmp(s, "ConditionDuration") == 0) return "Expertise";
        if (strcmp(s, "BoonDuration")      == 0) return "Concentration";
        if (strcmp(s, "CritDamage")        == 0) return "Ferocity";
        if (strcmp(s, "Healing")           == 0) return "Healing Power";
        if (strcmp(s, "AgonyResistance")   == 0) return "Agony Resistance";
        return s;
    }

    const char* GetWeaponHandedness(const char* s)
    {
        if (!s) return "";
        // Two-handed
        if (strcmp(s, "Greatsword") == 0 || strcmp(s, "Hammer")   == 0 ||
            strcmp(s, "LongBow")    == 0 || strcmp(s, "Rifle")     == 0 ||
            strcmp(s, "ShortBow")   == 0 || strcmp(s, "Staff")     == 0 ||
            strcmp(s, "Speargun")   == 0 || strcmp(s, "Spear")     == 0 ||
            strcmp(s, "Trident")    == 0 || strcmp(s, "Harpoon")   == 0)
            return s_Language == Language::German ? "Zweihand" : "Two-Handed";
        // Off-hand only
        if (strcmp(s, "Focus")   == 0 || strcmp(s, "Shield")  == 0 ||
            strcmp(s, "Torch")   == 0 || strcmp(s, "Warhorn") == 0)
            return s_Language == Language::German ? "Begleithand" : "Off-Hand";
        // One-handed main: Axe, Dagger, Mace, Pistol, Scepter, Sword
        return s_Language == Language::German ? "Einhand" : "One-Handed";
    }
}

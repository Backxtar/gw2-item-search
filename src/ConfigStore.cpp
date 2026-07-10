#include "ConfigStore.h"
#include "Constants.h"
#include "Lang.h"
#include "SharedState.h"
#include "Utility.h"
#include <cstring>
#include <direct.h>
#include <fstream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace ItemSearch
{
    ConfigStore::ConfigStore() = default;

    void ConfigStore::Load(AppState& state)
    {
        std::ifstream file(Constants::SettingsFile);
        if (!file.is_open()) return;

        try
        {
            json data;
            file >> data;

            PluginConfig config;
            const std::string key = data.value("apiKey", "");
            strncpy_s(config.apiKey, sizeof(config.apiKey), key.c_str(), _TRUNCATE);
            config.language    = data.value("language", 1);
            config.fontSize    = data.value("fontSize", 16.0f);
            config.headingSize = data.value("headingSize", 20.0f);
            config.buttonSize  = data.value("buttonSize", 16.0f);
            config.tooltipSize = data.value("tooltipSize", 16.0f);

            Lang::SetLanguage(static_cast<Lang::Language>(config.language));
            SetConfig(state, config);

            strncpy_s(m_EditApiKey.data(), m_EditApiKey.size(), config.apiKey, _TRUNCATE);
            m_EditLanguage    = config.language;
            m_EditFontSize    = config.fontSize;
            m_EditHeadingSize = config.headingSize;
            m_EditButtonSize  = config.buttonSize;
            m_EditTooltipSize = config.tooltipSize;
            m_EditShowWindow = data.value("showWindow", true);
            m_CachedAccountName = data.value("accountName", "");

            state.showWindow.store(m_EditShowWindow, std::memory_order_relaxed);

            if (!m_CachedAccountName.empty())
            {
                std::unique_lock sl(state.statusLock);
                state.accountName = m_CachedAccountName;
            }
        }
        catch (...) {}
    }

    void ConfigStore::Save(const AppState& state) const
    {
        _mkdir("addons");
        _mkdir(Constants::SettingsDir);

        const PluginConfig config = GetConfig(state);
        json data;
        data["apiKey"]       = config.apiKey;
        data["language"]     = config.language;
        data["fontSize"]     = config.fontSize;
        data["headingSize"]  = config.headingSize;
        data["buttonSize"]   = config.buttonSize;
        data["tooltipSize"]  = config.tooltipSize;
        data["showWindow"]   = state.showWindow.load(std::memory_order_relaxed);
        data["accountName"]  = m_CachedAccountName;

        std::ofstream file(Constants::SettingsFile);
        if (file.is_open()) file << data.dump(4);
    }

    void ConfigStore::ApplyFromEditBuffer(AppState& state)
    {
        PluginConfig next;
        strncpy_s(next.apiKey, sizeof(next.apiKey), m_EditApiKey.data(), _TRUNCATE);
        next.language    = m_EditLanguage;
        next.fontSize    = m_EditFontSize;
        next.headingSize = m_EditHeadingSize;
        next.buttonSize  = m_EditButtonSize;
        next.tooltipSize = m_EditTooltipSize;

        // Clear cached account name when API key changes
        const PluginConfig current = GetConfig(state);
        if (strcmp(current.apiKey, next.apiKey) != 0)
        {
            m_CachedAccountName.clear();
            {
                std::unique_lock sl(state.statusLock);
                state.accountName.clear();
            }
            // Clear item cache for the old key
            {
                std::unique_lock il(state.itemsLock);
                state.items.clear();
                state.itemsVersion.fetch_add(1, std::memory_order_release);
            }
            std::remove(Constants::ItemCacheFile);
        }

        Lang::SetLanguage(static_cast<Lang::Language>(next.language));
        SetConfig(state, next);
        state.showWindow.store(m_EditShowWindow, std::memory_order_relaxed);
        Save(state);
    }

    void ConfigStore::LoadItemCache(std::vector<FoundItem>& out) const
    {
        std::ifstream file(Constants::ItemCacheFile);
        if (!file.is_open()) return;
        try
        {
            const json data = json::parse(file);
            // v3: adds slotted upgrades/infusions, weapon/armor stats and
            // consumable effects — older caches would render incomplete
            // tooltips, so they are discarded (one refresh refills them).
            if (data.value("version", 0) != 3) return;
            for (const auto& j : data.value("items", json::array()))
            {
                FoundItem item;
                item.itemId        = j.value("id",           0);
                item.count         = j.value("count",        1);
                item.name          = j.value("name",         "");
                item.nameLower     = Utility::ToLower(item.name);
                item.description   = j.value("desc",         "");
                item.iconUrl       = j.value("icon",         "");
                item.type          = j.value("type",         "");
                item.rarity        = j.value("rarity",       "");
                item.level         = j.value("level",        0);
                item.accountBound  = j.value("accountBound", false);
                item.vendorValue   = j.value("vendorValue",  0);
                item.subType       = j.value("subType",      "");
                item.weightClass   = j.value("weightClass",  "");
                item.statName      = j.value("statName",     "");
                item.locationType  = static_cast<ItemLocation>(j.value("loc", 0));
                item.characterName = j.value("char",         "");
                item.characterProfession = j.value("prof",   "");
                item.characterEliteSpec  = j.value("elite",  "");
                item.characterEliteIcon  = j.value("eliteIcon", "");
                item.characterLevel      = j.value("clvl",   0);
                item.characterRace       = j.value("crace",  "");
                item.equipSlot           = j.value("slot",   "");
                item.bankSlot            = j.value("bslot",  -1);
                item.containerSize       = j.value("csize",  0);
                item.skinName            = j.value("skin",   "");
                item.equipTabIdx         = j.value("etab",   -1);
                item.equipTabName        = j.value("etabn",  "");
                item.equipTabActive      = j.value("etaba",  false);
                for (const auto& a : j.value("attrs", json::array()))
                    item.attributes.emplace_back(a.value("a", ""), a.value("v", 0));
                item.defense           = j.value("def",   0);
                item.minPower          = j.value("pmin",  0);
                item.maxPower          = j.value("pmax",  0);
                item.buffDescription   = j.value("buff",  "");
                for (const auto& b : j.value("bon", json::array()))
                    if (b.is_string()) item.bonuses.push_back(b.get<std::string>());
                item.consumableDesc    = j.value("cdesc", "");
                item.consumableIconUrl = j.value("cicon", "");
                item.durationMs        = j.value("cdur",  0);
                for (const auto& u : j.value("ups", json::array()))
                {
                    EmbeddedUpgrade eu;
                    eu.itemId          = u.value("id",  0);
                    eu.isInfusion      = u.value("inf", false);
                    eu.name            = u.value("n",   "");
                    eu.rarity          = u.value("r",   "");
                    eu.iconUrl         = u.value("i",   "");
                    eu.buffDescription = u.value("b",   "");
                    for (const auto& b : u.value("bs", json::array()))
                        if (b.is_string()) eu.bonuses.push_back(b.get<std::string>());
                    if (eu.itemId > 0) item.upgradeSlots.push_back(std::move(eu));
                }
                // Rebuild the search key from stat prefix + original + skin name
                {
                    std::string searchable = item.name;
                    if (!item.skinName.empty() && item.skinName != item.name)
                        searchable += " " + item.skinName;
                    if (!item.statName.empty())
                        searchable = item.statName + " " + searchable;
                    item.nameLower = Utility::ToLower(searchable);
                }
                if (item.itemId > 0) out.push_back(std::move(item));
            }
        }
        catch (...) {}
    }

    void ConfigStore::SaveItemCache(const std::vector<FoundItem>& items) const
    {
        _mkdir("addons");
        _mkdir(Constants::SettingsDir);

        json arr = json::array();
        for (const auto& item : items)
        {
            json j;
            j["id"]          = item.itemId;
            j["count"]       = item.count;
            j["name"]        = item.name;
            j["desc"]        = item.description;
            j["icon"]        = item.iconUrl;
            j["type"]        = item.type;
            j["rarity"]      = item.rarity;
            j["level"]       = item.level;
            j["accountBound"]= item.accountBound;
            j["vendorValue"] = item.vendorValue;
            j["subType"]     = item.subType;
            j["weightClass"] = item.weightClass;
            j["statName"]    = item.statName;
            j["loc"]         = static_cast<int>(item.locationType);
            j["char"]        = item.characterName;
            if (!item.characterProfession.empty()) j["prof"] = item.characterProfession;
            if (!item.characterEliteSpec.empty())  j["elite"] = item.characterEliteSpec;
            if (!item.characterEliteIcon.empty())  j["eliteIcon"] = item.characterEliteIcon;
            if (item.characterLevel > 0)           j["clvl"] = item.characterLevel;
            if (!item.characterRace.empty())       j["crace"] = item.characterRace;
            if (!item.equipSlot.empty())           j["slot"]  = item.equipSlot;
            if (item.bankSlot >= 0)                j["bslot"] = item.bankSlot;
            if (item.containerSize > 0)            j["csize"] = item.containerSize;
            if (!item.skinName.empty())            j["skin"]  = item.skinName;
            if (item.equipTabIdx >= 0)
            {
                j["etab"]  = item.equipTabIdx;
                if (!item.equipTabName.empty()) j["etabn"] = item.equipTabName;
                if (item.equipTabActive)        j["etaba"] = true;
            }
            if (!item.attributes.empty())
            {
                json attrs = json::array();
                for (const auto& [a, v] : item.attributes)
                    attrs.push_back(json{{"a", a}, {"v", v}});
                j["attrs"] = std::move(attrs);
            }
            if (item.defense  > 0)                 j["def"]   = item.defense;
            if (item.minPower > 0)                 j["pmin"]  = item.minPower;
            if (item.maxPower > 0)                 j["pmax"]  = item.maxPower;
            if (!item.buffDescription.empty())     j["buff"]  = item.buffDescription;
            if (!item.bonuses.empty())             j["bon"]   = item.bonuses;
            if (!item.consumableDesc.empty())      j["cdesc"] = item.consumableDesc;
            if (!item.consumableIconUrl.empty())   j["cicon"] = item.consumableIconUrl;
            if (item.durationMs > 0)               j["cdur"]  = item.durationMs;
            if (!item.upgradeSlots.empty())
            {
                json ups = json::array();
                for (const auto& eu : item.upgradeSlots)
                {
                    json u;
                    u["id"] = eu.itemId;
                    if (eu.isInfusion)                  u["inf"] = true;
                    if (!eu.name.empty())               u["n"]   = eu.name;
                    if (!eu.rarity.empty())             u["r"]   = eu.rarity;
                    if (!eu.iconUrl.empty())            u["i"]   = eu.iconUrl;
                    if (!eu.buffDescription.empty())    u["b"]   = eu.buffDescription;
                    if (!eu.bonuses.empty())            u["bs"]  = eu.bonuses;
                    ups.push_back(std::move(u));
                }
                j["ups"] = std::move(ups);
            }
            arr.push_back(std::move(j));
        }
        json data;
        data["version"] = 3;
        data["items"]   = std::move(arr);

        std::ofstream file(Constants::ItemCacheFile);
        if (file.is_open()) file << data.dump();
    }

    char*        ConfigStore::ApiKeyBuffer()      { return m_EditApiKey.data(); }
    int32_t&     ConfigStore::Language()          { return m_EditLanguage; }
    bool&        ConfigStore::ShowWindow()        { return m_EditShowWindow; }
    float&       ConfigStore::FontSize()          { return m_EditFontSize; }
    float&       ConfigStore::HeadingSize()       { return m_EditHeadingSize; }
    float&       ConfigStore::ButtonSize()        { return m_EditButtonSize; }
    float&       ConfigStore::TooltipSize()       { return m_EditTooltipSize; }
    std::string& ConfigStore::CachedAccountName() { return m_CachedAccountName; }
}

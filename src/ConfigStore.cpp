#include "ConfigStore.h"
#include "Constants.h"
#include "Lang.h"
#include "SharedState.h"
#include "Utility.h"
#include <cstdio>
#include <cstring>
#include <direct.h>
#include <fstream>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace ItemSearch
{
    ConfigStore::ConfigStore() = default;

    // FNV-1a hash of the API key; used as a stable per-account cache filename
    // so the key itself never appears on disk in the filename.
    std::string ConfigStore::CacheFileFor(const std::string& apiKey)
    {
        uint64_t h = 1469598103934665603ull;
        for (const unsigned char c : apiKey)
        {
            h ^= c;
            h *= 1099511628211ull;
        }
        char buf[17];
        std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(h));
        return std::string(Constants::ItemCachePrefix) + buf + ".json";
    }

    void ConfigStore::Load(AppState& state)
    {
        std::ifstream file(Constants::SettingsFile);
        if (!file.is_open()) return;

        try
        {
            json data;
            file >> data;

            PluginConfig config;
            if (data.contains("accounts"))
            {
                for (const auto& a : data.value("accounts", json::array()))
                {
                    AccountConfig acc;
                    acc.apiKey      = a.value("apiKey", "");
                    acc.accountName = a.value("accountName", "");
                    if (!acc.apiKey.empty()) config.accounts.push_back(std::move(acc));
                }
                config.activeAccount = data.value("activeAccount", 0);
            }
            else
            {
                // Legacy single-account settings: migrate key + cached name and
                // move the old shared item cache to the per-account file.
                AccountConfig acc;
                acc.apiKey      = data.value("apiKey", "");
                acc.accountName = data.value("accountName", "");
                if (!acc.apiKey.empty())
                {
                    std::rename(Constants::ItemCacheFile, CacheFileFor(acc.apiKey).c_str());
                    config.accounts.push_back(std::move(acc));
                }
            }
            if (config.activeAccount < 0 ||
                config.activeAccount >= static_cast<int32_t>(config.accounts.size()))
                config.activeAccount = 0;

            config.language    = data.value("language", 1);
            config.fontSize    = data.value("fontSize", 16.0f);
            config.headingSize = data.value("headingSize", 20.0f);
            config.buttonSize  = data.value("buttonSize", 16.0f);
            config.tooltipSize = data.value("tooltipSize", 16.0f);

            Lang::SetLanguage(static_cast<Lang::Language>(config.language));

            m_EditApiKeys.clear();
            for (const auto& acc : config.accounts)
            {
                ApiKeyBuf buf{};
                strncpy_s(buf.data(), buf.size(), acc.apiKey.c_str(), _TRUNCATE);
                m_EditApiKeys.push_back(buf);
            }
            m_EditLanguage    = config.language;
            m_EditFontSize    = config.fontSize;
            m_EditHeadingSize = config.headingSize;
            m_EditButtonSize  = config.buttonSize;
            m_EditTooltipSize = config.tooltipSize;
            m_EditShowWindow  = data.value("showWindow", true);

            state.showWindow.store(m_EditShowWindow, std::memory_order_relaxed);

            if (const AccountConfig* active = config.Active(); active && !active->accountName.empty())
            {
                std::unique_lock sl(state.statusLock);
                state.accountName = active->accountName;
            }

            SetConfig(state, config);
        }
        catch (...) {}
    }

    void ConfigStore::Save(const AppState& state) const
    {
        _mkdir("addons");
        _mkdir(Constants::SettingsDir);

        const PluginConfig config = GetConfig(state);
        json data;
        json accounts = json::array();
        for (const auto& acc : config.accounts)
        {
            json a;
            a["apiKey"]      = acc.apiKey;
            a["accountName"] = acc.accountName;
            accounts.push_back(std::move(a));
        }
        data["accounts"]      = std::move(accounts);
        data["activeAccount"] = config.activeAccount;
        data["language"]     = config.language;
        data["fontSize"]     = config.fontSize;
        data["headingSize"]  = config.headingSize;
        data["buttonSize"]   = config.buttonSize;
        data["tooltipSize"]  = config.tooltipSize;
        data["showWindow"]   = state.showWindow.load(std::memory_order_relaxed);

        std::ofstream file(Constants::SettingsFile);
        if (file.is_open()) file << data.dump(4);
    }

    void ConfigStore::ApplyFromEditBuffer(AppState& state)
    {
        const PluginConfig current = GetConfig(state);

        PluginConfig next;
        next.language    = m_EditLanguage;
        next.fontSize    = m_EditFontSize;
        next.headingSize = m_EditHeadingSize;
        next.buttonSize  = m_EditButtonSize;
        next.tooltipSize = m_EditTooltipSize;

        // Rebuild the account list from the edit buffers; empty rows are
        // dropped, cached account names are carried over by key.
        for (const auto& buf : m_EditApiKeys)
        {
            AccountConfig acc;
            acc.apiKey.assign(buf.data());
            if (acc.apiKey.empty()) continue;
            for (const auto& old : current.accounts)
                if (old.apiKey == acc.apiKey) { acc.accountName = old.accountName; break; }
            next.accounts.push_back(std::move(acc));
        }

        // Keep the same account active if its key survived the edit.
        const std::string prevActiveKey = current.ActiveKey();
        next.activeAccount = 0;
        for (size_t i = 0; i < next.accounts.size(); ++i)
            if (next.accounts[i].apiKey == prevActiveKey)
            {
                next.activeAccount = static_cast<int32_t>(i);
                break;
            }

        // Delete cache files of removed accounts.
        for (const auto& old : current.accounts)
        {
            bool kept = false;
            for (const auto& acc : next.accounts)
                if (acc.apiKey == old.apiKey) { kept = true; break; }
            if (!kept) std::remove(CacheFileFor(old.apiKey).c_str());
        }

        // Active key changed (removed or replaced): drop the displayed
        // account name and in-memory items; the follow-up refresh refills.
        if (next.ActiveKey() != prevActiveKey)
        {
            const AccountConfig* active = next.Active();
            {
                std::unique_lock sl(state.statusLock);
                state.accountName = active ? active->accountName : "";
            }
            {
                std::unique_lock il(state.itemsLock);
                state.items.clear();
                state.itemsVersion.fetch_add(1, std::memory_order_release);
            }
        }

        Lang::SetLanguage(static_cast<Lang::Language>(next.language));
        SetConfig(state, next);
        state.showWindow.store(m_EditShowWindow, std::memory_order_relaxed);
        Save(state);
    }

    void ConfigStore::LoadItemCache(const std::string& apiKey, std::vector<FoundItem>& out) const
    {
        if (apiKey.empty()) return;
        std::ifstream file(CacheFileFor(apiKey));
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

    void ConfigStore::SaveItemCache(const std::string& apiKey, const std::vector<FoundItem>& items) const
    {
        if (apiKey.empty()) return;
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

        std::ofstream file(CacheFileFor(apiKey));
        if (file.is_open()) file << data.dump();
    }

    std::vector<ConfigStore::ApiKeyBuf>& ConfigStore::ApiKeyBuffers() { return m_EditApiKeys; }

    void ConfigStore::AddAccountBuffer()
    {
        m_EditApiKeys.emplace_back(ApiKeyBuf{});
    }

    void ConfigStore::RemoveAccountBuffer(size_t index)
    {
        if (index < m_EditApiKeys.size())
            m_EditApiKeys.erase(m_EditApiKeys.begin() + index);
    }

    int32_t&     ConfigStore::Language()          { return m_EditLanguage; }
    bool&        ConfigStore::ShowWindow()        { return m_EditShowWindow; }
    float&       ConfigStore::FontSize()          { return m_EditFontSize; }
    float&       ConfigStore::HeadingSize()       { return m_EditHeadingSize; }
    float&       ConfigStore::ButtonSize()        { return m_EditButtonSize; }
    float&       ConfigStore::TooltipSize()       { return m_EditTooltipSize; }
}

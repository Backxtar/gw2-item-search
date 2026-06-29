#include "GW2ApiService.h"
#include "Constants.h"
#include "Utility.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <sstream>
#include <unordered_set>

using json = nlohmann::json;

namespace LegendaryImpactItemSearch
{
    GW2ApiService::GW2ApiService(const HttpClient& http) : m_Http(http) {}

    std::string GW2ApiService::AuthUrl(const std::string& endpoint, const std::string& apiKey) const
    {
        return std::string(Constants::GW2ApiBase) + endpoint + "?access_token=" + apiKey;
    }

    bool GW2ApiService::FetchAccountName(const std::string& apiKey, std::string& name, std::string& error) const
    {
        std::string resp;
        if (!m_Http.Get(AuthUrl("/account", apiKey), resp, error)) return false;
        try   { name = json::parse(resp).value("name", ""); }
        catch (...) { error = "Failed to parse account response."; return false; }
        return true;
    }

    bool GW2ApiService::FetchBank(const std::string& apiKey, std::vector<FoundItem>& out, std::string& error) const
    {
        std::string resp;
        if (!m_Http.Get(AuthUrl("/account/bank", apiKey), resp, error)) return false;
        try
        {
            for (const auto& slot : json::parse(resp))
            {
                if (slot.is_null()) continue;
                FoundItem item;
                item.itemId       = slot.value("id", 0);
                item.count        = slot.value("count", 1);
                item.locationType = ItemLocation::Bank;
                if (item.itemId > 0) out.push_back(std::move(item));
            }
        }
        catch (...) { error = "Failed to parse bank response."; return false; }
        return true;
    }

    bool GW2ApiService::FetchSharedInventory(const std::string& apiKey, std::vector<FoundItem>& out, std::string& error) const
    {
        std::string resp;
        if (!m_Http.Get(AuthUrl("/account/inventory", apiKey), resp, error)) return false;
        try
        {
            for (const auto& slot : json::parse(resp))
            {
                if (slot.is_null()) continue;
                FoundItem item;
                item.itemId       = slot.value("id", 0);
                item.count        = slot.value("count", 1);
                item.locationType = ItemLocation::SharedInventory;
                if (item.itemId > 0) out.push_back(std::move(item));
            }
        }
        catch (...) { error = "Failed to parse shared inventory response."; return false; }
        return true;
    }

    bool GW2ApiService::FetchMaterials(const std::string& apiKey, std::vector<FoundItem>& out, std::string& error) const
    {
        std::string resp;
        if (!m_Http.Get(AuthUrl("/account/materials", apiKey), resp, error)) return false;
        try
        {
            for (const auto& slot : json::parse(resp))
            {
                const int count = slot.value("count", 0);
                if (count <= 0) continue;
                FoundItem item;
                item.itemId       = slot.value("id", 0);
                item.count        = count;
                item.locationType = ItemLocation::Materials;
                if (item.itemId > 0) out.push_back(std::move(item));
            }
        }
        catch (...) { error = "Failed to parse materials response."; return false; }
        return true;
    }

    bool GW2ApiService::FetchCharacters(const std::string& apiKey, std::vector<std::string>& names, std::string& error) const
    {
        std::string resp;
        if (!m_Http.Get(AuthUrl("/characters", apiKey), resp, error)) return false;
        try
        {
            for (const auto& entry : json::parse(resp))
                if (entry.is_string()) names.push_back(entry.get<std::string>());
        }
        catch (...) { error = "Failed to parse characters response."; return false; }
        return true;
    }

    bool GW2ApiService::FetchCharacterInventory(const std::string& apiKey, const std::string& charName,
                                                std::vector<FoundItem>& out, std::string& error) const
    {
        const std::string url = AuthUrl("/characters/" + Utility::UrlEncode(charName) + "/inventory", apiKey);
        std::string resp;
        if (!m_Http.Get(url, resp, error)) return false;
        try
        {
            const json data = json::parse(resp);
            for (const auto& bag : data.value("bags", json::array()))
            {
                if (bag.is_null()) continue;
                for (const auto& slot : bag.value("inventory", json::array()))
                {
                    if (slot.is_null()) continue;
                    FoundItem item;
                    item.itemId        = slot.value("id", 0);
                    item.count         = slot.value("count", 1);
                    item.locationType  = ItemLocation::Character;
                    item.characterName = charName;
                    if (item.itemId > 0) out.push_back(std::move(item));
                }
            }
        }
        catch (...) { error = "Failed to parse inventory for " + charName + "."; return false; }
        return true;
    }

    bool GW2ApiService::ResolveItemNames(const std::vector<FoundItem>& items, const std::string& apiKey,
                                         const std::string& lang,
                                         std::unordered_map<int, std::string>& nameMap, std::string& error) const
    {
        std::unordered_set<int> seen;
        std::vector<int> ids;
        for (const auto& item : items)
            if (seen.insert(item.itemId).second) ids.push_back(item.itemId);

        for (size_t i = 0; i < ids.size(); i += Constants::ApiBatchSize)
        {
            const size_t end = std::min(i + static_cast<size_t>(Constants::ApiBatchSize), ids.size());
            std::ostringstream oss;
            for (size_t j = i; j < end; ++j)
            {
                if (j > i) oss << ',';
                oss << ids[j];
            }
            const std::string url = std::string(Constants::GW2ApiBase)
                + "/items?ids=" + oss.str()
                + "&lang=" + lang
                + "&access_token=" + apiKey;

            std::string resp;
            if (!m_Http.Get(url, resp, error)) return false;
            try
            {
                for (const auto& entry : json::parse(resp))
                {
                    const int id = entry.value("id", 0);
                    if (id > 0) nameMap[id] = entry.value("name", "");
                }
            }
            catch (...) { error = "Failed to parse items response."; return false; }
        }
        return true;
    }

    bool GW2ApiService::FetchAll(const std::string& apiKey, const std::string& lang,
                                  std::string& outAccountName, std::vector<FoundItem>& out,
                                  std::string& error) const
    {
        out.clear();
        outAccountName.clear();

        if (!FetchAccountName(apiKey, outAccountName, error)) return false;
        if (!FetchBank(apiKey, out, error)) return false;

        std::vector<FoundItem> shared;
        if (!FetchSharedInventory(apiKey, shared, error)) return false;
        for (auto& item : shared) out.push_back(std::move(item));

        std::vector<FoundItem> materials;
        if (!FetchMaterials(apiKey, materials, error)) return false;
        for (auto& item : materials) out.push_back(std::move(item));

        std::vector<std::string> characters;
        if (!FetchCharacters(apiKey, characters, error)) return false;
        for (const auto& charName : characters)
        {
            std::vector<FoundItem> charInv;
            if (!FetchCharacterInventory(apiKey, charName, charInv, error)) return false;
            for (auto& item : charInv) out.push_back(std::move(item));
        }

        std::unordered_map<int, std::string> nameMap;
        if (!out.empty() && !ResolveItemNames(out, apiKey, lang, nameMap, error)) return false;

        for (auto& item : out)
        {
            auto it = nameMap.find(item.itemId);
            if (it != nameMap.end()) item.name = it->second;
        }

        return true;
    }
}

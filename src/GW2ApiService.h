#pragma once
#include "HttpClient.h"
#include "Models.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ItemSearch
{
    class GW2ApiService
    {
    public:
        explicit GW2ApiService(const HttpClient& http);

        bool FetchAll(const std::string& apiKey, const std::string& lang,
                      std::string& outAccountName, std::vector<FoundItem>& out,
                      std::string& error) const;

        bool CheckPermissions(const std::string& apiKey,
                              std::vector<std::string>& missing,
                              std::string& error) const;

    private:
        bool FetchAccountName(const std::string& apiKey, std::string& name, std::string& error) const;
        bool FetchBank(const std::string& apiKey, std::vector<FoundItem>& out, std::string& error) const;
        bool FetchSharedInventory(const std::string& apiKey, std::vector<FoundItem>& out, std::string& error) const;
        bool FetchMaterials(const std::string& apiKey, std::vector<FoundItem>& out, std::string& error) const;
        bool FetchAllCharacters(const std::string& apiKey,
                                std::vector<FoundItem>& out,
                                std::unordered_map<std::string, std::vector<int>>& outCharSpecs,
                                std::vector<std::pair<std::string, std::string>>& outChars,
                                std::string& error) const;
        // All equipment template tabs for a single character (/equipmenttabs?tabs=all)
        bool FetchEquipmentTabs(const std::string& apiKey, const std::string& charName,
                                const std::string& charProf,
                                std::vector<FoundItem>& out, std::string& error) const;
        // Legendary armory unlocks (/account/legendaryarmory). Requires the optional
        // "unlocks" scope; missing scope is treated as "no items" (not an error).
        bool FetchLegendaryArmory(const std::string& apiKey,
                                  std::vector<FoundItem>& out, std::string& error) const;
        bool ResolveItemNames(const std::vector<FoundItem>& items, const std::string& apiKey,
                              const std::string& lang,
                              std::unordered_map<int, ResolvedItem>& itemMap,
                              std::string& error) const;

        bool ResolveStatNames(const std::vector<FoundItem>& items, const std::string& apiKey,
                              const std::string& lang,
                              std::unordered_map<int, StatInfo>& statMap,
                              std::string& error) const;

        // Resolves transmuted skin ids to their localized name + icon (/v2/skins).
        bool ResolveSkinNames(const std::vector<FoundItem>& items, const std::string& apiKey,
                              const std::string& lang,
                              std::unordered_map<int, SkinInfo>& skinMap,
                              std::string& error) const;

        std::string AuthUrl(const std::string& endpoint, const std::string& apiKey) const;

        const HttpClient& m_Http;
    };
}

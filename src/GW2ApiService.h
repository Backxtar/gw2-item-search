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
                                std::string& error) const;
        bool ResolveItemNames(const std::vector<FoundItem>& items, const std::string& apiKey,
                              const std::string& lang,
                              std::unordered_map<int, ResolvedItem>& itemMap,
                              std::string& error) const;

        bool ResolveStatNames(const std::vector<FoundItem>& items, const std::string& apiKey,
                              const std::string& lang,
                              std::unordered_map<int, StatInfo>& statMap,
                              std::string& error) const;

        std::string AuthUrl(const std::string& endpoint, const std::string& apiKey) const;

        const HttpClient& m_Http;
    };
}

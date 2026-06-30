#include "GW2ApiService.h"
#include "Constants.h"
#include "Utility.h"

#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <future>
#include <mutex>
#include <sstream>
#include <unordered_set>

using json = nlohmann::json;

namespace ItemSearch
{
    GW2ApiService::GW2ApiService(const HttpClient& http) : m_Http(http) {}

    // Elite specialization id -> canonical English name (matches bundled icon keys).
    // Ids are stable and already present in the characters response, so no extra
    // /v2/specializations request is needed.
    static const char* EliteSpecName(int id)
    {
        switch (id)
        {
        case 5:  return "Druid";        case 7:  return "Daredevil";   case 18: return "Berserker";
        case 27: return "Dragonhunter"; case 34: return "Reaper";      case 40: return "Chronomancer";
        case 43: return "Scrapper";     case 48: return "Tempest";     case 52: return "Herald";
        case 55: return "Soulbeast";    case 56: return "Weaver";      case 57: return "Holosmith";
        case 58: return "Deadeye";      case 59: return "Mirage";      case 60: return "Scourge";
        case 61: return "Spellbreaker"; case 62: return "Firebrand";   case 63: return "Renegade";
        case 64: return "Harbinger";    case 65: return "Willbender";  case 66: return "Virtuoso";
        case 67: return "Catalyst";     case 68: return "Bladesworn";  case 69: return "Vindicator";
        case 70: return "Mechanist";    case 71: return "Specter";     case 72: return "Untamed";
        case 73: return "Troubadour";   case 74: return "Paragon";     case 75: return "Amalgam";
        case 76: return "Ritualist";    case 77: return "Antiquary";   case 78: return "Galeshot";
        case 79: return "Conduit";      case 80: return "Evoker";      case 81: return "Luminary";
        default: return nullptr;
        }
    }

    // Reads the selected stat combination onto `item` and appends any slotted
    // upgrades (runes/sigils) and infusions as separate items in the same location.
    static void ProcessSlotDetails(const json& slot, FoundItem& item, std::vector<FoundItem>& out)
    {
        if (slot.contains("stats") && slot["stats"].is_object())
        {
            const auto& st = slot["stats"];
            item.statId = st.value("id", 0);

            // Method A: actual computed values are already present on the instance
            if (st.contains("attributes") && st["attributes"].is_object())
            {
                for (const auto& [attr, val] : st["attributes"].items())
                    if (val.is_number())
                        item.attributes.emplace_back(attr, val.get<int>());
                std::sort(item.attributes.begin(), item.attributes.end(),
                          [](const auto& a, const auto& b) { return a.second > b.second; });
            }
        }

        auto addIds = [&](const char* key, bool isInfusion)
        {
            if (!slot.contains(key) || !slot[key].is_array()) return;
            for (const auto& sub : slot[key])
            {
                if (!sub.is_number_integer()) continue;
                const int id = sub.get<int>();
                if (id <= 0) continue;

                // Searchable standalone entry
                FoundItem extra;
                extra.itemId              = id;
                extra.count               = 1;
                extra.locationType        = item.locationType;
                extra.characterName       = item.characterName;
                extra.characterProfession = item.characterProfession;
                out.push_back(std::move(extra));

                // Nested reference for the parent's tooltip (details filled later)
                EmbeddedUpgrade eu;
                eu.itemId     = id;
                eu.isInfusion = isInfusion;
                item.upgradeSlots.push_back(std::move(eu));
            }
        };
        addIds("upgrades",  false);
        addIds("infusions", true);
    }

    std::string GW2ApiService::AuthUrl(const std::string& endpoint, const std::string& apiKey) const
    {
        return std::string(Constants::GW2ApiBase) + endpoint + "?access_token=" + apiKey;
    }

    static const char* const kRequiredPermissions[] = { "account", "inventories", "characters" };

    bool GW2ApiService::CheckPermissions(const std::string& apiKey,
                                         std::vector<std::string>& missing,
                                         std::string& error) const
    {
        std::string resp;
        if (!m_Http.Get(std::string(Constants::GW2ApiBase) + "/tokeninfo?access_token=" + apiKey, resp, error))
            return false;
        try
        {
            const json data = json::parse(resp);
            std::unordered_set<std::string> have;
            for (const auto& p : data.value("permissions", json::array()))
                if (p.is_string()) have.insert(p.get<std::string>());
            for (const auto* req : kRequiredPermissions)
                if (!have.count(req)) missing.push_back(req);
        }
        catch (...) { error = "Failed to parse token info."; return false; }
        return true;
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
            const json arr = json::parse(resp);
            int slotIdx = -1;
            for (const auto& slot : arr)
            {
                ++slotIdx;
                if (slot.is_null()) continue;
                FoundItem item;
                item.itemId       = slot.value("id", 0);
                item.count        = slot.value("count", 1);
                item.locationType = ItemLocation::Bank;
                item.bankSlot     = slotIdx;
                if (item.itemId > 0)
                {
                    ProcessSlotDetails(slot, item, out);
                    out.push_back(std::move(item));
                }
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
                if (item.itemId > 0)
                {
                    ProcessSlotDetails(slot, item, out);
                    out.push_back(std::move(item));
                }
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

    bool GW2ApiService::FetchAllCharacters(const std::string& apiKey,
                                           std::vector<FoundItem>& out,
                                           std::unordered_map<std::string, std::vector<int>>& outCharSpecs,
                                           std::string& error) const
    {
        // One bulk request returns every character with bags + equipment,
        // replacing the old 1 + 2N per-character requests.
        std::string resp;
        if (!m_Http.Get(AuthUrl("/characters", apiKey) + "&ids=all", resp, error)) return false;
        try
        {
            for (const auto& chr : json::parse(resp))
            {
                if (!chr.is_object()) continue;
                const std::string charName = chr.value("name", "");
                const std::string charProf = chr.value("profession", "");

                // Active build specialization ids (needs the "builds" permission;
                // absent otherwise -> we simply fall back to the core profession).
                if (chr.contains("specializations") && chr["specializations"].contains("pve"))
                {
                    std::vector<int> specIds;
                    for (const auto& sp : chr["specializations"]["pve"])
                        if (sp.is_object() && sp.contains("id") && sp["id"].is_number_integer())
                            specIds.push_back(sp["id"].get<int>());
                    if (!specIds.empty()) outCharSpecs[charName] = std::move(specIds);
                }

                // Carried inventory (bag contents)
                for (const auto& bag : chr.value("bags", json::array()))
                {
                    if (bag.is_null()) continue;
                    for (const auto& slot : bag.value("inventory", json::array()))
                    {
                        if (slot.is_null()) continue;
                        FoundItem item;
                        item.itemId              = slot.value("id", 0);
                        item.count               = slot.value("count", 1);
                        item.locationType        = ItemLocation::Character;
                        item.characterName       = charName;
                        item.characterProfession = charProf;
                        if (item.itemId > 0)
                        {
                            ProcessSlotDetails(slot, item, out);
                            out.push_back(std::move(item));
                        }
                    }
                }

                // Equipped gear
                for (const auto& slot : chr.value("equipment", json::array()))
                {
                    if (slot.is_null()) continue;
                    const int id = slot.value("id", 0);
                    if (id <= 0) continue;
                    FoundItem item;
                    item.itemId              = id;
                    item.count               = 1;
                    item.locationType        = ItemLocation::Equipment;
                    item.characterName       = charName;
                    item.characterProfession = charProf;
                    item.equipSlot           = slot.value("slot", "");
                    ProcessSlotDetails(slot, item, out);
                    out.push_back(std::move(item));
                }
            }
        }
        catch (...) { error = "Failed to parse characters response."; return false; }
        return true;
    }

    bool GW2ApiService::ResolveItemNames(const std::vector<FoundItem>& items, const std::string& apiKey,
                                         const std::string& lang,
                                         std::unordered_map<int, ResolvedItem>& itemMap,
                                         std::string& error) const
    {
        std::unordered_set<int> seen;
        std::vector<int> ids;
        for (const auto& item : items)
        {
            if (seen.insert(item.itemId).second) ids.push_back(item.itemId);
            for (const auto& eu : item.upgradeSlots)
                if (seen.insert(eu.itemId).second) ids.push_back(eu.itemId);
        }

        // Build one URL per batch (max 200 ids per request)
        std::vector<std::string> batchUrls;
        for (size_t i = 0; i < ids.size(); i += Constants::ApiBatchSize)
        {
            const size_t end = std::min(i + static_cast<size_t>(Constants::ApiBatchSize), ids.size());
            std::ostringstream oss;
            for (size_t j = i; j < end; ++j) { if (j > i) oss << ','; oss << ids[j]; }
            batchUrls.push_back(std::string(Constants::GW2ApiBase)
                + "/items?ids=" + oss.str()
                + "&lang=" + lang
                + "&access_token=" + apiKey);
        }

        // Fetch all batches in parallel
        struct BatchResult
        {
            std::unordered_map<int, ResolvedItem> items;
            std::string error;
            bool ok = false;
        };

        std::vector<std::future<BatchResult>> futs;
        futs.reserve(batchUrls.size());
        for (const auto& url : batchUrls)
        {
            futs.push_back(std::async(std::launch::async, [this, url]() -> BatchResult
            {
                BatchResult r;
                std::string resp;
                if (!m_Http.Get(url, resp, r.error)) return r;
                try
                {
                    for (const auto& entry : json::parse(resp))
                    {
                        const int id = entry.value("id", 0);
                        if (id <= 0) continue;

                        ResolvedItem ri;
                        ri.name        = entry.value("name", "");
                        ri.icon        = entry.value("icon", "");
                        ri.desc        = Utility::StripHtml(entry.value("description", ""));
                        ri.type        = entry.value("type", "");
                        ri.rarity      = entry.value("rarity", "");
                        ri.level       = entry.value("level", 0);
                        ri.vendorValue = entry.value("vendor_value", 0);
                        if (entry.contains("flags"))
                            for (const auto& f : entry["flags"])
                                if (f.is_string() && f.get<std::string>() == "AccountBound")
                                    { ri.accountBound = true; break; }

                        if (entry.contains("details"))
                        {
                            const auto& det = entry["details"];
                            ri.subType     = det.value("type",         "");
                            ri.weightClass = det.value("weight_class", "");
                            ri.defense     = det.value("defense",      0);
                            ri.minPower    = det.value("min_power",    0);
                            ri.maxPower    = det.value("max_power",    0);
                            if (det.contains("attribute_adjustment") && det["attribute_adjustment"].is_number())
                                ri.attrAdjust = det["attribute_adjustment"].get<double>();

                            // Fixed (non-selectable) stats baked into the item
                            if (det.contains("infix_upgrade") &&
                                det["infix_upgrade"].contains("attributes") &&
                                det["infix_upgrade"]["attributes"].is_array())
                            {
                                for (const auto& a : det["infix_upgrade"]["attributes"])
                                {
                                    const std::string attr = a.value("attribute", "");
                                    const int mod          = a.value("modifier", 0);
                                    if (!attr.empty() && mod != 0)
                                        ri.infixAttributes.emplace_back(attr, mod);
                                }
                                std::sort(ri.infixAttributes.begin(), ri.infixAttributes.end(),
                                          [](const auto& a, const auto& b) { return a.second > b.second; });
                            }

                            // UpgradeComponent (rune/sigil/infusion) buff + set bonuses
                            if (det.contains("infix_upgrade") && det["infix_upgrade"].contains("buff"))
                                ri.buffDescription =
                                    Utility::StripHtml(det["infix_upgrade"]["buff"].value("description", ""));
                            if (det.contains("bonuses") && det["bonuses"].is_array())
                                for (const auto& b : det["bonuses"])
                                    if (b.is_string()) ri.bonuses.push_back(Utility::StripHtml(b.get<std::string>()));

                            // Consumable (food / utility nourishment) effect + duration
                            ri.consumableDesc = Utility::StripHtml(det.value("description", ""));
                            ri.durationMs     = det.value("duration_ms", 0);
                        }

                        r.items[id] = std::move(ri);
                    }
                    r.ok = true;
                }
                catch (...) { r.error = "Failed to parse items response."; }
                return r;
            }));
        }

        for (auto& fut : futs)
        {
            auto res = fut.get();
            if (!res.ok) { error = res.error; return false; }
            for (auto& [id, ri] : res.items) itemMap[id] = std::move(ri);
        }
        return true;
    }

    bool GW2ApiService::ResolveStatNames(const std::vector<FoundItem>& items, const std::string& apiKey,
                                         const std::string& lang,
                                         std::unordered_map<int, StatInfo>& statMap,
                                         std::string& error) const
    {
        std::unordered_set<int> seen;
        std::vector<int> ids;
        for (const auto& item : items)
            if (item.statId > 0 && seen.insert(item.statId).second) ids.push_back(item.statId);

        if (ids.empty()) return true;

        std::vector<std::string> batchUrls;
        for (size_t i = 0; i < ids.size(); i += Constants::ApiBatchSize)
        {
            const size_t end = std::min(i + static_cast<size_t>(Constants::ApiBatchSize), ids.size());
            std::ostringstream oss;
            for (size_t j = i; j < end; ++j) { if (j > i) oss << ','; oss << ids[j]; }
            batchUrls.push_back(std::string(Constants::GW2ApiBase)
                + "/itemstats?ids=" + oss.str()
                + "&lang=" + lang
                + "&access_token=" + apiKey);
        }

        struct BatchResult
        {
            std::unordered_map<int, StatInfo> stats;
            std::string error;
            bool ok = false;
        };

        std::vector<std::future<BatchResult>> futs;
        futs.reserve(batchUrls.size());
        for (const auto& url : batchUrls)
        {
            futs.push_back(std::async(std::launch::async, [this, url]() -> BatchResult
            {
                BatchResult r;
                std::string resp;
                if (!m_Http.Get(url, resp, r.error)) return r;
                try
                {
                    for (const auto& entry : json::parse(resp))
                    {
                        const int id = entry.value("id", 0);
                        if (id <= 0) continue;
                        StatInfo info;
                        info.name = entry.value("name", "");
                        if (entry.contains("attributes") && entry["attributes"].is_array())
                        {
                            for (const auto& a : entry["attributes"])
                            {
                                StatAttribute sa;
                                sa.attribute  = a.value("attribute", "");
                                sa.multiplier = a.value("multiplier", 0.0);
                                sa.value      = a.value("value", 0.0);
                                if (!sa.attribute.empty()) info.attrs.push_back(std::move(sa));
                            }
                        }
                        r.stats[id] = std::move(info);
                    }
                    r.ok = true;
                }
                catch (...) { r.error = "Failed to parse itemstats response."; }
                return r;
            }));
        }

        for (auto& fut : futs)
        {
            auto res = fut.get();
            if (!res.ok) { error = res.error; return false; }
            statMap.insert(res.stats.begin(), res.stats.end());
        }
        return true;
    }

    bool GW2ApiService::FetchAll(const std::string& apiKey, const std::string& lang,
                                  std::string& outAccountName, std::vector<FoundItem>& out,
                                  std::string& error) const
    {
        out.clear();
        outAccountName.clear();

        // --- Phase 1: all independent endpoints in parallel ---
        struct ItemResult
        {
            std::vector<FoundItem> items;
            std::unordered_map<std::string, std::vector<int>> charSpecs;
            std::string error;
            bool ok = false;
        };

        auto futAcc = std::async(std::launch::async, [this, &apiKey]()
            -> std::pair<std::string, std::string>
        {
            std::string name, err;
            if (!FetchAccountName(apiKey, name, err)) return {"", err};
            return {name, ""};
        });

        auto futBank = std::async(std::launch::async, [this, &apiKey]() -> ItemResult
        {
            ItemResult r; r.ok = FetchBank(apiKey, r.items, r.error); return r;
        });

        auto futShared = std::async(std::launch::async, [this, &apiKey]() -> ItemResult
        {
            ItemResult r; r.ok = FetchSharedInventory(apiKey, r.items, r.error); return r;
        });

        auto futMats = std::async(std::launch::async, [this, &apiKey]() -> ItemResult
        {
            ItemResult r; r.ok = FetchMaterials(apiKey, r.items, r.error); return r;
        });

        // All characters (bags + equipment) in a single bulk request
        auto futChars = std::async(std::launch::async, [this, &apiKey]() -> ItemResult
        {
            ItemResult r; r.ok = FetchAllCharacters(apiKey, r.items, r.charSpecs, r.error); return r;
        });

        // Collect phase 1 (all tasks run until here)
        auto [accName, accErr] = futAcc.get();
        auto bankRes           = futBank.get();
        auto sharedRes         = futShared.get();
        auto matsRes           = futMats.get();
        auto charsRes          = futChars.get();

        if (!accErr.empty())   { error = accErr;          return false; }
        if (!bankRes.ok)       { error = bankRes.error;   return false; }
        if (!sharedRes.ok)     { error = sharedRes.error; return false; }
        if (!matsRes.ok)       { error = matsRes.error;   return false; }
        if (!charsRes.ok)      { error = charsRes.error;  return false; }

        outAccountName = accName;
        for (auto& i : bankRes.items)   out.push_back(std::move(i));
        for (auto& i : sharedRes.items) out.push_back(std::move(i));
        for (auto& i : matsRes.items)   out.push_back(std::move(i));
        for (auto& i : charsRes.items)  out.push_back(std::move(i));

        // --- Phase 2: resolve names, icons, descriptions (batches in parallel) ---
        if (out.empty()) return true;

        std::unordered_map<int, ResolvedItem> itemMap;
        if (!ResolveItemNames(out, apiKey, lang, itemMap, error))
            return false;

        // Resolve selectable stat combinations (name + per-attribute formula)
        std::unordered_map<int, StatInfo> statMap;
        if (!ResolveStatNames(out, apiKey, lang, statMap, error))
            return false;

        // Active elite spec per character — derived from the spec ids already present
        // in the characters response (no extra API call).
        std::unordered_map<std::string, int> eliteByChar;
        for (const auto& [cn, ids] : charsRes.charSpecs)
            for (int id : ids)
                if (EliteSpecName(id)) { eliteByChar[cn] = id; break; }

        for (auto& item : out)
        {
            if (!item.characterName.empty())
                if (auto it = eliteByChar.find(item.characterName); it != eliteByChar.end())
                {
                    item.characterEliteSpecId = it->second;
                    item.characterEliteSpec   = EliteSpecName(it->second);
                }

            if (auto it = itemMap.find(item.itemId); it != itemMap.end())
            {
                const ResolvedItem& ri = it->second;
                item.name            = ri.name;
                item.iconUrl         = ri.icon;
                item.description     = ri.desc;
                item.type            = ri.type;
                item.rarity          = ri.rarity;
                item.level           = ri.level;
                item.accountBound    = ri.accountBound;
                item.vendorValue     = ri.vendorValue;
                item.subType         = ri.subType;
                item.weightClass     = ri.weightClass;
                item.defense         = ri.defense;
                item.minPower        = ri.minPower;
                item.maxPower        = ri.maxPower;
                item.buffDescription = ri.buffDescription;
                item.bonuses         = ri.bonuses;
                item.consumableDesc  = ri.consumableDesc;
                item.durationMs      = ri.durationMs;

                // Ascended food/utility has no duration in the API but always lasts 1h
                if (item.type == "Consumable" && item.rarity == "Ascended" && item.durationMs == 0)
                    item.durationMs = 3600000;
            }

            // Stat prefix name (only meaningful for selectable stats)
            if (item.statId > 0)
                if (auto it = statMap.find(item.statId); it != statMap.end())
                    item.statName = it->second.name;

            // Stat values, when the equipped instance didn't already provide them (method A).
            // Priority matches the reference project: fixed item stats first, formula last.
            if (item.attributes.empty())
            {
                if (auto ai = itemMap.find(item.itemId); ai != itemMap.end())
                {
                    const ResolvedItem& ri2 = ai->second;

                    // Method C: fixed stats baked into the item
                    // (exotic / ascended with a non-selectable prefix like "Berserker's").
                    if (!ri2.infixAttributes.empty())
                    {
                        item.attributes = ri2.infixAttributes;
                    }
                    // Method B: selectable stats (legendary etc.) -> compute via itemstat.
                    // value = round(attribute_adjustment * multiplier)   [no extra + value]
                    else if (item.statId > 0 && ri2.attrAdjust != 0.0)
                    {
                        if (auto it = statMap.find(item.statId); it != statMap.end())
                        {
                            for (const auto& sa : it->second.attrs)
                            {
                                const int v = static_cast<int>(std::lround(ri2.attrAdjust * sa.multiplier));
                                if (v != 0) item.attributes.emplace_back(sa.attribute, v);
                            }
                            std::sort(item.attributes.begin(), item.attributes.end(),
                                      [](const auto& a, const auto& b) { return a.second > b.second; });
                        }
                    }
                }
            }

            // Fill nested upgrade/infusion details for the tooltip
            for (auto& eu : item.upgradeSlots)
            {
                if (auto it = itemMap.find(eu.itemId); it != itemMap.end())
                {
                    eu.name            = it->second.name;
                    eu.rarity          = it->second.rarity;
                    eu.iconUrl         = it->second.icon;
                    eu.buffDescription = it->second.buffDescription;
                    eu.bonuses         = it->second.bonuses;
                }
            }

            // nameLower includes the stat prefix so e.g. "viper" matches equipped gear
            item.nameLower = Utility::ToLower(item.statName.empty()
                ? item.name
                : item.statName + " " + item.name);
        }

        return true;
    }
}

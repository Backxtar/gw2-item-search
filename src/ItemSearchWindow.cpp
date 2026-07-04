#include "ItemSearchWindow.h"
#include "Constants.h"
#include "Lang.h"
#include "Utility.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace ItemSearch
{
    // Accent colours used sparingly (location badges, account name)
    static const ImVec4 kGold      = { 0.90f, 0.78f, 0.42f, 1.00f };
    static const ImVec4 kLocBank   = { 0.40f, 0.65f, 1.00f, 1.00f };
    static const ImVec4 kLocShared = { 0.40f, 0.85f, 0.50f, 1.00f };
    static const ImVec4 kLocMat    = { 1.00f, 0.65f, 0.25f, 1.00f };
    static const ImVec4 kLocChar   = { 0.85f, 0.72f, 0.38f, 1.00f };
    static const ImVec4 kRed       = { 1.00f, 0.35f, 0.30f, 1.00f };

    static constexpr float kIconSize = 36.0f;

    // ---------- helpers ----------

    std::string ItemSearchWindow::GetLocationStr(const FoundItem& item)
    {
        const auto& s = Lang::Get();
        switch (item.locationType)
        {
        case ItemLocation::Bank:            return s.locBank;
        case ItemLocation::SharedInventory: return s.locSharedInventory;
        case ItemLocation::Materials:       return s.locMaterials;
        case ItemLocation::Character:       return item.characterName + " - " + s.locInventory;
        case ItemLocation::Equipment:       return item.characterName + " - " + s.locEquipment;
        case ItemLocation::LegendaryArmory: return s.locLegendaryArmory;
        default:                            return "-";
        }
    }

    // Display name prefixed with the selected stat combination (e.g. "Viper's Sword").
    // Transmuted items show their skin (current in-game appearance) name.
    static std::string DisplayName(const FoundItem& item)
    {
        const std::string base = !item.skinName.empty() ? item.skinName
                               : (item.name.empty() ? std::string("???") : item.name);
        if (item.statName.empty()) return base;
        return item.statName + " " + base;
    }

    bool ItemSearchWindow::MatchesFilter(const FoundItem& item, const char* filterLower)
    {
        if (!filterLower || filterLower[0] == '\0') return true;
        return item.nameLower.find(filterLower) != std::string::npos;
    }

    bool ItemSearchWindow::SplitUrl(const std::string& url, std::string& remote, std::string& endpoint)
    {
        const size_t schemeEnd = url.find("://");
        if (schemeEnd == std::string::npos) return false;
        const size_t pathStart = url.find('/', schemeEnd + 3);
        if (pathStart == std::string::npos) { remote = url; endpoint = "/"; return true; }
        remote   = url.substr(0, pathStart);
        endpoint = url.substr(pathStart);
        return true;
    }

    void* ItemSearchWindow::GetOrLoadTexture(const std::string& iconUrl)
    {
        if (iconUrl.empty()) return nullptr;

        // Fast path: already cached and loaded. Keyed by icon URL so transmuted items
        // (which share an item id with the untransmuted version but use the skin's
        // icon) don't collide on a shared texture.
        auto it = m_TexCache.find(iconUrl);
        if (it != m_TexCache.end() && it->second != nullptr)
            return it->second;

        if (!m_Api || !m_Api->Textures_GetOrCreateFromURL)
            return nullptr;
        std::string remote, endpoint;
        if (!SplitUrl(iconUrl, remote, endpoint)) return nullptr;
        const std::string texId = "LIIS_TEX_" + std::to_string(std::hash<std::string>{}(iconUrl));
        Texture_t* tex = m_Api->Textures_GetOrCreateFromURL(texId.c_str(), remote.c_str(), endpoint.c_str());
        void* res = tex ? tex->Resource : nullptr;
        if (res) m_TexCache[iconUrl] = res; // only cache once fully loaded
        return res;
    }

    void* ItemSearchWindow::GetClassIcon(const std::string& name)
    {
        if (name.empty()) return nullptr;
        auto it = m_ProfIcons.find(name);
        if (it != m_ProfIcons.end()) return it->second;
        if (!m_Api || !m_Api->Textures_Get) return nullptr;
        void* res = nullptr;
        if (auto* t = m_Api->Textures_Get((std::string(Constants::ProfIconPrefix) + name).c_str()))
            res = t->Resource;
        if (res) m_ProfIcons[name] = res; // cache once available
        return res;
    }

    // ---------- location colour ----------

    static ImVec4 RarityColor(const std::string& r)
    {
        if (r == "Legendary")  return { 0.70f, 0.25f, 1.00f, 1.0f };
        if (r == "Ascended")   return { 0.98f, 0.48f, 0.72f, 1.0f };
        if (r == "Exotic")     return { 1.00f, 0.65f, 0.00f, 1.0f };
        if (r == "Rare")       return { 1.00f, 0.90f, 0.10f, 1.0f };
        if (r == "Masterwork") return { 0.12f, 0.75f, 0.12f, 1.0f };
        if (r == "Fine")       return { 0.32f, 0.53f, 1.00f, 1.0f };
        if (r == "Basic")      return { 1.00f, 1.00f, 1.00f, 1.0f };
        return { 0.60f, 0.60f, 0.60f, 1.0f }; // Junk / unknown
    }

    static ImVec4 RarityColor(const FoundItem& item) { return RarityColor(item.rarity); }

    // Equipment slot -> section category (0..5) and intra-section order, from the
    // equipment slot name. Unknown equip slots fall back to the trinket section.
    static void EquipCategory(const FoundItem& it, int& cat, int& order)
    {
        struct S { const char* name; int cat; int order; };
        static const S kSlots[] = {
            {"Helm",0,0},{"Shoulders",0,1},{"Coat",0,2},{"Gloves",0,3},{"Leggings",0,4},{"Boots",0,5},
            {"WeaponA1",1,0},{"WeaponA2",1,1},{"WeaponB1",1,2},{"WeaponB2",1,3},
            {"Backpack",2,0},{"Amulet",2,1},{"Accessory1",2,2},{"Accessory2",2,3},{"Ring1",2,4},{"Ring2",2,5},{"Relic",2,6},
            {"HelmAquatic",3,0},{"WeaponAquaticA",3,1},{"WeaponAquaticB",3,2},
            {"Sickle",4,0},{"Axe",4,1},{"Pick",4,2},{"FishingRod",4,3},{"FishingBait",4,4},{"FishingLure",4,5},
            {"PowerCore",5,0},{"JadeBotCore",5,0},
        };
        for (const auto& sl : kSlots)
            if (it.equipSlot == sl.name) { cat = sl.cat; order = sl.order; return; }

        // Fall back to the item type (covers fishing gear, jade tech modules, etc.)
        const std::string& t = it.type;
        if (t == "Gathering")                                   { cat = 4; order = 50; return; }
        if (t == "PowerCore" || t == "Power Core" || t == "JadeTechModule") { cat = 5; order = 50; return; }
        if (t == "Weapon")                                      { cat = 1; order = 50; return; }
        if (t == "Armor")                                       { cat = 0; order = 50; return; }
        if (t == "Back" || t == "Trinket")                      { cat = 2; order = 50; return; }
        cat = 2; order = 90; // last resort: trinkets
    }

    // Canonical base-profession order for sorting the character tabs, so they
    // group by class instead of appearing in alphabetical-by-name order.
    static int ProfessionOrder(const std::string& p)
    {
        // Light armor first, then medium, then heavy (rest).
        if (p == "Mesmer")       return 0;
        if (p == "Necromancer")  return 1;
        if (p == "Elementalist") return 2;
        if (p == "Ranger")       return 3;
        if (p == "Engineer")     return 4;
        if (p == "Thief")        return 5;
        if (p == "Guardian")     return 6;
        if (p == "Warrior")      return 7;
        if (p == "Revenant")     return 8;
        return 9; // unknown / core-less characters last
    }

    // Signature profession colours (used for the character tab labels)
    static ImVec4 ProfessionColor(const std::string& p)
    {
        if (p == "Guardian")     return { 0.45f, 0.76f, 0.85f, 1.0f }; // light blue
        if (p == "Warrior")      return { 1.00f, 0.82f, 0.40f, 1.0f }; // gold
        if (p == "Engineer")     return { 0.82f, 0.61f, 0.38f, 1.0f }; // bronze
        if (p == "Ranger")       return { 0.56f, 0.82f, 0.43f, 1.0f }; // green
        if (p == "Thief")        return { 0.75f, 0.56f, 0.59f, 1.0f }; // dusty red
        if (p == "Elementalist") return { 0.96f, 0.54f, 0.53f, 1.0f }; // red/coral
        if (p == "Mesmer")       return { 0.71f, 0.47f, 0.84f, 1.0f }; // purple
        if (p == "Necromancer")  return { 0.32f, 0.65f, 0.44f, 1.0f }; // dark green
        if (p == "Revenant")     return { 0.82f, 0.43f, 0.35f, 1.0f }; // red-brown
        return { 0.85f, 0.72f, 0.38f, 1.0f }; // default gold
    }

    static const ImVec4 kLocEquip   = { 0.70f, 0.85f, 1.00f, 1.00f };
    static const ImVec4 kLocArmory  = { 0.70f, 0.25f, 1.00f, 1.00f }; // legendary purple

    static ImVec4 LocationColor(const FoundItem& item)
    {
        switch (item.locationType)
        {
        case ItemLocation::Bank:            return kLocBank;
        case ItemLocation::SharedInventory: return kLocShared;
        case ItemLocation::Materials:       return kLocMat;
        case ItemLocation::Equipment:       return kLocEquip;
        case ItemLocation::LegendaryArmory: return kLocArmory;
        default:                            return kLocChar;
        }
    }

    // ---------- loading spinner (public ImGui API only) ----------

    static void RenderLoadingSpinner(float radius, float thickness)
    {
        ImDrawList*  dl     = ImGui::GetWindowDrawList();
        const ImVec2 pos    = ImGui::GetCursorScreenPos();
        const float  t      = static_cast<float>(ImGui::GetTime());

        ImGui::Dummy(ImVec2(radius * 2.0f, radius * 2.0f));

        const ImVec2  centre  = ImVec2(pos.x + radius, pos.y + radius);
        constexpr int numDots = 8;

        for (int i = 0; i < numDots; ++i)
        {
            const float phase = static_cast<float>(i) / numDots;
            const float angle = phase * 6.2832f - t * 3.0f;
            const float alpha = std::fmod(phase + t * 0.8f, 1.0f);
            const float dotR  = thickness * (0.4f + 0.6f * alpha);
            const ImVec2 dp   = ImVec2(centre.x + std::cos(angle) * (radius - dotR),
                                       centre.y + std::sin(angle) * (radius - dotR));
            dl->AddCircleFilled(dp, dotR, IM_COL32(180, 160, 100, static_cast<int>(alpha * 200.0f)));
        }
    }

    // ---------- GW2-style item slot (icon + count badge) ----------

    static void DrawItemSlot(void* texResource, int count, ImDrawList* dl)
    {
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const ImVec2 br     = ImVec2(cursor.x + kIconSize, cursor.y + kIconSize);

        if (texResource)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(texResource), ImVec2(kIconSize, kIconSize));
        }
        else
        {
            // Dark placeholder that looks like an empty GW2 slot
            dl->AddRectFilled(cursor, br, IM_COL32(40, 35, 25, 200));
            const char* q  = "?";
            const ImVec2 ts = ImGui::CalcTextSize(q);
            dl->AddText(ImVec2(cursor.x + (kIconSize - ts.x) * 0.5f,
                               cursor.y + (kIconSize - ts.y) * 0.5f),
                        IM_COL32(120, 100, 60, 160), q);
            ImGui::Dummy(ImVec2(kIconSize, kIconSize));
        }

        // Amber slot border (like GW2)
        dl->AddRect(cursor, br, IM_COL32(120, 90, 35, 200), 0.0f, 0, 1.5f);

        // Count badge — dark background box + gold text for readability on any icon
        char cntBuf[12];
        snprintf(cntBuf, sizeof(cntBuf), "%d", count);
        const ImVec2 ts      = ImGui::CalcTextSize(cntBuf);
        const float  pad     = 2.0f;
        const ImVec2 bgMin   = ImVec2(br.x - ts.x - pad * 2.0f, br.y - ts.y - pad);
        const ImVec2 bgMax   = ImVec2(br.x,                       br.y);
        const ImVec2 tPos    = ImVec2(bgMin.x + pad, bgMin.y + pad * 0.5f);
        dl->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 180), 2.0f);
        dl->AddText(tPos, IM_COL32(255, 220, 100, 255), cntBuf);
    }

    // ---------- item tooltip ----------

    void ItemSearchWindow::ShowItemTooltip(const FoundItem& item, void* texIcon) const
    {
        const auto& s = Lang::Get();

        // Lazily cache coin textures (first time tooltip opens after load)
        if (!m_CoinGold && m_Api && m_Api->Textures_Get)
        {
            if (auto* t = m_Api->Textures_Get(Constants::CoinGoldId))
                const_cast<ItemSearchWindow*>(this)->m_CoinGold = t->Resource;
            if (auto* t = m_Api->Textures_Get(Constants::CoinSilverId))
                const_cast<ItemSearchWindow*>(this)->m_CoinSilver = t->Resource;
            if (auto* t = m_Api->Textures_Get(Constants::CoinCopperId))
                const_cast<ItemSearchWindow*>(this)->m_CoinCopper = t->Resource;
            if (auto* t = m_Api->Textures_Get(Constants::FoodIconId))
                const_cast<ItemSearchWindow*>(this)->m_FoodIcon = t->Resource;
            if (auto* t = m_Api->Textures_Get(Constants::UtilityIconId))
                const_cast<ItemSearchWindow*>(this)->m_UtilityIcon = t->Resource;
        }

        auto* self = const_cast<ItemSearchWindow*>(this);
        static const ImVec4 kAttr     = { 0.55f, 0.85f, 0.55f, 1.0f }; // attribute green
        static const ImVec4 kBuff     = { 0.55f, 0.78f, 1.00f, 1.0f }; // upgrade buff blue
        static const ImVec4 kBonus    = { 0.78f, 0.78f, 0.78f, 1.0f }; // bonus text
        const ImVec4 kBonusNum = kAttr; // bonus index (n) — same green as the stat values

        // Renders rune/sigil set bonuses with a coloured "(n)" index for readability
        auto renderBonuses = [&](const std::vector<std::string>& bonuses)
        {
            for (size_t i = 0; i < bonuses.size(); ++i)
            {
                ImGui::TextColored(kBonusNum, "(%zu)", i + 1);
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::TextColored(kBonus, "%s", bonuses[i].c_str());
            }
        };

        ImGui::BeginTooltip();

        // ── Header: icon + name ──────────────────────────────────────────────
        if (texIcon)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(texIcon), ImVec2(32.0f, 32.0f));
            ImGui::SameLine(0.0f, 8.0f);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (32.0f - ImGui::GetTextLineHeight()) * 0.5f);
        }
        ImGui::TextColored(RarityColor(item), "%s", DisplayName(item).c_str());

        const bool isArmor  = (item.type == "Armor");
        const bool isWeapon = (item.type == "Weapon");

        // ── If the item itself is a rune/sigil/infusion: its buff + set bonuses ─
        if (!item.buffDescription.empty() || !item.bonuses.empty())
        {
            ImGui::Spacing();
            if (!item.buffDescription.empty())
                ImGui::TextColored(kBuff, "%s", item.buffDescription.c_str());
            renderBonuses(item.bonuses);
        }

        // ── Stats: defense / weapon strength / attributes ────────────────────
        if (isArmor && item.defense > 0)
            ImGui::Text("%s %d", s.tooltipDefense, item.defense);
        if (isWeapon && item.maxPower > 0)
            ImGui::Text("%s %d - %d", s.tooltipWeaponStrength, item.minPower, item.maxPower);

        if (!item.statName.empty())
        {
            ImGui::Text("%s", s.tooltipStats);
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::TextColored(kGold, "%s", item.statName.c_str());
        }
        for (const auto& [attr, val] : item.attributes)
            ImGui::TextColored(kAttr, "+%d %s", val, Lang::TranslateAttribute(attr.c_str()));

        // Consumable effect (food / utility nourishment) with duration.
        // Ascended food often has no description/duration in the API -> still show
        // the food icon + the 1h duration filled in by the service.
        if (item.type == "Consumable" && (item.durationMs > 0 || !item.consumableDesc.empty()))
        {
            ImGui::Spacing();
            void* nIcon = (item.subType == "Utility")                       ? m_UtilityIcon
                        : (item.subType == "Food" || item.rarity == "Ascended") ? m_FoodIcon
                                                                                : nullptr;
            if (nIcon)
            {
                const float isz = ImGui::GetTextLineHeight() * 1.6f;
                ImGui::Image(reinterpret_cast<ImTextureID>(nIcon), ImVec2(isz, isz));
                ImGui::SameLine(0.0f, 5.0f); // icon sits right before the duration
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (isz - ImGui::GetTextLineHeight()) * 0.5f);
            }
            if (item.durationMs > 0)
                ImGui::TextColored(kBuff, "(%d min)", item.durationMs / 60000);
            if (!item.consumableDesc.empty())
                ImGui::TextColored(kAttr, "%s", item.consumableDesc.c_str());
        }

        // ── Slotted upgrades / infusions (runes, sigils, infusions) ──────────
        if (!item.upgradeSlots.empty())
        {
            ImGui::Spacing();
            bool sawNonInfusion = false, spacedBeforeInfusion = false;
            for (const auto& eu : item.upgradeSlots)
            {
                // Visual gap between upgrades (runes/sigils) and infusions
                if (eu.isInfusion && sawNonInfusion && !spacedBeforeInfusion)
                {
                    ImGui::Spacing();
                    ImGui::Spacing();
                    spacedBeforeInfusion = true;
                }
                if (!eu.isInfusion) sawNonInfusion = true;

                void* utex = self->GetOrLoadTexture(eu.iconUrl);
                if (utex)
                {
                    ImGui::Image(reinterpret_cast<ImTextureID>(utex), ImVec2(20.0f, 20.0f));
                    ImGui::SameLine(0.0f, 6.0f);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (20.0f - ImGui::GetTextLineHeight()) * 0.5f);
                }
                ImGui::TextColored(RarityColor(eu.rarity), "%s", eu.name.empty() ? "???" : eu.name.c_str());
                if (!eu.buffDescription.empty())
                    ImGui::TextColored(kBuff, "%s", eu.buffDescription.c_str());
                renderBonuses(eu.bonuses);
            }
        }

        // ── Footer separator ─────────────────────────────────────────────────
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const char* rarityStr = Lang::TranslateRarity(item.rarity.c_str());

        // Type label — for armor prefix with weight class
        std::string typeLabel;
        if (isArmor && !item.weightClass.empty())
        {
            const char* wc = Lang::TranslateWeightClass(item.weightClass.c_str());
            typeLabel = std::string(wc) + " " + Lang::TranslateItemType(item.type.c_str());
        }
        else
        {
            const char* ts = Lang::TranslateItemType(item.type.c_str());
            if (ts) typeLabel = ts;
        }

        // Transmuted: original item name (the header already shows the skin's name)
        if (!item.skinName.empty() && item.skinName != item.name && !item.name.empty())
        {
            static const ImVec4 kTransmute = { 0.72f, 0.60f, 0.86f, 1.0f }; // soft purple
            ImGui::TextColored(kTransmute, "%s", s.tooltipTransmuted);
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::TextUnformatted(item.name.c_str());
        }

        // Rarity | Type
        const bool hasRarity = rarityStr && rarityStr[0] != '\0';
        const bool hasType   = !typeLabel.empty();
        if (hasRarity)
        {
            ImGui::TextColored(RarityColor(item), "%s", rarityStr);
            if (hasType)
            {
                ImGui::SameLine(0.0f, 6.0f);
                ImGui::TextDisabled("|");
                ImGui::SameLine(0.0f, 6.0f);
                ImGui::TextUnformatted(typeLabel.c_str());
            }
        }
        else if (hasType)
        {
            ImGui::TextUnformatted(typeLabel.c_str());
        }

        // Weapon subtype + handedness
        if (isWeapon && !item.subType.empty())
        {
            ImGui::TextDisabled("%s (%s)",
                Lang::TranslateWeaponType(item.subType.c_str()),
                Lang::GetWeaponHandedness(item.subType.c_str()));
        }

        // Armor slot
        if (isArmor && !item.subType.empty())
            ImGui::TextDisabled("%s", Lang::TranslateArmorSlot(item.subType.c_str()));

        // Description (only breaks on real newlines, never auto-wrapped)
        if (!item.description.empty())
            ImGui::TextUnformatted(item.description.c_str());

        // Level
        if (item.level > 0)
            ImGui::TextDisabled("%s %d", s.tooltipLevel, item.level);

        // Account Bound
        if (item.accountBound)
            ImGui::TextDisabled("%s", s.tooltipAccountBound);

        // Vendor value with coin icons
        if (item.vendorValue > 0)
        {
            const int gold   = item.vendorValue / 10000;
            const int silver = (item.vendorValue % 10000) / 100;
            const int copper = item.vendorValue % 100;
            const float iconSz = ImGui::GetTextLineHeight();

            ImGui::Text("%s", s.tooltipVendor);
            if (gold > 0)
            {
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::TextColored({1.00f, 0.85f, 0.20f, 1.0f}, "%d", gold);
                ImGui::SameLine(0.0f, 2.0f);
                if (m_CoinGold)
                    ImGui::Image(reinterpret_cast<ImTextureID>(m_CoinGold), ImVec2(iconSz, iconSz));
                else
                    ImGui::TextColored({1.00f, 0.85f, 0.20f, 1.0f}, "%s", s.tooltipGold);
            }
            if (silver > 0)
            {
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::TextColored({0.80f, 0.80f, 0.80f, 1.0f}, "%d", silver);
                ImGui::SameLine(0.0f, 2.0f);
                if (m_CoinSilver)
                    ImGui::Image(reinterpret_cast<ImTextureID>(m_CoinSilver), ImVec2(iconSz, iconSz));
                else
                    ImGui::TextColored({0.80f, 0.80f, 0.80f, 1.0f}, "%s", s.tooltipSilver);
            }
            if (copper > 0)
            {
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::TextColored({0.80f, 0.50f, 0.20f, 1.0f}, "%d", copper);
                ImGui::SameLine(0.0f, 2.0f);
                if (m_CoinCopper)
                    ImGui::Image(reinterpret_cast<ImTextureID>(m_CoinCopper), ImVec2(iconSz, iconSz));
                else
                    ImGui::TextColored({0.80f, 0.50f, 0.20f, 1.0f}, "%s", s.tooltipCopper);
            }
        }

        ImGui::EndTooltip();
    }

    // ---------- shared item table ----------

    void ItemSearchWindow::RenderItemTable(const char* id, const std::vector<const FoundItem*>& items,
                                           bool showLocation,
                                           const FoundItem*& ioHover, void*& ioHoverTex,
                                           float height, bool scroll)
    {
        const auto& s = Lang::Get();
        ImGuiTableFlags tableFlags =
            ImGuiTableFlags_BordersOuter   |
            ImGuiTableFlags_BordersInnerV  |
            ImGuiTableFlags_RowBg          |
            ImGuiTableFlags_SizingStretchProp;
        if (scroll) tableFlags |= ImGuiTableFlags_ScrollY;

        const int    cols      = showLocation ? 3 : 2;
        // scroll=false -> auto height (fits content); the parent child handles scrolling
        const ImVec2 tableSize = scroll ? ImVec2(0.0f, height > 0.0f ? height : ImGui::GetContentRegionAvail().y)
                                        : ImVec2(0.0f, 0.0f);

        constexpr float kCellPadY = 3.0f; // symmetric vertical padding for item rows
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, kCellPadY));
        if (!ImGui::BeginTable(id, cols, tableFlags, tableSize)) { ImGui::PopStyleVar(); return; }

        ImGui::TableSetupColumn(s.colItem, ImGuiTableColumnFlags_WidthStretch, showLocation ? 0.55f : 0.82f);
        if (showLocation)
            ImGui::TableSetupColumn(s.colLocation, ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn(s.colCount, ImGuiTableColumnFlags_WidthFixed, 52.0f);
        if (scroll)
            ImGui::TableSetupScrollFreeze(0, 1);

        // Header row with extra vertical padding (taller than item rows)
        {
            const float th       = ImGui::GetTextLineHeight();
            const float headerH  = th + 12.0f;
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers, headerH);
            for (int c = 0; c < cols; ++c)
            {
                ImGui::TableSetColumnIndex(c);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (headerH - th) * 0.5f - kCellPadY);
                ImGui::TableHeader(ImGui::TableGetColumnName(c));
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // The icon (kIconSize) drives the row height. Text is drawn via the draw list
        // (not ImGui::Text) so the host font's tall line box doesn't inflate the row;
        // it is vertically centered to the icon.
        const float fontH = ImGui::GetFontSize();

        auto drawText = [&](float x, float cellTopY, const ImVec4& col, const char* txt)
        {
            const ImVec2 pos(x, cellTopY + (kIconSize - fontH) * 0.5f);
            dl->AddText(pos, ImGui::ColorConvertFloat4ToU32(col), txt);
        };

        auto drawRow = [&](const FoundItem* item)
        {
            ImGui::TableNextRow();

            // Col 0: icon slot + name
            ImGui::TableSetColumnIndex(0);
            const ImVec2 c0 = ImGui::GetCursorScreenPos();
            void* tex = GetOrLoadTexture(item->iconUrl);
            DrawItemSlot(tex, item->count, dl);   // ImGui::Image(kIconSize) -> sets row height
            const std::string name = DisplayName(*item);
            drawText(c0.x + kIconSize + 7.0f, c0.y, RarityColor(*item), name.c_str());

            const float nameW = ImGui::CalcTextSize(name.c_str()).x;
            const bool hovered = ImGui::IsMouseHoveringRect(
                c0, ImVec2(c0.x + kIconSize + 7.0f + nameW, c0.y + kIconSize));

            int colIdx = 1;
            if (showLocation)
            {
                ImGui::TableSetColumnIndex(colIdx++);
                const ImVec2 cp = ImGui::GetCursorScreenPos();
                drawText(cp.x, cp.y, LocationColor(*item), GetLocationStr(*item).c_str());
            }

            ImGui::TableSetColumnIndex(colIdx);
            const ImVec2 cc = ImGui::GetCursorScreenPos();
            char cntBuf[16];
            snprintf(cntBuf, sizeof(cntBuf), "%d", item->count);
            drawText(cc.x, cc.y, ImGui::GetStyleColorVec4(ImGuiCol_Text), cntBuf);

            if (!ioHover && hovered) { ioHover = item; ioHoverTex = tex; }
        };

        if (scroll)
        {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(items.size()), kIconSize + kCellPadY * 2.0f);
            while (clipper.Step())
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                    drawRow(items[i]);
            clipper.End();
        }
        else
        {
            for (const FoundItem* item : items) drawRow(item); // small list -> render all
        }

        ImGui::EndTable();
        ImGui::PopStyleVar();
    }

    // ---------- main render ----------

    void ItemSearchWindow::Render(AppState& state, bool& outRequestRefresh)
    {
        outRequestRefresh = false;
        const auto& s = Lang::Get();

        if (!state.showWindow.load(std::memory_order_relaxed)) return;

        const std::string title = std::string(s.windowTitle) + Constants::WindowId;
        ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f, 200.0f), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(580, 500), ImGuiCond_FirstUseEver);
        bool isOpen = true;
        if (!ImGui::Begin(title.c_str(), &isOpen))
        {
            if (!isOpen) state.showWindow.store(false, std::memory_order_relaxed);
            ImGui::End();
            return;
        }
        if (!isOpen) state.showWindow.store(false, std::memory_order_relaxed);

        // ── Account name + Load button on one line ───────────────────────────
        const bool fetching = state.fetching.load(std::memory_order_relaxed);
        {
            std::string accName, fetchErr;
            {
                std::shared_lock lk(state.statusLock);
                accName  = state.accountName;
                fetchErr = state.fetchError;
            }

            const PluginConfig cfg = GetConfig(state);
            const bool hasKey = cfg.apiKey[0] != '\0';

            ImGui::AlignTextToFramePadding(); // vertically center the name with the button
            if (!accName.empty())
                ImGui::TextColored(kGold, "Account: %s", accName.c_str());
            else
                ImGui::TextDisabled("Account: -");

            // Refresh button directly after the account name
            const char* btnLabel = fetching ? s.refreshing : s.refreshBtn;
            ImGui::SameLine(0.0f, 12.0f);

            const float now      = static_cast<float>(ImGui::GetTime());
            const float since    = now - m_LastRefreshTime;
            const bool  hasError = !fetchErr.empty();
            const bool  cooldown = (since < 300.0f) && !hasError; // retry immediately after an error
            const bool  canRefresh = !fetching && hasKey && !cooldown;

            if (canRefresh)
            {
                if (ImGui::Button(btnLabel))
                {
                    outRequestRefresh = true;
                    m_LastRefreshTime = now;
                }
            }
            else
            {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                ImGui::Button(btnLabel);
                ImGui::PopStyleVar();
                if (cooldown && ImGui::IsItemHovered())
                {
                    const int remain = static_cast<int>(300.0f - since);
                    ImGui::SetTooltip("%s %dm %02ds", s.refreshCooldown, remain / 60, remain % 60);
                }
            }

            if (hasError)
                ImGui::TextColored(kRed, "%s", fetchErr.c_str());

            // ── API key guard ────────────────────────────────────────────────
            if (!hasKey)
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f), "%s", s.noApiKey);
                ImGui::End();
                return;
            }

            // ── Search bar (full width) ──────────────────────────────────────
            ImGui::Spacing();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputTextWithHint("##search", s.searchHint, m_SearchBuf.data(), m_SearchBuf.size()))
                m_FilterLower = Utility::ToLower(m_SearchBuf.data());
            if (m_SearchBuf[0] == '\0') m_FilterLower.clear();
        }
        ImGui::Spacing();

        // ── Loading spinner ───────────────────────────────────────────────────
        if (fetching)
        {
            constexpr float kSpinRadius = 12.0f;
            const float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - kSpinRadius * 2.0f) * 0.5f);
            RenderLoadingSpinner(kSpinRadius, 3.5f);
            ImGui::Spacing();
        }

        // ── Snapshot & aggregate (only when items change) ────────────────────
        const uint64_t currentVersion = state.itemsVersion.load(std::memory_order_acquire);
        if (currentVersion != m_AggregatedVersion)
        {
            std::vector<FoundItem> raw;
            {
                std::shared_lock lk(state.itemsLock);
                raw = state.items;
            }
            using AggKey = std::tuple<int, uint8_t, std::string>;
            std::map<AggKey, FoundItem> aggMap;
            for (const auto& item : raw)
            {
                AggKey key{item.itemId, static_cast<uint8_t>(item.locationType), item.characterName};
                auto [it, inserted] = aggMap.emplace(key, item);
                // Equipment is presence-based (one row per item+character); it lives in
                // multiple template tabs and legendary-armory gear is shared across them,
                // so summing would over-count. Everything else sums stack sizes.
                if (!inserted && item.locationType != ItemLocation::Equipment)
                    it->second.count += item.count;
            }
            m_AggregatedSnapshot.clear();
            m_AggregatedSnapshot.reserve(aggMap.size());
            for (auto& [k, v] : aggMap) m_AggregatedSnapshot.push_back(std::move(v));

            // Bank items kept per-slot (not aggregated) for bank-tab paging
            m_BankRaw.clear();
            for (const auto& item : raw)
                if (item.locationType == ItemLocation::Bank && item.bankSlot >= 0)
                    m_BankRaw.push_back(item);
            std::sort(m_BankRaw.begin(), m_BankRaw.end(),
                      [](const FoundItem& a, const FoundItem& b) { return a.bankSlot < b.bankSlot; });

            // Equipment kept per template tab (not aggregated) for the character tab's
            // equipment-template switcher.
            m_EquipRaw.clear();
            for (const auto& item : raw)
                if (item.locationType == ItemLocation::Equipment)
                    m_EquipRaw.push_back(item);

            m_AggregatedVersion = currentVersion;
        }

        // ── Nothing fetched yet ──────────────────────────────────────────────
        if (m_AggregatedSnapshot.empty())
        {
            ImGui::TextDisabled("%s", s.noResults);
            ImGui::End();
            return;
        }

        // ── Search filter for the general tab (cached) ───────────────────────
        {
            const bool filterChanged = (m_FilterLower != m_CachedFilter);
            if (m_FilteredVersion != m_AggregatedVersion || filterChanged)
            {
                m_FilteredItems.clear();
                m_FilteredItems.reserve(m_AggregatedSnapshot.size());
                for (const auto& item : m_AggregatedSnapshot)
                    if (MatchesFilter(item, m_FilterLower.c_str()))
                        m_FilteredItems.push_back(&item);
                m_CachedFilter    = m_FilterLower;
                m_FilteredVersion = m_AggregatedVersion;
            }
        }

        // ── Distinct character names + professions for tabs (cached) ─────────
        if (m_CharTabsVersion != m_AggregatedVersion)
        {
            std::set<std::string> names;
            m_CharProf.clear();
            m_CharEliteName.clear();
            m_CharEliteId.clear();
            for (const auto& item : m_AggregatedSnapshot)
                if ((item.locationType == ItemLocation::Character ||
                     item.locationType == ItemLocation::Equipment) &&
                    !item.characterName.empty())
                {
                    names.insert(item.characterName);
                    if (!item.characterProfession.empty())
                        m_CharProf[item.characterName] = item.characterProfession;
                    if (!item.characterEliteSpec.empty())
                        m_CharEliteName[item.characterName] = item.characterEliteSpec;
                    if (item.characterEliteSpecId != 0)
                        m_CharEliteId[item.characterName] = item.characterEliteSpecId;
                }
            m_CharNames.assign(names.begin(), names.end());
            // Group tabs by base profession (fixed class order), then by elite
            // spec id (= release order, core first), then name, so characters of
            // the same class sit next to each other instead of name-alphabetical.
            auto eliteId = [&](const std::string& n)
            {
                auto it = m_CharEliteId.find(n);
                return it != m_CharEliteId.end() ? it->second : 0;
            };
            std::sort(m_CharNames.begin(), m_CharNames.end(),
                      [&](const std::string& a, const std::string& b)
            {
                const int oa = ProfessionOrder(m_CharProf[a]);
                const int ob = ProfessionOrder(m_CharProf[b]);
                if (oa != ob) return oa < ob;
                const int ea = eliteId(a);
                const int eb = eliteId(b);
                if (ea != eb) return ea < eb;
                return a < b;
            });
            m_CharTabsVersion = m_AggregatedVersion;
        }

        // ── Custom tab strip with class/elite icons (Search + one per character) ─
        const FoundItem* tooltipItem = nullptr;
        void*            tooltipTex  = nullptr;

        // Tabs: 0 = Search, 1 = Bank, 2.. = characters
        if (m_ActiveTab < 0 || m_ActiveTab > static_cast<int>(m_CharNames.size()) + 1)
            m_ActiveTab = 0;

        // Draw one rounded, clickable tab (icon + coloured label); wraps to a new
        // line instead of showing a horizontal scrollbar.
        const float kTabRound  = 5.0f;
        const float kTabSpacing = 4.0f;
        const float lineLimit  = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        bool firstTab = true;
        auto drawTab = [&](int index, void* icon, const char* label, ImVec4 col)
        {
            const float iconSz = ImGui::GetTextLineHeight() + 2.0f;
            const float pad    = 8.0f;
            const float textW  = ImGui::CalcTextSize(label).x;
            const float w      = pad + (icon ? iconSz + 4.0f : 0.0f) + textW + pad;
            const float h      = iconSz + 8.0f;

            // Wrap to a new line when the next tab would overflow the window width
            if (!firstTab)
            {
                const float nextRight = ImGui::GetItemRectMax().x + kTabSpacing + w;
                if (nextRight <= lineLimit) ImGui::SameLine(0.0f, kTabSpacing);
            }
            firstTab = false;

            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::PushID(index);
            ImGui::InvisibleButton("##tab", ImVec2(w, h));
            const bool clicked = ImGui::IsItemClicked();
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();

            ImDrawList* dl = ImGui::GetWindowDrawList();
            const bool active = (m_ActiveTab == index);
            if (active || hovered)
            {
                const ImU32 bg = ImGui::GetColorU32(active ? ImGuiCol_TabActive : ImGuiCol_TabHovered);
                dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, kTabRound);
            }
            if (active)
                dl->AddRect(p, ImVec2(p.x + w, p.y + h),
                            ImGui::ColorConvertFloat4ToU32(col), kTabRound, ImDrawCornerFlags_All, 1.5f);

            float x = p.x + pad;
            const float cy = p.y + h * 0.5f;
            if (icon)
            {
                dl->AddImage(reinterpret_cast<ImTextureID>(icon),
                             ImVec2(x, cy - iconSz * 0.5f), ImVec2(x + iconSz, cy + iconSz * 0.5f));
                x += iconSz + 4.0f;
            }
            dl->AddText(ImVec2(x, cy - ImGui::GetTextLineHeight() * 0.5f),
                        ImGui::ColorConvertFloat4ToU32(col), label);

            if (clicked) m_ActiveTab = index;
        };

        drawTab(0, nullptr, s.tabSearch, ImGui::GetStyleColorVec4(ImGuiCol_Text));
        drawTab(1, nullptr, s.tabBank,   ImGui::GetStyleColorVec4(ImGuiCol_Text));
        for (size_t i = 0; i < m_CharNames.size(); ++i)
        {
            const std::string& cn = m_CharNames[i];
            std::string prof, elite;
            if (auto it = m_CharProf.find(cn);      it != m_CharProf.end())      prof  = it->second;
            if (auto it = m_CharEliteName.find(cn); it != m_CharEliteName.end()) elite = it->second;
            void* icon = GetClassIcon(elite);                 // elite first
            if (!icon) icon = GetClassIcon(prof);             // fall back to core
            drawTab(static_cast<int>(i) + 2, icon, cn.c_str(), ProfessionColor(prof));
        }
        ImGui::Spacing();
        ImGui::Separator();

        // ── Tab content ──────────────────────────────────────────────────────
        if (m_ActiveTab == 0)
        {
            if (m_FilteredItems.empty())
                ImGui::TextDisabled("%s", s.noResults);
            else
                RenderItemTable("##all", m_FilteredItems, true, tooltipItem, tooltipTex);
        }
        else if (m_ActiveTab == 1)
        {
            // Bank tab: materials (left) | bank grouped into bank tabs (right)
            std::vector<const FoundItem*> mats;
            for (const auto& item : m_AggregatedSnapshot)
                if (item.locationType == ItemLocation::Materials && MatchesFilter(item, m_FilterLower.c_str()))
                    mats.push_back(&item);

            const float availY = ImGui::GetContentRegionAvail().y;
            const float leftW  = ImGui::GetContentRegionAvail().x * 0.42f;

            // Left: material storage
            ImGui::BeginChild("##matcol", ImVec2(leftW, availY), false);
            ImGui::TextColored(kGold, "%s", s.locMaterials);
            if (mats.empty()) ImGui::TextDisabled("-");
            else RenderItemTable("##mats", mats, false, tooltipItem, tooltipTex, 0.0f, false);
            ImGui::EndChild();

            ImGui::SameLine();

            // Right: bank, grouped into bank tabs (30 slots each)
            ImGui::BeginChild("##bankcol", ImVec2(0.0f, availY), false);
            constexpr int kBankTabSize = 30;
            int maxTab = -1;
            for (const auto& it : m_BankRaw)
                if (it.bankSlot / kBankTabSize > maxTab) maxTab = it.bankSlot / kBankTabSize;
            bool anyBank = false;
            for (int t = 0; t <= maxTab; ++t)
            {
                std::vector<const FoundItem*> tabItems;
                for (const auto& it : m_BankRaw)
                    if (it.bankSlot / kBankTabSize == t && MatchesFilter(it, m_FilterLower.c_str()))
                        tabItems.push_back(&it);
                if (tabItems.empty()) continue;
                anyBank = true;
                ImGui::TextColored(kLocChar, "%s %d", s.bankTab, t + 1);
                const std::string tid = "##bank" + std::to_string(t);
                RenderItemTable(tid.c_str(), tabItems, false, tooltipItem, tooltipTex, 0.0f, false);
                ImGui::Spacing();
            }
            if (!anyBank) ImGui::TextDisabled("-");
            ImGui::EndChild();
        }
        else
        {
            const std::string& charName = m_CharNames[m_ActiveTab - 2];
            const bool searching = !m_FilterLower.empty();

            // Profession colour for this character (used to tint template buttons)
            std::string charProf;
            if (auto it = m_CharProf.find(charName); it != m_CharProf.end()) charProf = it->second;
            const ImVec4 profCol = ProfessionColor(charProf);
            const ImVec4 profDim = { profCol.x * 0.55f, profCol.y * 0.55f, profCol.z * 0.55f, 1.0f };

            // Account-wide shared inventory + this character's bag inventory (aggregated)
            std::vector<const FoundItem*> inv, shared;
            for (const auto& item : m_AggregatedSnapshot)
            {
                if (!MatchesFilter(item, m_FilterLower.c_str())) continue;
                // Shared inventory is account-wide -> shown on every character tab
                if (item.locationType == ItemLocation::SharedInventory) { shared.push_back(&item); continue; }
                if (item.characterName != charName) continue;
                if (item.locationType == ItemLocation::Character) inv.push_back(&item);
            }

            // Equipment templates (tabs) for this character, from the raw per-tab data
            struct TabInfo { int idx; std::string name; bool active; };
            std::vector<TabInfo> tabs;
            std::set<int> tabsWithMatch;
            for (const auto& it : m_EquipRaw)
            {
                if (it.characterName != charName) continue;
                bool found = false;
                for (auto& t : tabs)
                    if (t.idx == it.equipTabIdx) { if (it.equipTabActive) t.active = true; found = true; break; }
                if (!found) tabs.push_back({ it.equipTabIdx, it.equipTabName, it.equipTabActive });
                if (searching && MatchesFilter(it, m_FilterLower.c_str()))
                    tabsWithMatch.insert(it.equipTabIdx);
            }
            std::sort(tabs.begin(), tabs.end(),
                      [](const TabInfo& a, const TabInfo& b) { return a.idx < b.idx; });

            // Resolve the selected template (default = active set, else the first tab)
            int sel = -1;
            if (auto si = m_SelEquipTab.find(charName); si != m_SelEquipTab.end()) sel = si->second;
            bool selValid = false;
            for (const auto& t : tabs) if (t.idx == sel) { selValid = true; break; }
            if (!selValid)
            {
                sel = tabs.empty() ? -1 : tabs.front().idx;
                for (const auto& t : tabs) if (t.active) { sel = t.idx; break; }
                m_SelEquipTab[charName] = sel;
            }

            const float availY = ImGui::GetContentRegionAvail().y;
            const float leftW  = ImGui::GetContentRegionAvail().x * 0.42f;

            // ── Left: template switcher + equipment sections (the child scrolls) ──
            ImGui::BeginChild("##equipcol", ImVec2(leftW, availY), false);

            // Template tabs — styled like the character tabs (rounded, class-coloured
            // label + active border), just without an icon. Active set preselected;
            // when searching, tabs containing a match get a dim class-colour fill.
            const float kEqTabRound   = 5.0f;
            const float kEqTabSpacing = 4.0f;
            const float eqColRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            bool firstEqTab = true;
            auto drawEquipTab = [&](int tabIdx, const char* label, bool selected, bool matched) -> bool
            {
                const float pad   = 8.0f;
                const float th    = ImGui::GetTextLineHeight();
                const float textW = ImGui::CalcTextSize(label).x;
                const float w     = pad + textW + pad;
                const float h     = th + 8.0f;

                if (!firstEqTab)
                {
                    const float nextRight = ImGui::GetItemRectMax().x + kEqTabSpacing + w;
                    if (nextRight <= eqColRight) ImGui::SameLine(0.0f, kEqTabSpacing);
                }
                firstEqTab = false;

                const ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::PushID(tabIdx);
                ImGui::InvisibleButton("##etab", ImVec2(w, h));
                const bool clicked = ImGui::IsItemClicked();
                const bool hovered = ImGui::IsItemHovered();
                ImGui::PopID();

                ImDrawList* dl = ImGui::GetWindowDrawList();
                if (selected || hovered || matched)
                {
                    const ImU32 bg = selected ? ImGui::GetColorU32(ImGuiCol_TabActive)
                                   : hovered  ? ImGui::GetColorU32(ImGuiCol_TabHovered)
                                              : ImGui::ColorConvertFloat4ToU32(profDim);
                    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, kEqTabRound);
                }
                if (selected)
                    dl->AddRect(p, ImVec2(p.x + w, p.y + h),
                                ImGui::ColorConvertFloat4ToU32(profCol), kEqTabRound, ImDrawCornerFlags_All, 1.5f);
                dl->AddText(ImVec2(p.x + pad, p.y + (h - th) * 0.5f),
                            ImGui::ColorConvertFloat4ToU32(profCol), label);
                return clicked;
            };
            for (size_t i = 0; i < tabs.size(); ++i)
            {
                const std::string label = tabs[i].name.empty()
                    ? std::string(s.bankTab) + " " + std::to_string(tabs[i].idx)
                    : tabs[i].name;
                if (drawEquipTab(tabs[i].idx, label.c_str(), tabs[i].idx == sel,
                                 tabsWithMatch.count(tabs[i].idx) > 0))
                {
                    sel = tabs[i].idx;
                    m_SelEquipTab[charName] = sel;
                }
            }
            if (!tabs.empty()) ImGui::Spacing();

            // Matches the filter if the gear's own name matches, or if one of its slotted
            // runes/sigils/infusions/enrichments matches — so searching for an equipped
            // upgrade surfaces the item that contains it (not the component itself).
            auto gearMatches = [&](const FoundItem& it) -> bool
            {
                if (MatchesFilter(it, m_FilterLower.c_str())) return true;
                if (!searching) return false;
                for (const auto& eu : it.upgradeSlots)
                    if (Utility::ToLower(eu.name).find(m_FilterLower) != std::string::npos) return true;
                return false;
            };

            // Gear of the selected template (slotted upgrades stay nested in the tooltip)
            std::vector<const FoundItem*> equip;
            for (const auto& it : m_EquipRaw)
            {
                if (it.characterName != charName || it.equipTabIdx != sel) continue;
                if (it.type == "UpgradeComponent") continue; // shown via its parent gear
                if (!gearMatches(it)) continue;
                equip.push_back(&it);
            }

            {
                std::vector<const FoundItem*> buckets[6];
                for (const FoundItem* it : equip) { int c, o; EquipCategory(*it, c, o); buckets[c].push_back(it); }
                for (auto& b : buckets)
                    std::sort(b.begin(), b.end(), [](const FoundItem* a, const FoundItem* z)
                    {
                        int ca, oa, cz, oz; EquipCategory(*a, ca, oa); EquipCategory(*z, cz, oz);
                        return oa < oz;
                    });
                const char* secNames[6] = { s.secArmor, s.secWeapons, s.secTrinkets,
                                            s.secAquatic, s.secGathering, s.secJadebot };
                for (int c = 0; c < 6; ++c)
                {
                    if (buckets[c].empty()) continue;
                    ImGui::TextColored(kLocChar, "%s", secNames[c]);
                    const std::string tid = "##eq" + std::to_string(c);
                    RenderItemTable(tid.c_str(), buckets[c], false, tooltipItem, tooltipTex, 0.0f, false);
                    ImGui::Spacing();
                }
            }

            if (tabs.empty() && equip.empty() && !searching)
                ImGui::TextDisabled("-");
            ImGui::EndChild();

            ImGui::SameLine();

            // ── Right: shared inventory (top) + character inventory ──
            ImGui::BeginChild("##invcol", ImVec2(0.0f, availY), false);
            if (!shared.empty())
            {
                ImGui::TextColored(kGold, "%s", s.locSharedInventory);
                RenderItemTable("##shared", shared, false, tooltipItem, tooltipTex, 0.0f, false);
                ImGui::Spacing();
            }
            ImGui::TextColored(kGold, "%s", s.locInventory);
            if (inv.empty()) ImGui::TextDisabled("-");
            else RenderItemTable("##inv", inv, false, tooltipItem, tooltipTex, 0.0f, false);
            ImGui::EndChild();
        }

        // Tooltip for the hovered row of whichever tab is active
        if (tooltipItem)
        {
            const float now = static_cast<float>(ImGui::GetTime());
            if (m_HoveredItemId != tooltipItem->itemId)
            {
                m_HoveredItemId  = tooltipItem->itemId;
                m_HoverStartTime = now;
            }
            else if (now - m_HoverStartTime >= 0.0f)
            {
                this->ShowItemTooltip(*tooltipItem, tooltipTex);
            }
        }
        else
        {
            m_HoveredItemId  = 0;
            m_HoverStartTime = 0.0f;
        }

        ImGui::End();
    }

    // ---------- options panel ----------

    void ItemSearchWindow::RenderOptions(ConfigStore& config, AppState& state, bool& outRequestRefresh)
    {
        outRequestRefresh = false;
        const auto& s = Lang::Get();

        ImGui::TextColored(kGold, "%s", s.optGeneralSection);
        ImGui::Separator();
        ImGui::Spacing();

        // Show window — applies immediately
        bool showWin = state.showWindow.load(std::memory_order_relaxed);
        if (ImGui::Checkbox(s.optShowWindow, &showWin))
        {
            state.showWindow.store(showWin, std::memory_order_relaxed);
            config.ShowWindow() = showWin;
        }
        ImGui::Spacing();

        // API key
        ImGui::SetNextItemWidth(400.0f);
        ImGui::InputText(s.optApiKey, config.ApiKeyBuffer(), 256, ImGuiInputTextFlags_Password);
        ImGui::TextDisabled("%s", s.optApiKeyHint);
        ImGui::TextDisabled("%s", s.optApiKeyPerms);
        ImGui::Spacing();

        // Language dropdown
        const char* langItems[] = { "Deutsch", "English" };
        int langIdx = config.Language();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::Combo(s.optLanguage, &langIdx, langItems, 2))
            config.Language() = langIdx;
        ImGui::Spacing();

        ImGui::TextDisabled("%s", s.optKeybindHint);
        ImGui::Spacing();

        if (ImGui::Button(s.optSave, ImVec2(140.0f, 0.0f)))
        {
            config.ApplyFromEditBuffer();
            outRequestRefresh = true; // validate key + reload right after saving
        }
    }
}

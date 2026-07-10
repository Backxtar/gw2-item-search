#include "ItemSearchWindow.h"
#include "Constants.h"
#include "FontConfig.h"

void OnFontReceived(const char* identifier, void* font); // entry.cpp -> app
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

    // Per-frame UI metrics, set at the top of Render() and shared with the
    // file-static draw helpers (single window, render thread only). s_Ui is
    // the knob all fixed-pixel layout metrics scale with: configured item/body
    // px / the 16 px default. The per-role fonts may be nullptr until Nexus
    // delivered them — draw with an explicit px size (AddText(font, px, ...)),
    // so a fallback font still renders at the right size, just scaled.
    static float   s_Ui           = 1.0f;
    static float   s_IconSize     = 36.0f;   // table row icon edge, floored to whole px
    static float   s_SlotSize     = 40.0f;   // inventory-grid / equipment slot edge (one size everywhere)
    static ImFont* s_FontHeading  = nullptr; // section headers
    static float   s_HeadingPx    = 20.0f;
    static ImFont* s_FontButton   = nullptr; // button labels
    static float   s_ButtonPx     = 16.0f;
    static ImFont* s_FontTooltip  = nullptr; // item-hover tooltip body
    static float   s_TooltipPx    = 16.0f;
    static ImFont* s_FontTipTitle = nullptr; // item name in the tooltip header
    static ImFont* s_FontTitle    = nullptr; // window title (big Menomonia)
    static float   s_TitlePx      = 28.0f;

    // ---------- Blish HUD / GW2 look ----------
    // Scoped theme push around our window only: Nexus shares a single ImGui
    // context across all addons, so the global style must stay untouched.
    // Palette follows Blish HUD (near-black translucent panels, angular corners,
    // white Menomonia text, khaki/gold accents) so the window blends with the
    // native GW2 UI.
    class ThemeScope
    {
    public:
        explicit ThemeScope(ImFont* font, float ui = 1.0f)
        {
            if (font) { ImGui::PushFont(font); m_Font = true; }

            auto col = [this](ImGuiCol idx, float r, float g, float b, float a)
                { ImGui::PushStyleColor(idx, ImVec4(r, g, b, a)); ++m_Colors; };

            col(ImGuiCol_Text,             1.00f, 1.00f, 1.00f, 1.00f); // Blish StandardColors.Default
            col(ImGuiCol_TextDisabled,     0.67f, 0.67f, 0.67f, 1.00f); // Blish DisabledText (#AAAAAA)
            // Fully opaque base — the window body is solid; only overlays like
            // the item-hover glow stay translucent.
            col(ImGuiCol_WindowBg,         0.020f, 0.018f, 0.015f, 1.00f);
            col(ImGuiCol_ChildBg,          0.00f, 0.00f, 0.00f, 0.00f);
            // Tooltips/popups keep only a hint of translucency — combined with
            // the tooltip texture this lands at near-full opacity.
            col(ImGuiCol_PopupBg,          0.030f, 0.027f, 0.022f, 0.75f);
            col(ImGuiCol_Border,           0.55f, 0.52f, 0.45f, 0.30f);
            col(ImGuiCol_BorderShadow,     0.00f, 0.00f, 0.00f, 0.45f);
            col(ImGuiCol_FrameBg,          0.00f, 0.00f, 0.00f, 0.55f);
            col(ImGuiCol_FrameBgHovered,   0.18f, 0.17f, 0.13f, 0.90f);
            col(ImGuiCol_FrameBgActive,    0.24f, 0.22f, 0.17f, 0.95f);
            col(ImGuiCol_TitleBg,          0.03f, 0.03f, 0.025f, 0.92f);
            col(ImGuiCol_TitleBgActive,    0.06f, 0.055f, 0.045f, 1.00f);
            col(ImGuiCol_TitleBgCollapsed, 0.03f, 0.03f, 0.025f, 0.75f);
            col(ImGuiCol_ScrollbarBg,      0.00f, 0.00f, 0.00f, 0.35f);
            col(ImGuiCol_ScrollbarGrab,    0.33f, 0.31f, 0.26f, 0.80f);
            col(ImGuiCol_ScrollbarGrabHovered, 0.44f, 0.42f, 0.35f, 0.90f);
            col(ImGuiCol_ScrollbarGrabActive,  0.55f, 0.52f, 0.42f, 1.00f);
            col(ImGuiCol_CheckMark,        0.90f, 0.78f, 0.42f, 1.00f); // GW2 gold accents
            col(ImGuiCol_SliderGrab,       0.72f, 0.63f, 0.36f, 1.00f);
            col(ImGuiCol_SliderGrabActive, 0.90f, 0.78f, 0.42f, 1.00f);
            col(ImGuiCol_Button,           0.15f, 0.14f, 0.11f, 0.85f);
            col(ImGuiCol_ButtonHovered,    0.35f, 0.32f, 0.24f, 1.00f);
            col(ImGuiCol_ButtonActive,     0.45f, 0.40f, 0.29f, 1.00f);
            col(ImGuiCol_Header,           0.24f, 0.22f, 0.16f, 0.55f);
            col(ImGuiCol_HeaderHovered,    0.35f, 0.32f, 0.24f, 0.90f);
            col(ImGuiCol_HeaderActive,     0.45f, 0.40f, 0.29f, 1.00f);
            col(ImGuiCol_Separator,        0.40f, 0.38f, 0.31f, 0.45f);
            col(ImGuiCol_SeparatorHovered, 0.55f, 0.52f, 0.42f, 0.70f);
            col(ImGuiCol_SeparatorActive,  0.66f, 0.62f, 0.47f, 1.00f);
            col(ImGuiCol_ResizeGrip,       0.40f, 0.38f, 0.31f, 0.35f);
            col(ImGuiCol_ResizeGripHovered,0.55f, 0.52f, 0.42f, 0.65f);
            col(ImGuiCol_ResizeGripActive, 0.66f, 0.62f, 0.47f, 0.90f);
            col(ImGuiCol_Tab,              0.06f, 0.055f, 0.045f, 0.85f);
            col(ImGuiCol_TabHovered,       0.35f, 0.32f, 0.24f, 0.95f);
            col(ImGuiCol_TabActive,        0.33f, 0.30f, 0.22f, 1.00f);
            col(ImGuiCol_TabUnfocused,     0.06f, 0.055f, 0.045f, 0.85f);
            col(ImGuiCol_TabUnfocusedActive, 0.24f, 0.22f, 0.16f, 1.00f);
            col(ImGuiCol_TableHeaderBg,    0.00f, 0.00f, 0.00f, 0.80f);
            col(ImGuiCol_TableBorderStrong,0.30f, 0.28f, 0.23f, 0.60f);
            col(ImGuiCol_TableBorderLight, 0.22f, 0.21f, 0.17f, 0.40f);
            col(ImGuiCol_TableRowBgAlt,    1.00f, 1.00f, 1.00f, 0.02f);
            col(ImGuiCol_TextSelectedBg,   0.90f, 0.78f, 0.42f, 0.35f);

            auto var = [this](ImGuiStyleVar idx, float v)
                { ImGui::PushStyleVar(idx, v); ++m_Vars; };
            auto var2 = [this](ImGuiStyleVar idx, ImVec2 v)
                { ImGui::PushStyleVar(idx, v); ++m_Vars; };

            var(ImGuiStyleVar_WindowRounding,    0.0f); // GW2 UI is angular
            var(ImGuiStyleVar_ChildRounding,     0.0f);
            var(ImGuiStyleVar_FrameRounding,     0.0f);
            var(ImGuiStyleVar_PopupRounding,     0.0f);
            var(ImGuiStyleVar_ScrollbarRounding, 0.0f);
            var(ImGuiStyleVar_GrabRounding,      0.0f);
            var(ImGuiStyleVar_TabRounding,       0.0f);
            var(ImGuiStyleVar_WindowBorderSize,  1.0f);
            var(ImGuiStyleVar_FrameBorderSize,   1.0f);

            // The whole layout follows the UI size, not only the text: the
            // ImGui default metrics scaled by the effective body-size factor.
            var2(ImGuiStyleVar_WindowPadding,    ImVec2(8.0f * ui, 8.0f * ui));
            var2(ImGuiStyleVar_FramePadding,     ImVec2(4.0f * ui, 3.0f * ui));
            var2(ImGuiStyleVar_ItemSpacing,      ImVec2(8.0f * ui, 4.0f * ui));
            var2(ImGuiStyleVar_ItemInnerSpacing, ImVec2(4.0f * ui, 4.0f * ui));
            var2(ImGuiStyleVar_CellPadding,      ImVec2(4.0f * ui, 2.0f * ui));
            var(ImGuiStyleVar_IndentSpacing,     21.0f * ui);
            var(ImGuiStyleVar_ScrollbarSize,     14.0f * ui);
            var(ImGuiStyleVar_GrabMinSize,       10.0f * ui);
        }

        ~ThemeScope()
        {
            ImGui::PopStyleVar(m_Vars);
            ImGui::PopStyleColor(m_Colors);
            if (m_Font) ImGui::PopFont();
        }

        ThemeScope(const ThemeScope&)            = delete;
        ThemeScope& operator=(const ThemeScope&) = delete;

    private:
        int  m_Colors = 0;
        int  m_Vars   = 0;
        bool m_Font   = false;
    };

    // Full-width dark section bar in the style of Blish HUD panel headers
    // ("Beschreibung", "API-Berechtigungen", ...).
    // Full-width collapsible section bar, styled exactly like Gw2Button (khaki
    // gradient, top sheen, dark border, gold label, animated hover fade) plus
    // a collapse arrow. Clicking toggles the section; returns true while open.
    // Open sections sit flush against the content below (no item-spacing gap).
    static bool SectionHeader(const char* label, const char* suffix = nullptr)
    {
        ImFont*     f  = s_FontHeading ? s_FontHeading : ImGui::GetFont();
        const float fs = s_HeadingPx;
        const float h  = fs + 8.0f * s_Ui;
        float       w  = ImGui::GetContentRegionAvail().x;
        if (w < 1.0f) w = 1.0f;
        const ImVec2 p = ImGui::GetCursorScreenPos();

        ImGui::PushID(label);
        ImGui::InvisibleButton("##sec", ImVec2(w, h));
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        const bool held    = ImGui::IsItemActive();

        // Collapse state + hover fade live in the window's state storage
        ImGuiStorage* store  = ImGui::GetStateStorage();
        const ImGuiID openId = ImGui::GetID("##secopen");
        const ImGuiID fadeId = ImGui::GetID("##secfade");
        bool open = store->GetInt(openId, 1) != 0;
        if (clicked) { open = !open; store->SetInt(openId, open ? 1 : 0); }
        float t = store->GetFloat(fadeId, 0.0f);
        t += (hovered ? 1.0f : -1.0f) * ImGui::GetIO().DeltaTime / 0.2f;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        store->SetFloat(fadeId, t);
        ImGui::PopID();

        // Same gradient formula as Gw2Button
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 pMax = { p.x + w, p.y + h };
        const ImVec4 base = { 0.85f, 0.72f, 0.38f, 1.0f };
        float kTop = 0.30f + 0.20f * t;
        float kBot = 0.18f + 0.15f * t;
        if (held) { kTop *= 0.80f; kBot *= 0.80f; }
        const ImU32 cTop = ImGui::ColorConvertFloat4ToU32(ImVec4(base.x * kTop, base.y * kTop, base.z * kTop, 0.96f));
        const ImU32 cBot = ImGui::ColorConvertFloat4ToU32(ImVec4(base.x * kBot, base.y * kBot, base.z * kBot, 0.96f));
        const ImU32 gold = ImGui::ColorConvertFloat4ToU32(kGold);
        dl->AddRectFilledMultiColor(p, pMax, cTop, cTop, cBot, cBot);
        dl->AddRectFilled(p, ImVec2(pMax.x, p.y + 1.0f), IM_COL32(255, 255, 255, 28)); // top sheen
        dl->AddRect(p, pMax, IM_COL32(0, 0, 0, 210));
        if (t > 0.01f)
            dl->AddRect(p, pMax, ImGui::ColorConvertFloat4ToU32(ImVec4(kGold.x, kGold.y, kGold.z, t)),
                        0.0f, ImDrawCornerFlags_All, 1.5f);

        // Collapse arrow (down = open, right = closed) + label
        const float ax = p.x + 10.0f * s_Ui;
        const float cy = p.y + h * 0.5f;
        const float as = fs * 0.30f;
        if (open)
            dl->AddTriangleFilled(ImVec2(ax, cy - as * 0.6f), ImVec2(ax + as * 2.0f, cy - as * 0.6f),
                                  ImVec2(ax + as, cy + as * 0.8f), gold);
        else
            dl->AddTriangleFilled(ImVec2(ax, cy - as), ImVec2(ax + as * 1.4f, cy),
                                  ImVec2(ax, cy + as), gold);
        dl->AddText(f, fs, ImVec2(ax + as * 2.0f + 6.0f * s_Ui, p.y + (h - fs) * 0.5f), gold, label);

        // Optional right-aligned suffix (e.g. used/total slot count)
        if (suffix && suffix[0])
        {
            const float sfs = fs * 0.75f;
            const float sw  = f->CalcTextSizeA(sfs, FLT_MAX, 0.0f, suffix).x;
            dl->AddText(f, sfs, ImVec2(pMax.x - sw - 8.0f * s_Ui, p.y + (h - sfs) * 0.5f),
                        IM_COL32(214, 190, 128, 210), suffix);
        }

        // Open: the content below sits flush against the bar
        if (open)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
        return open;
    }

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

    ImFont* ItemSearchWindow::EnsureFont(AppState& state, float px)
    {
        if (px < 8.0f || !m_Api || !m_Api->Fonts_AddFromMemory || !state.fontData)
            return nullptr;
        char id[64];
        std::snprintf(id, sizeof(id), "%s_%d", Constants::FontIdPrefix,
                      static_cast<int>(px * 10.0f));
        auto it = state.fontsById.find(id);
        if (it != state.fontsById.end())
            return static_cast<ImFont*>(it->second); // nullptr while pending
        state.fontsById.emplace(id, nullptr);
        state.addedFontIds.push_back(id);
        m_Api->Fonts_AddFromMemory(id, px, state.fontData, state.fontDataSize,
                                   ::OnFontReceived, FontLoadConfig());
        return nullptr;
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

    // Official GW2 rarity colours (wiki hex values). Legendary is brightened a
    // touch from the chat-link purple (#4C139D) to stay readable on dark bg,
    // matching the in-game item name colour.
    static ImVec4 RarityColor(const std::string& r)
    {
        if (r == "Legendary")  return { 0.58f, 0.27f, 0.93f, 1.0f }; // ~#9445EE
        if (r == "Ascended")   return { 0.984f, 0.243f, 0.553f, 1.0f }; // #FB3E8D
        if (r == "Exotic")     return { 1.00f, 0.643f, 0.020f, 1.0f }; // #FFA405
        if (r == "Rare")       return { 0.988f, 0.816f, 0.043f, 1.0f }; // #FCD00B
        if (r == "Masterwork") return { 0.102f, 0.576f, 0.024f, 1.0f }; // #1A9306
        if (r == "Fine")       return { 0.384f, 0.643f, 0.855f, 1.0f }; // #62A4DA
        if (r == "Basic")      return { 1.00f, 1.00f, 1.00f, 1.0f };    // #FFFFFF
        return { 0.667f, 0.667f, 0.667f, 1.0f }; // Junk #AAAAAA / unknown
    }

    static ImVec4 RarityColor(const FoundItem& item) { return RarityColor(item.rarity); }

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

    // Empty slot, matching the in-game inventory: a light translucent box that
    // brightens the background a touch, with a slightly stronger light frame.
    // The box is snapped to whole pixels so the 1 px frame never drops an edge.
    static void DrawEmptySlot(ImDrawList* dl, float size)
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        p.x = std::floor(p.x); p.y = std::floor(p.y);
        ImGui::SetCursorScreenPos(p);
        const ImVec2 br = { p.x + size, p.y + size };
        dl->AddRectFilled(p, br, IM_COL32(255, 255, 255, 26));
        dl->AddRect(ImVec2(p.x + 0.5f, p.y + 0.5f), ImVec2(br.x - 0.5f, br.y - 0.5f),
                    IM_COL32(255, 255, 255, 60), 0.0f, 0, 1.0f);
        ImGui::Dummy(ImVec2(size, size));
    }

    static void DrawItemSlot(void* texResource, int count, ImDrawList* dl,
                             ImU32 borderCol = IM_COL32(120, 90, 35, 200),
                             float size = 0.0f, bool dim = false)
    {
        const float  S      = size > 0.0f ? size : s_IconSize;
        // Snap the slot to whole pixels: keeps the icon edge-to-edge in its box
        // and the border from dropping an edge at fractional positions.
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        cursor.x = std::floor(cursor.x); cursor.y = std::floor(cursor.y);
        ImGui::SetCursorScreenPos(cursor);
        const ImVec2 br     = ImVec2(cursor.x + S, cursor.y + S);

        if (texResource)
        {
            // Dimmed icons (search misses) render darkened like the in-game search
            const ImVec4 tint = dim ? ImVec4(0.38f, 0.38f, 0.38f, 1.0f) : ImVec4(1, 1, 1, 1);
            ImGui::Image(reinterpret_cast<ImTextureID>(texResource), ImVec2(S, S),
                         ImVec2(0, 0), ImVec2(1, 1), tint);
        }
        else
        {
            // Dark placeholder that looks like an empty GW2 slot
            dl->AddRectFilled(cursor, br, IM_COL32(40, 35, 25, 200));
            const char* q  = "?";
            const ImVec2 ts = ImGui::CalcTextSize(q);
            dl->AddText(ImVec2(cursor.x + (S - ts.x) * 0.5f,
                               cursor.y + (S - ts.y) * 0.5f),
                        IM_COL32(120, 100, 60, 160), q);
            ImGui::Dummy(ImVec2(S, S));
        }

        // Slot border — rarity coloured like the GW2 inventory (amber default),
        // drawn fully inside the box so neighbouring slots never clip it
        dl->AddRect(ImVec2(cursor.x + 0.5f, cursor.y + 0.5f), ImVec2(br.x - 0.5f, br.y - 0.5f),
                    borderCol, 0.0f, 0, 1.0f);
        dl->AddRect(ImVec2(cursor.x + 1.5f, cursor.y + 1.5f), ImVec2(br.x - 1.5f, br.y - 1.5f),
                    (borderCol & 0x00FFFFFF) | 0x60000000, 0.0f, 0, 1.0f); // soft inner line

        // Count badge (stacks only) — dark box + gold text, readable on any icon
        if (count > 1)
        {
            char cntBuf[12];
            snprintf(cntBuf, sizeof(cntBuf), "%d", count);
            const ImVec2 ts      = ImGui::CalcTextSize(cntBuf);
            const float  pad     = 2.0f;
            const ImVec2 bgMin   = ImVec2(br.x - ts.x - pad * 2.0f, br.y - ts.y - pad);
            const ImVec2 bgMax   = ImVec2(br.x,                       br.y);
            const ImVec2 tPos    = ImVec2(bgMin.x + pad, bgMin.y + pad * 0.5f);
            dl->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 180), 2.0f);
            dl->AddText(tPos, IM_COL32(255, 220, 100, dim ? 120 : 255), cntBuf);
        }
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
        // The tooltip uses its own configured text size; the window font scale
        // bridges until the exact atlas size has been delivered.
        ImFont* tipFont = s_FontTooltip ? s_FontTooltip : ImGui::GetFont();
        ImGui::PushFont(tipFont);
        float tipScale = (tipFont && tipFont->FontSize > 1.0f) ? s_TooltipPx / tipFont->FontSize : 1.0f;
        if (tipScale < 0.5f)      tipScale = 0.5f;
        else if (tipScale > 2.0f) tipScale = 2.0f;
        ImGui::SetWindowFontScale(tipScale);
        // Fixed-pixel tooltip metrics (icons, gaps) follow the tooltip size
        const float tui = s_TooltipPx / Constants::FontBodySize;
        // GW2 tooltip texture as background, cropped 1:1 (942x942). Window size
        // lags one frame while the tooltip resizes; content is static per item.
        if (void* bg = GetTex(Constants::TooltipBgId, m_TexTooltipBg))
        {
            const ImVec2 tp = ImGui::GetWindowPos();
            const ImVec2 ts = ImGui::GetWindowSize();
            const ImVec2 uv1 = { ts.x < 942.0f ? ts.x / 942.0f : 1.0f,
                                 ts.y < 942.0f ? ts.y / 942.0f : 1.0f };
            ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(bg),
                tp, ImVec2(tp.x + ts.x, tp.y + ts.y), ImVec2(0, 0), uv1,
                IM_COL32(255, 255, 255, 230));
        }

        // Cap the tooltip width: long descriptions wrap instead of stretching
        // the tooltip across the screen.
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);

        // ── Header: icon + name in the larger tooltip-title Menomonia (like
        //    the in-game tooltip, where the item name is larger than the body) ─
        const float hIcon = std::floor(32.0f * tui);
        if (texIcon)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(texIcon), ImVec2(hIcon, hIcon));
            ImGui::SameLine(0.0f, 8.0f * tui);
        }
        if (s_FontTipTitle) ImGui::PushFont(s_FontTipTitle);
        if (texIcon)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (hIcon - ImGui::GetTextLineHeight()) * 0.5f);
        ImGui::TextColored(RarityColor(item), "%s", DisplayName(item).c_str());
        if (s_FontTipTitle) ImGui::PopFont();

        // Line under the header — always, for every item
        ImGui::Spacing();
        ImGui::Separator();

        const bool isArmor  = (item.type == "Armor");
        const bool isWeapon = (item.type == "Weapon");

        // Whether anything renders between the header line and the footer info
        // (stats, buffs, consumable effect, slotted upgrades) — only then is a
        // second separator above the rarity/type footer needed.
        const bool hasBody =
               !item.buffDescription.empty() || !item.bonuses.empty()
            || (isArmor && item.defense > 0) || (isWeapon && item.maxPower > 0)
            || !item.statName.empty() || !item.attributes.empty()
            || (item.type == "Consumable" && (item.durationMs > 0 || !item.consumableDesc.empty()))
            || !item.upgradeSlots.empty();

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
        // Runes/sigils/infusions state their stats in the buff description
        // already — listing the attributes too would duplicate those lines.
        if (item.buffDescription.empty())
            for (const auto& [attr, val] : item.attributes)
                ImGui::TextColored(kAttr, "+%d %s", val, Lang::TranslateAttribute(attr.c_str()));

        // Consumable effect (food / utility nourishment) with duration.
        // Ascended food often has no description/duration in the API -> still show
        // the food icon + the 1h duration filled in by the service.
        if (item.type == "Consumable" && (item.durationMs > 0 || !item.consumableDesc.empty()))
        {
            ImGui::Spacing();
            // Prefer the real effect/buff icon from the API (details.icon). Ascended
            // food has no such icon in the API, so it falls back to the static icon.
            void* nIcon = item.consumableIconUrl.empty() ? nullptr
                                                         : self->GetOrLoadTexture(item.consumableIconUrl);
            if (!nIcon)
                nIcon = (item.subType == "Utility")                       ? m_UtilityIcon
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
            {
                const int totalSec = item.durationMs / 1000;
                const int days  = totalSec / 86400;
                const int hours = (totalSec % 86400) / 3600;
                const int mins  = (totalSec % 3600) / 60;
                const int secs  = totalSec % 60;
                std::string dur;
                if (days  > 0)                dur += std::to_string(days)  + s.durDay  + " ";
                if (hours > 0)                dur += std::to_string(hours) + s.durHour + " ";
                if (mins  > 0)                dur += std::to_string(mins)  + s.durMin  + " ";
                if (secs  > 0 || dur.empty()) dur += std::to_string(secs)  + s.durSec;
                else if (!dur.empty())        dur.pop_back(); // trim trailing space
                ImGui::TextColored(kBuff, "(%s)", dur.c_str());
            }
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
                    const float uIcon = std::floor(20.0f * tui);
                    ImGui::Image(reinterpret_cast<ImTextureID>(utex), ImVec2(uIcon, uIcon));
                    ImGui::SameLine(0.0f, 6.0f * tui);
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (uIcon - ImGui::GetTextLineHeight()) * 0.5f);
                }
                ImGui::TextColored(RarityColor(eu.rarity), "%s", eu.name.empty() ? "???" : eu.name.c_str());
                if (!eu.buffDescription.empty())
                    ImGui::TextColored(kBuff, "%s", eu.buffDescription.c_str());
                renderBonuses(eu.bonuses);
            }
        }

        // ── Footer separator (only when body content sits above it — for
        //    plain items the header line already serves as the divider) ───────
        if (hasBody)
        {
            ImGui::Spacing();
            ImGui::Separator();
        }
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

        // Description (wraps at the tooltip max width pushed above)
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

        ImGui::PopTextWrapPos();
        ImGui::PopFont();
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

        const float kCellPadY = std::floor(3.0f * s_Ui); // symmetric vertical padding for item rows
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, kCellPadY));
        if (!ImGui::BeginTable(id, cols, tableFlags, tableSize)) { ImGui::PopStyleVar(); return; }

        ImGui::TableSetupColumn(s.colItem, ImGuiTableColumnFlags_WidthStretch, showLocation ? 0.55f : 0.82f);
        if (showLocation)
            ImGui::TableSetupColumn(s.colLocation, ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn(s.colCount, ImGuiTableColumnFlags_WidthFixed, 52.0f * s_Ui);
        if (scroll)
            ImGui::TableSetupScrollFreeze(0, 1);

        // Header row with extra vertical padding (taller than item rows)
        {
            const float th       = ImGui::GetTextLineHeight();
            const float headerH  = th + 12.0f * s_Ui;
            ImGui::TableNextRow(ImGuiTableRowFlags_Headers, headerH);
            for (int c = 0; c < cols; ++c)
            {
                ImGui::TableSetColumnIndex(c);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (headerH - th) * 0.5f - kCellPadY);
                ImGui::TableHeader(ImGui::TableGetColumnName(c));
            }
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();

        // The icon (s_IconSize) drives the row height. Text is drawn via the draw list
        // (not ImGui::Text) so the host font's tall line box doesn't inflate the row;
        // it is vertically centered to the icon.
        const float fontH = ImGui::GetFontSize();

        auto drawText = [&](float x, float cellTopY, const ImVec4& col, const char* txt)
        {
            const ImVec2 pos(x, cellTopY + (s_IconSize - fontH) * 0.5f);
            dl->AddText(pos, ImGui::ColorConvertFloat4ToU32(col), txt);
        };

        auto drawRow = [&](const FoundItem* item)
        {
            ImGui::TableNextRow();

            // Col 0: icon slot + name
            ImGui::TableSetColumnIndex(0);
            const ImVec2 c0 = ImGui::GetCursorScreenPos();
            void* tex = GetOrLoadTexture(item->iconUrl);
            const ImVec4 rarCol = RarityColor(*item);
            DrawItemSlot(tex, item->count, dl,    // ImGui::Image(s_IconSize) -> sets row height
                         ImGui::ColorConvertFloat4ToU32(ImVec4(rarCol.x, rarCol.y, rarCol.z, 0.90f)));
            const std::string name = DisplayName(*item);
            drawText(c0.x + s_IconSize + 7.0f * s_Ui, c0.y, RarityColor(*item), name.c_str());

            const float nameW = ImGui::CalcTextSize(name.c_str()).x;
            const bool hovered = ImGui::IsMouseHoveringRect(
                c0, ImVec2(c0.x + s_IconSize + 7.0f * s_Ui + nameW, c0.y + s_IconSize));

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
            clipper.Begin(static_cast<int>(items.size()), s_IconSize + kCellPadY * 2.0f);
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

    // ---------- GW2-style slot grid + equipment slot ----------

    void ItemSearchWindow::RenderItemGrid(const char* id,
                                          const std::vector<std::pair<const FoundItem*, bool>>& items,
                                          bool searching,
                                          const FoundItem*& ioHover, void*& ioHoverTex)
    {
        if (items.empty()) { ImGui::TextDisabled("-"); return; }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float S    = s_SlotSize; // same slot size as the equipment panel
        const float g    = std::floor(3.0f * s_Ui);
        const float xMax = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;

        ImGui::PushID(id);
        // Uniform slot gaps horizontally and between the wrapped rows
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(g, g));
        bool first = true;
        int  idx   = 0;
        for (const auto& [item, matched] : items)
        {
            // Wrap to a new line when the next slot would overflow the width
            if (!first && ImGui::GetItemRectMax().x + g + S <= xMax)
                ImGui::SameLine(0.0f, g);
            first = false;

            ImGui::PushID(idx++);
            if (!item)
            {
                DrawEmptySlot(dl, S);
            }
            else
            {
                const bool  dim = searching && !matched;
                void*       tex = GetOrLoadTexture(item->iconUrl);
                const ImVec4 rc = RarityColor(*item);
                const ImU32 border = (searching && matched)
                    ? ImGui::ColorConvertFloat4ToU32(kGold)
                    : ImGui::ColorConvertFloat4ToU32(ImVec4(rc.x, rc.y, rc.z, dim ? 0.30f : 0.90f));
                DrawItemSlot(tex, item->count, dl, border, S, dim);
                if (!ioHover && ImGui::IsItemHovered()) { ioHover = item; ioHoverTex = tex; }
            }
            ImGui::PopID();
        }
        ImGui::PopStyleVar();
        ImGui::PopID();
    }

    void ItemSearchWindow::DrawEquipSlot(const FoundItem* item, bool matched, bool searching,
                                         float size, const FoundItem*& ioHover, void*& ioHoverTex)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (!item)
        {
            DrawEmptySlot(dl, size);
            return;
        }
        const bool  dim = searching && !matched;
        void*       tex = GetOrLoadTexture(item->iconUrl);
        const ImVec4 rc = RarityColor(*item);
        const ImU32 border = (searching && matched)
            ? ImGui::ColorConvertFloat4ToU32(kGold)
            : ImGui::ColorConvertFloat4ToU32(ImVec4(rc.x, rc.y, rc.z, dim ? 0.30f : 0.90f));
        DrawItemSlot(tex, item->count, dl, border, size, dim);
        if (!ioHover && ImGui::IsItemHovered()) { ioHover = item; ioHoverTex = tex; }
    }

    // ---------- window chrome (GW2 / Blish HUD style) ----------

    void* ItemSearchWindow::GetTex(const char* id, void*& cache) const
    {
        if (!cache && m_Api && m_Api->Textures_Get)
            if (auto* t = m_Api->Textures_Get(id)) cache = t->Resource;
        return cache;
    }

    // GW2-style button, drawn in the same achievement-bar style as the tabs
    // and section headers (khaki gradient, top sheen, dark border, gold label;
    // hover fades the bar brighter and in a gold border). Drawn procedurally:
    // the "button states" atlas page carries packed junk around its frames, so
    // any sampled region shows artefacts.
    bool ItemSearchWindow::Gw2Button(const char* label, float height, bool disabled)
    {
        // Buttons use their own configured text size (fallback: current font
        // scaled to that px until the atlas size has been delivered)
        ImFont*     bf  = s_FontButton ? s_FontButton : ImGui::GetFont();
        const float bfs = s_ButtonPx;
        if (height <= 0.0f) height = bfs + ImGui::GetStyle().FramePadding.y * 2.0f + 2.0f;
        const ImVec2 textSz = bf->CalcTextSizeA(bfs, FLT_MAX, 0.0f, label);
        const ImVec2 size   = { textSz.x + 32.0f * s_Ui, height };
        const ImVec2 p      = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton(label, size);
        const bool hovered = !disabled && ImGui::IsItemHovered();
        const bool clicked = !disabled && ImGui::IsItemClicked();
        const bool held    = !disabled && ImGui::IsItemActive();

        // Hover progress (0 = idle, 1 = fully lit), animated over ~0.2 s and
        // kept per button id in the window's state storage.
        ImGuiStorage* store = ImGui::GetStateStorage();
        const ImGuiID hid   = ImGui::GetID(label);
        float t = store->GetFloat(hid, 0.0f);
        t += (hovered ? 1.0f : -1.0f) * ImGui::GetIO().DeltaTime / 0.2f;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        store->SetFloat(hid, t);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 pMax = { p.x + size.x, p.y + size.y };
        // Same gradient formula as the tab bars, on the warm khaki base;
        // pressing dims the bar slightly.
        const ImVec4 base = { 0.85f, 0.72f, 0.38f, 1.0f };
        float kTop = 0.30f + 0.20f * t;
        float kBot = 0.18f + 0.15f * t;
        if (held) { kTop *= 0.80f; kBot *= 0.80f; }
        const float alpha = disabled ? 0.55f : 0.96f;
        const ImU32 cTop = ImGui::ColorConvertFloat4ToU32(ImVec4(base.x * kTop, base.y * kTop, base.z * kTop, alpha));
        const ImU32 cBot = ImGui::ColorConvertFloat4ToU32(ImVec4(base.x * kBot, base.y * kBot, base.z * kBot, alpha));
        dl->AddRectFilledMultiColor(p, pMax, cTop, cTop, cBot, cBot);
        dl->AddRectFilled(p, ImVec2(pMax.x, p.y + 1.0f), IM_COL32(255, 255, 255, 28)); // top sheen
        dl->AddRect(p, pMax, IM_COL32(0, 0, 0, 210));
        if (!disabled && t > 0.01f) // gold border fades in like the active tabs
            dl->AddRect(p, pMax, ImGui::ColorConvertFloat4ToU32(ImVec4(kGold.x, kGold.y, kGold.z, t)),
                        0.0f, ImDrawCornerFlags_All, 1.5f);
        const ImU32 txtCol = disabled ? IM_COL32(150, 150, 150, 160)
                                      : ImGui::ColorConvertFloat4ToU32(kGold);
        dl->AddText(bf, bfs, ImVec2(p.x + (size.x - textSz.x) * 0.5f, p.y + (size.y - textSz.y) * 0.5f),
                    txtCol, label);
        return clicked;
    }

    void ItemSearchWindow::RenderWindowChrome(AppState& state, const char* titleText)
    {
        // Geometry mirrors Blish HUD's WindowBase2: a 40 px title bar cut from the
        // 1024x64 strip (rows 11..51), the 128x64 top-right corner overhanging the
        // window edge by 16 px right / 11 px up, and a 32x32 exit button.
        // All chrome metrics follow the UI-size factor.
        const float kBarH = std::floor(40.0f * s_Ui);
        const ImVec2 wp = ImGui::GetWindowPos();
        const ImVec2 ws = ImGui::GetWindowSize();
        ImDrawList* dl  = ImGui::GetWindowDrawList();

        auto tex = [&](const char* id, void*& cache) { return GetTex(id, cache); };

        // Smoky GW2 window body (asset 155985; the original's alpha gradient is
        // flattened to a constant in our copy), stretched over the full window.
        if (void* bg = tex(Constants::WndBackgroundId, m_TexWindowBg))
            dl->AddImage(reinterpret_cast<ImTextureID>(bg),
                         wp, ImVec2(wp.x + ws.x, wp.y + ws.y),
                         ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));

        const ImVec2 barMin = wp;
        const ImVec2 barMax = { wp.x + ws.x, wp.y + kBarH };
        const bool barHovered = ImGui::IsMouseHoveringRect(barMin, barMax, false);

        void* barTex = barHovered ? tex(Constants::WndTitlebarActiveId, m_TexTitlebarActive)
                                  : tex(Constants::WndTitlebarId,       m_TexTitlebar);
        void* trTex  = barHovered ? tex(Constants::WndTopRightActiveId, m_TexTopRightActive)
                                  : tex(Constants::WndTopRightId,       m_TexTopRight);
        if (!barTex) barTex = tex(Constants::WndTitlebarId, m_TexTitlebar);
        if (!trTex)  trTex  = tex(Constants::WndTopRightId, m_TexTopRight);

        // Corner piece and emblem hang slightly past the window edges
        dl->PushClipRect(ImVec2(wp.x - 8.0f * s_Ui, wp.y - 16.0f * s_Ui),
                         ImVec2(wp.x + ws.x + 20.0f * s_Ui, wp.y + ws.y), false);

        if (barTex)
        {
            // Texel density follows the UI scale (1:1 at 100%), stretch only if wider
            const float u1 = ws.x < 1024.0f * s_Ui ? ws.x / (1024.0f * s_Ui) : 1.0f;
            dl->AddImage(reinterpret_cast<ImTextureID>(barTex), barMin, barMax,
                         ImVec2(0.0f, 11.0f / 64.0f), ImVec2(u1, 51.0f / 64.0f));
        }
        else
        {
            dl->AddRectFilled(barMin, barMax, IM_COL32(0, 0, 0, 230)); // textures still loading
        }

        if (trTex)
            dl->AddImage(reinterpret_cast<ImTextureID>(trTex),
                         ImVec2(wp.x + ws.x - 112.0f * s_Ui, wp.y - 11.0f * s_Ui),
                         ImVec2(wp.x + ws.x + 16.0f * s_Ui,  wp.y + 53.0f * s_Ui));

        // Emblem: the addon icon, sized to the bar like Blish window emblems
        float titleX = wp.x + 16.0f * s_Ui;
        if (void* em = tex(Constants::IconId, m_TexEmblem))
        {
            const float kEmblem = std::floor(40.0f * s_Ui);
            dl->AddImage(reinterpret_cast<ImTextureID>(em),
                         ImVec2(wp.x + 6.0f * s_Ui, wp.y),
                         ImVec2(wp.x + 6.0f * s_Ui + kEmblem, wp.y + kEmblem));
            titleX = wp.x + 6.0f * s_Ui + kEmblem + 8.0f * s_Ui;
        }

        // Title in the big GW2 font (Menomonia) at its wanted px size
        ImFont* fBig = s_FontTitle;
        if (!fBig && m_NexusLink) fBig = static_cast<ImFont*>(m_NexusLink->FontBig);
        if (fBig)
        {
            dl->AddText(fBig, s_TitlePx,
                        ImVec2(titleX, wp.y + (kBarH - s_TitlePx) * 0.5f),
                        IM_COL32(255, 255, 255, 255), titleText);
        }
        else
            dl->AddText(ImVec2(titleX, wp.y + (kBarH - ImGui::GetTextLineHeight()) * 0.5f),
                        IM_COL32(255, 255, 255, 255), titleText);

        // Exit button (drawn after the corner piece so it sits on top of it)
        {
            const float kExit = std::floor(32.0f * s_Ui);
            const ImVec2 exMin = { wp.x + ws.x - kExit - 8.0f * s_Ui, wp.y + (kBarH - kExit) * 0.5f };
            ImGui::SetCursorScreenPos(exMin);
            ImGui::InvisibleButton("##chromeclose", ImVec2(kExit, kExit));
            const bool exHovered = ImGui::IsItemHovered();
            if (ImGui::IsItemClicked())
                state.showWindow.store(false, std::memory_order_relaxed);
            void* ex = exHovered ? tex(Constants::WndExitActiveId, m_TexExitActive)
                                 : tex(Constants::WndExitId,       m_TexExit);
            if (!ex) ex = tex(Constants::WndExitId, m_TexExit);
            if (ex)
                dl->AddImage(reinterpret_cast<ImTextureID>(ex), exMin,
                             ImVec2(exMin.x + kExit, exMin.y + kExit));
            else
                dl->AddText(ImVec2(exMin.x + 12.0f * s_Ui, exMin.y + 8.0f * s_Ui), IM_COL32(255, 255, 255, 255), "x");
        }

        dl->PopClipRect();

        // Start the window content below the bar
        ImGui::SetCursorScreenPos(ImVec2(wp.x + ImGui::GetStyle().WindowPadding.x, wp.y + kBarH + 8.0f * s_Ui));
    }

    // ---------- main render ----------

    void ItemSearchWindow::Render(AppState& state, bool& outRequestRefresh)
    {
        outRequestRefresh = false;
        const auto& s = Lang::Get();

        if (!state.showWindow.load(std::memory_order_relaxed)) return;

        // GW2 font (Menomonia at the fixed Blish body size) + Blish-style theme,
        // scoped to this window only. Falls back to the Nexus-shared Menomonia
        // until our own font finished loading.
        if (!m_NexusLink && m_Api)
            m_NexusLink = static_cast<NexusLinkData_t*>(m_Api->DataLink_Get(DL_NEXUS_LINK));

        // Per-role font sizes from the options, rounded to whole pixels —
        // fractional sizes rasterize visibly soft. Each size is registered
        // once as its own crisp Nexus atlas font (EnsureFont); the game's
        // UI-size scaling (NexusLink->Scaling) is deliberately NOT applied.
        const PluginConfig sizeCfg = GetConfig(state);
        const float wantBody     = std::round(sizeCfg.fontSize);
        const float wantHeading  = std::round(sizeCfg.headingSize);
        const float wantButton   = std::round(sizeCfg.buttonSize);
        const float wantTooltip  = std::round(sizeCfg.tooltipSize);
        const float wantTitle    = std::round(wantBody    * Constants::FontTitleScale);
        const float wantTipTitle = std::round(wantTooltip * Constants::FontTipTitleScale);

        ImFont* fontBody = EnsureFont(state, wantBody);
        if (!fontBody && m_NexusLink) fontBody = static_cast<ImFont*>(m_NexusLink->Font);

        // The wanted body size takes effect immediately via the per-window
        // font scale (wanted px / current atlas px); once the async atlas
        // rebuild delivered the exact size the ratio is back to ~1 and crisp.
        m_EffFontScale = (fontBody && fontBody->FontSize > 1.0f) ? wantBody / fontBody->FontSize : 1.0f;
        if (m_EffFontScale < 0.5f)      m_EffFontScale = 0.5f;
        else if (m_EffFontScale > 2.0f) m_EffFontScale = 2.0f;

        // Publish the per-frame metrics for the file-static draw helpers
        s_Ui           = wantBody / Constants::FontBodySize;
        s_IconSize     = std::floor(36.0f * s_Ui);
        s_SlotSize     = std::floor(40.0f * s_Ui);
        s_FontHeading  = EnsureFont(state, wantHeading);  s_HeadingPx = wantHeading;
        s_FontButton   = EnsureFont(state, wantButton);   s_ButtonPx  = wantButton;
        s_FontTooltip  = EnsureFont(state, wantTooltip);  s_TooltipPx = wantTooltip;
        s_FontTipTitle = EnsureFont(state, wantTipTitle);
        s_FontTitle    = EnsureFont(state, wantTitle);    s_TitlePx   = wantTitle;

        ThemeScope theme(fontBody, s_Ui);

        // Title only changes with the language (static string per language)
        if (m_WindowTitleSrc != s.windowTitle)
        {
            m_WindowTitleSrc = s.windowTitle;
            m_WindowTitle    = std::string(s.windowTitle) + Constants::WindowId;
        }
        const std::string& title = m_WindowTitle;
        ImGui::SetNextWindowSizeConstraints(ImVec2(300.0f * s_Ui, 200.0f * s_Ui), ImVec2(FLT_MAX, FLT_MAX));
        ImGui::SetNextWindowSize(ImVec2(580.0f * s_Ui, 500.0f * s_Ui), ImGuiCond_FirstUseEver);
        // The native title bar is replaced by the GW2-style chrome; the window
        // name (and thus escape-close registration and saved settings) stays.
        if (!ImGui::Begin(title.c_str(), nullptr, ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::End();
            return;
        }
        // Per-window font scale: applies the wanted size instantly (and clears
        // stale values stuck on the window object in the shared ImGui context).
        ImGui::SetWindowFontScale(m_EffFontScale);
        RenderWindowChrome(state, s.windowTitle);

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
            ImGui::SameLine(0.0f, 12.0f * s_Ui);

            const float now      = static_cast<float>(ImGui::GetTime());
            const float since    = now - m_LastRefreshTime;
            const bool  hasError = !fetchErr.empty();
            const bool  cooldown = (since < 300.0f) && !hasError; // retry immediately after an error
            const bool  canRefresh = !fetching && hasKey && !cooldown;

            if (canRefresh)
            {
                if (Gw2Button(btnLabel))
                {
                    outRequestRefresh = true;
                    m_LastRefreshTime = now;
                }
            }
            else
            {
                Gw2Button(btnLabel, 0.0f, true);
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

            // ── Search bar (full width), on the native GW2 textbox texture ───
            ImGui::Spacing();
            {
                // Roomier inner padding for the search text; pushed before the
                // height is computed so the texture matches the frame height.
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f * s_Ui, 7.0f * s_Ui));
                // 3-slice draw of the 516x27 textbox strip (8 px end caps) so the
                // baked border isn't stretched away.
                const ImVec2 p = ImGui::GetCursorScreenPos();
                const float  w = ImGui::GetContentRegionAvail().x;
                const float  h = ImGui::GetFrameHeight();
                if (void* tb = GetTex(Constants::TextboxId, m_TexTextbox))
                {
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    auto* t = reinterpret_cast<ImTextureID>(tb);
                    const float capU = 8.0f / 516.0f;
                    dl->AddImage(t, p, ImVec2(p.x + 8.0f, p.y + h), ImVec2(0, 0), ImVec2(capU, 1));
                    dl->AddImage(t, ImVec2(p.x + 8.0f, p.y), ImVec2(p.x + w - 8.0f, p.y + h),
                                 ImVec2(capU, 0), ImVec2(1.0f - capU, 1));
                    dl->AddImage(t, ImVec2(p.x + w - 8.0f, p.y), ImVec2(p.x + w, p.y + h),
                                 ImVec2(1.0f - capU, 0), ImVec2(1, 1));
                }
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f); // border is baked into the texture
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (ImGui::InputTextWithHint("##search", s.searchHint, m_SearchBuf.data(), m_SearchBuf.size()))
                    m_FilterLower = Utility::ToLower(m_SearchBuf.data());
                ImGui::PopStyleVar(2); // frame border + frame padding
                ImGui::PopStyleColor();
            }
            if (m_SearchBuf[0] == '\0') m_FilterLower.clear();
        }
        ImGui::Spacing();

        // ── Loading spinner ───────────────────────────────────────────────────
        if (fetching)
        {
            const float kSpinRadius = 12.0f * s_Ui;
            const float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - kSpinRadius * 2.0f) * 0.5f);
            RenderLoadingSpinner(kSpinRadius, 3.5f * s_Ui);
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
            // Keyed by DISPLAYED name (not item id): e.g. stat variants of the
            // same infusion share one name across many ids — per id they would
            // show as identical-looking duplicate rows. Stat prefix and skin
            // are part of the display name, so genuinely different-looking
            // items stay separate; unresolved names fall back to the id.
            using AggKey = std::tuple<std::string, uint8_t, std::string>;
            std::map<AggKey, FoundItem> aggMap;
            for (const auto& item : raw)
            {
                std::string nameKey = DisplayName(item);
                if (item.name.empty() && item.skinName.empty())
                    nameKey += "#" + std::to_string(item.itemId);
                AggKey key{std::move(nameKey), static_cast<uint8_t>(item.locationType), item.characterName};
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

            // Character bag items + shared inventory kept per slot (not
            // aggregated) for the GW2-style full-inventory grids
            m_CharInvRaw.clear();
            m_SharedRaw.clear();
            for (const auto& item : raw)
            {
                if (item.locationType == ItemLocation::Character)
                    m_CharInvRaw.push_back(item);
                else if (item.locationType == ItemLocation::SharedInventory)
                    m_SharedRaw.push_back(item);
            }
            auto bySlot = [](const FoundItem& a, const FoundItem& b) { return a.bankSlot < b.bankSlot; };
            std::sort(m_CharInvRaw.begin(), m_CharInvRaw.end(), bySlot);
            std::sort(m_SharedRaw.begin(),  m_SharedRaw.end(),  bySlot);

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
            m_CharLevel.clear();
            m_CharRace.clear();
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
                    if (item.characterLevel > 0)
                        m_CharLevel[item.characterName] = item.characterLevel;
                    if (!item.characterRace.empty())
                        m_CharRace[item.characterName] = item.characterRace;
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

        // Draw one clickable tab (icon + coloured label); angular corners to match
        // the GW2 UI. Wraps to a new line instead of showing a horizontal scrollbar.
        const float kTabRound  = 0.0f;
        const float kTabSpacing = 4.0f * s_Ui;
        const float lineLimit  = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        bool firstTab = true;
        auto drawTab = [&](int index, void* icon, const char* label, ImVec4 col)
        {
            // Tab labels follow the configured button text size
            ImFont*     bf     = s_FontButton ? s_FontButton : ImGui::GetFont();
            const float bfs    = s_ButtonPx;
            const float iconSz = bfs + 2.0f;
            const float pad    = 8.0f * s_Ui;
            const float textW  = bf->CalcTextSizeA(bfs, FLT_MAX, 0.0f, label).x;
            const float w      = pad + (icon ? iconSz + 4.0f * s_Ui : 0.0f) + textW + pad;
            const float h      = iconSz + 8.0f * s_Ui;

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
            const ImVec2 pMax = { p.x + w, p.y + h };

            // Achievement-category style bar (see the GW2 achievement panel):
            // coloured vertical gradient in the profession colour, subtle top
            // sheen, dark border, gold label. Active tabs get a gold border.
            const float kTop = active ? 0.62f : hovered ? 0.50f : 0.36f;
            const float kBot = active ? 0.42f : hovered ? 0.33f : 0.22f;
            const ImU32 cTop = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x * kTop, col.y * kTop, col.z * kTop, 0.96f));
            const ImU32 cBot = ImGui::ColorConvertFloat4ToU32(ImVec4(col.x * kBot, col.y * kBot, col.z * kBot, 0.96f));
            dl->AddRectFilledMultiColor(p, pMax, cTop, cTop, cBot, cBot);
            dl->AddRectFilled(p, ImVec2(pMax.x, p.y + 1.0f), IM_COL32(255, 255, 255, 28)); // top sheen
            dl->AddRect(p, pMax, IM_COL32(0, 0, 0, 210), kTabRound);
            if (active)
                dl->AddRect(p, pMax, ImGui::ColorConvertFloat4ToU32(kGold), kTabRound,
                            ImDrawCornerFlags_All, 1.5f);

            float x = p.x + pad;
            const float cy = p.y + h * 0.5f;
            if (icon)
            {
                dl->AddImage(reinterpret_cast<ImTextureID>(icon),
                             ImVec2(x, cy - iconSz * 0.5f), ImVec2(x + iconSz, cy + iconSz * 0.5f));
                x += iconSz + 4.0f * s_Ui;
            }
            dl->AddText(bf, bfs, ImVec2(x, cy - bfs * 0.5f),
                        ImGui::ColorConvertFloat4ToU32(kGold), label);

            if (clicked) m_ActiveTab = index;
        };

        // Account/Bank get a neutral steel-blue bar; character tabs use their
        // profession colour as the bar tint (labels are always gold). Item
        // spacing is pinned so horizontal and wrapped-line gaps are identical.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kTabSpacing, kTabSpacing));
        static const ImVec4 kNeutralTabCol = { 0.48f, 0.54f, 0.66f, 1.0f };
        drawTab(0, nullptr, s.tabSearch, kNeutralTabCol);
        drawTab(1, nullptr, s.tabBank,   kNeutralTabCol);
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
        ImGui::PopStyleVar();
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
            // Bank tab: materials (left) | bank as full 30-slot grids (right).
            // While searching the grids dim non-matching slots instead of
            // hiding them. Views are cached; rebuilt on data/filter change.
            const bool searching = !m_FilterLower.empty();
            if (m_BankCacheVersion != m_AggregatedVersion || m_BankCacheFilter != m_FilterLower)
            {
                m_MatsAll.clear();
                for (const auto& item : m_AggregatedSnapshot)
                    if (item.locationType == ItemLocation::Materials)
                        m_MatsAll.emplace_back(&item,
                            searching && MatchesFilter(item, m_FilterLower.c_str()));

                constexpr int kBankTabSize = 30; // slots per bank tab
                int cap = 0;
                for (const auto& it : m_BankRaw)
                    if (it.containerSize > cap) cap = it.containerSize;
                if (cap == 0 && !m_BankRaw.empty())          // old cache: no capacity info,
                    cap = m_BankRaw.back().bankSlot + 1;     // derive from the last used slot
                const int tabs = (cap + kBankTabSize - 1) / kBankTabSize;
                m_BankTabGrids.assign(tabs, {});
                for (auto& grid : m_BankTabGrids)
                    grid.assign(kBankTabSize, { nullptr, false });
                for (const auto& it : m_BankRaw)
                {
                    const size_t t = static_cast<size_t>(it.bankSlot / kBankTabSize);
                    if (t >= m_BankTabGrids.size()) continue;
                    m_BankTabGrids[t][it.bankSlot % kBankTabSize] =
                        { &it, searching && MatchesFilter(it, m_FilterLower.c_str()) };
                }
                m_BankCacheVersion = m_AggregatedVersion;
                m_BankCacheFilter  = m_FilterLower;
            }

            const float availY = ImGui::GetContentRegionAvail().y;
            const float leftW  = ImGui::GetContentRegionAvail().x * 0.42f;

            // Left: material storage
            ImGui::BeginChild("##matcol", ImVec2(leftW, availY), false);
            ImGui::SetWindowFontScale(m_EffFontScale);
            if (SectionHeader(s.locMaterials))
                RenderItemGrid("##mats", m_MatsAll, searching, tooltipItem, tooltipTex);
            ImGui::EndChild();

            ImGui::SameLine();

            // Right: bank tabs, 30 slots each incl. empty ones
            ImGui::BeginChild("##bankcol", ImVec2(0.0f, availY), false);
            ImGui::SetWindowFontScale(m_EffFontScale);
            for (size_t t = 0; t < m_BankTabGrids.size(); ++t)
            {
                char secBuf[64];
                std::snprintf(secBuf, sizeof(secBuf), "%s %d", s.bankTab, static_cast<int>(t) + 1);
                if (SectionHeader(secBuf))
                {
                    const std::string tid = "##bank" + std::to_string(t);
                    RenderItemGrid(tid.c_str(), m_BankTabGrids[t], searching, tooltipItem, tooltipTex);
                }
                ImGui::Spacing();
            }
            if (m_BankTabGrids.empty()) ImGui::TextDisabled("-");
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

            // Rebuild the cached character views only when data, filter or the
            // shown character changed (not every frame).
            const bool charDirty = m_CharCacheVersion != m_AggregatedVersion
                                || m_CharCacheName    != charName
                                || m_CharCacheFilter  != m_FilterLower;
            if (charDirty)
            {
                // Full inventories per bag slot (incl. empty slots), like the
                // in-game inventory. Items land at their slot index; the
                // capacity comes from the container size (0 for items from an
                // old cache -> plain grid without empty-slot padding). While
                // searching the grids dim non-matching slots instead of hiding.
                auto buildGrid = [&](const std::vector<FoundItem>& rawItems, bool thisCharOnly,
                                     std::vector<std::pair<const FoundItem*, bool>>& outGrid)
                {
                    int cap = 0;
                    for (const auto& it : rawItems)
                    {
                        if (thisCharOnly && it.characterName != charName) continue;
                        if (it.containerSize > cap) cap = it.containerSize;
                    }
                    outGrid.assign(cap, { nullptr, false });
                    for (const auto& it : rawItems)
                    {
                        if (thisCharOnly && it.characterName != charName) continue;
                        const bool match = searching && MatchesFilter(it, m_FilterLower.c_str());
                        if (it.bankSlot >= 0 && it.bankSlot < cap && !outGrid[it.bankSlot].first)
                            outGrid[it.bankSlot] = { &it, match };
                        else
                            outGrid.emplace_back(&it, match);
                    }
                };
                buildGrid(m_CharInvRaw, true,  m_CharInv);    // this character's bags
                buildGrid(m_SharedRaw,  false, m_CharShared); // account-wide shared slots

                // Equipment templates (tabs) for this character, from the raw per-tab data
                m_EquipTabs.clear();
                for (const auto& it : m_EquipRaw)
                {
                    if (it.characterName != charName) continue;
                    if (it.equipTabIdx < 0) continue; // gathering/fishing/jade bot: not tab-specific
                    EquipTabInfo* tab = nullptr;
                    for (auto& t : m_EquipTabs)
                        if (t.idx == it.equipTabIdx) { tab = &t; break; }
                    if (!tab)
                    {
                        m_EquipTabs.push_back({ it.equipTabIdx, it.equipTabName, false, false });
                        tab = &m_EquipTabs.back();
                    }
                    if (it.equipTabActive) tab->active = true;
                    if (searching && !tab->matched && MatchesFilter(it, m_FilterLower.c_str()))
                        tab->matched = true;
                }
                std::sort(m_EquipTabs.begin(), m_EquipTabs.end(),
                          [](const EquipTabInfo& a, const EquipTabInfo& b) { return a.idx < b.idx; });

                m_CharCacheVersion = m_AggregatedVersion;
                m_CharCacheName    = charName;
                m_CharCacheFilter  = m_FilterLower;
                m_CharCacheSel     = -2; // force the equipment buckets to rebuild below
            }

            // Resolve the selected template (default = active set, else the first tab)
            int sel = -1;
            if (auto si = m_SelEquipTab.find(charName); si != m_SelEquipTab.end()) sel = si->second;
            bool selValid = false;
            for (const auto& t : m_EquipTabs) if (t.idx == sel) { selValid = true; break; }
            if (!selValid)
            {
                sel = m_EquipTabs.empty() ? -1 : m_EquipTabs.front().idx;
                for (const auto& t : m_EquipTabs) if (t.active) { sel = t.idx; break; }
                m_SelEquipTab[charName] = sel;
            }

            const float availY = ImGui::GetContentRegionAvail().y;
            // Equipment column: a bit under a third of the window — the rest
            // belongs to the inventory grid.
            const float leftW = ImGui::GetContentRegionAvail().x * 0.32f;

            // ── Left: template switcher + equipment sections (the child scrolls) ──
            ImGui::BeginChild("##equipcol", ImVec2(leftW, availY), false);
            ImGui::SetWindowFontScale(m_EffFontScale);

            // Template tabs — styled like the character tabs (angular, class-coloured
            // label + active border), just without an icon. Active set preselected;
            // when searching, tabs containing a match get a dim class-colour fill.
            const float kEqTabRound   = 0.0f;
            const float kEqTabSpacing = 4.0f * s_Ui;
            if (!m_EquipTabs.empty()) ImGui::Spacing(); // same gap above the row as the character tabs
            const float eqColRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            bool firstEqTab = true;
            auto drawEquipTab = [&](int tabIdx, const char* label, const char* hoverName,
                                    bool selected, bool matched) -> bool
            {
                // Same button text size and height as the character tabs; the
                // numbered tabs get a square-ish box like the in-game panel
                ImFont*     bf    = s_FontButton ? s_FontButton : ImGui::GetFont();
                const float pad   = 8.0f * s_Ui;
                const float th    = s_ButtonPx;
                const float textW = bf->CalcTextSizeA(th, FLT_MAX, 0.0f, label).x;
                const float h     = (th + 2.0f) + 8.0f * s_Ui;
                float       w     = pad + textW + pad;
                if (w < h) w = h;

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
                const ImVec2 pMax = { p.x + w, p.y + h };

                // Same achievement-bar style as the character tabs; a search
                // match brightens the bar like a hover so it stays noticeable.
                const bool lit = hovered || matched;
                const float kTop = selected ? 0.62f : lit ? 0.50f : 0.36f;
                const float kBot = selected ? 0.42f : lit ? 0.33f : 0.22f;
                const ImU32 cTop = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(profCol.x * kTop, profCol.y * kTop, profCol.z * kTop, 0.96f));
                const ImU32 cBot = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(profCol.x * kBot, profCol.y * kBot, profCol.z * kBot, 0.96f));
                dl->AddRectFilledMultiColor(p, pMax, cTop, cTop, cBot, cBot);
                dl->AddRectFilled(p, ImVec2(pMax.x, p.y + 1.0f), IM_COL32(255, 255, 255, 28)); // top sheen
                dl->AddRect(p, pMax, IM_COL32(0, 0, 0, 210), kEqTabRound);
                if (selected)
                    dl->AddRect(p, pMax, ImGui::ColorConvertFloat4ToU32(kGold), kEqTabRound,
                                ImDrawCornerFlags_All, 1.5f);
                dl->AddText(bf, th, ImVec2(p.x + (w - textW) * 0.5f, p.y + (h - th) * 0.5f),
                            ImGui::ColorConvertFloat4ToU32(kGold), label);

                // Template name on hover (like the in-game numbered tabs)
                if (hovered && hoverName && hoverName[0])
                {
                    ImGui::BeginTooltip();
                    ImGui::SetWindowFontScale(m_EffFontScale);
                    ImGui::TextUnformatted(hoverName);
                    ImGui::EndTooltip();
                }
                return clicked;
            };
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kEqTabSpacing, kEqTabSpacing));
            // Center the numbered tab row over the panel (only when it fits on
            // one line; wrapped rows stay left-aligned)
            {
                ImFont*     bf   = s_FontButton ? s_FontButton : ImGui::GetFont();
                const float th   = s_ButtonPx;
                const float hTab = (th + 2.0f) + 8.0f * s_Ui;
                float total = 0.0f;
                for (const auto& t : m_EquipTabs)
                {
                    char nb[16];
                    std::snprintf(nb, sizeof(nb), "%d", t.idx);
                    float w = 2.0f * 8.0f * s_Ui + bf->CalcTextSizeA(th, FLT_MAX, 0.0f, nb).x;
                    if (w < hTab) w = hTab;
                    total += w + (total > 0.0f ? kEqTabSpacing : 0.0f);
                }
                const float avail = ImGui::GetContentRegionAvail().x;
                if (total > 0.0f && total < avail)
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - total) * 0.5f);
            }
            for (size_t i = 0; i < m_EquipTabs.size(); ++i)
            {
                // Numbered like the in-game equipment tabs; the template name
                // (if set) appears as a hover tooltip
                char numBuf[16];
                std::snprintf(numBuf, sizeof(numBuf), "%d", m_EquipTabs[i].idx);
                if (drawEquipTab(m_EquipTabs[i].idx, numBuf, m_EquipTabs[i].name.c_str(),
                                 m_EquipTabs[i].idx == sel, m_EquipTabs[i].matched))
                {
                    sel = m_EquipTabs[i].idx;
                    m_SelEquipTab[charName] = sel;
                }
            }
            ImGui::PopStyleVar();
            if (!m_EquipTabs.empty()) ImGui::Spacing();

            // Rebuild the per-slot equipment lookup when the selected template
            // changed (or the caches above were rebuilt).
            if (m_CharCacheSel != sel)
            {
                // Matches the filter if the gear's own name matches, or if one of its slotted
                // runes/sigils/infusions/enrichments matches — so searching for an equipped
                // upgrade surfaces the item that contains it (not the component itself).
                auto gearMatches = [&](const FoundItem& it) -> bool
                {
                    if (MatchesFilter(it, m_FilterLower.c_str())) return true;
                    for (const auto& eu : it.upgradeSlots)
                        if (Utility::ToLower(eu.name).find(m_FilterLower) != std::string::npos) return true;
                    return false;
                };

                m_EquipBySlot.clear();
                m_EquipExtra.clear();
                for (const auto& it : m_EquipRaw)
                {
                    if (it.characterName != charName) continue;
                    // Tab-less gear (gathering/fishing/jade bot) shows in every template tab
                    if (it.equipTabIdx >= 0 && it.equipTabIdx != sel) continue;
                    if (it.type == "UpgradeComponent") continue; // shown via its parent gear
                    const bool m = searching && gearMatches(it);
                    if (!it.equipSlot.empty() && !m_EquipBySlot.count(it.equipSlot))
                        m_EquipBySlot.emplace(it.equipSlot, std::make_pair(&it, m));
                    else
                        m_EquipExtra.emplace_back(&it, m); // unknown/duplicate slot
                }
                m_CharCacheSel = sel;
            }

            // ── GW2-style equipment panel: armor + weapons left, class plate in
            //    the middle, trinkets right, aquatic/gathering/jade bot below ──
            {
                const float S = s_SlotSize;              // same slot size as the inventory grids
                const float g = std::floor(4.0f * s_Ui); // slot gap
                const float panelW = ImGui::GetContentRegionAvail().x;
                const ImVec2 o = ImGui::GetCursorScreenPos();
                ImDrawList* dl = ImGui::GetWindowDrawList();

                auto slotAt = [&](float x, float y, const char* slotName)
                {
                    ImGui::SetCursorScreenPos(ImVec2(x, y));
                    const FoundItem* it = nullptr; bool m = false;
                    if (auto e = m_EquipBySlot.find(slotName); e != m_EquipBySlot.end())
                    { it = e->second.first; m = e->second.second; }
                    DrawEquipSlot(it, m, searching, S, tooltipItem, tooltipTex);
                };

                // Left column: armor, then both weapon sets
                static const char* kArmorSlots[] = {"Helm","Shoulders","Coat","Gloves","Leggings","Boots"};
                float ly = o.y;
                for (const char* sn : kArmorSlots) { slotAt(o.x, ly, sn); ly += S + g; }
                ly += g * 2.0f;
                slotAt(o.x, ly, "WeaponA1"); ly += S + g;
                slotAt(o.x, ly, "WeaponA2"); ly += S + g;
                ly += g * 2.0f;
                slotAt(o.x, ly, "WeaponB1"); ly += S + g;
                slotAt(o.x, ly, "WeaponB2"); ly += S + g;

                // Right column: back item, trinkets (amulet above the rings,
                // like in-game), relic — the jade bot core lives in the
                // Jade-Bot section below
                static const char* kTrinketSlots[] = {"Backpack","Accessory1","Accessory2","",
                                                      "Amulet","Ring1","Ring2","",
                                                      "Relic"};
                const float rx = o.x + panelW - S;
                float ry = o.y;
                for (const char* sn : kTrinketSlots)
                {
                    if (!sn[0]) { ry += g * 2.0f; continue; }
                    slotAt(rx, ry, sn); ry += S + g;
                }

                // Middle: big class icon + name + level/class/race lines (in
                // place of the in-game 3D character model)
                {
                    const float mx0 = o.x + S + g * 3.0f;
                    const float mw  = (rx - g * 3.0f) - mx0;
                    if (mw > 24.0f)
                    {
                        float my = o.y + g * 2.0f;
                        // Core profession icon (the bundled 200x200 tangos) —
                        // it replaces the profession text line below
                        void* big = GetClassIcon(charProf);
                        if (big)
                        {
                            float isz = std::floor(96.0f * s_Ui);
                            if (isz > mw) isz = mw;
                            const float ix = mx0 + (mw - isz) * 0.5f;
                            dl->AddImage(reinterpret_cast<ImTextureID>(big),
                                         ImVec2(ix, my), ImVec2(ix + isz, my + isz));
                            my += isz + g * 2.0f;
                        }
                        // Name (heading font, shrunk to fit the middle column)
                        ImFont* hf  = s_FontHeading ? s_FontHeading : ImGui::GetFont();
                        float   hpx = s_HeadingPx;
                        float   nw  = hf->CalcTextSizeA(hpx, FLT_MAX, 0.0f, charName.c_str()).x;
                        if (nw > mw && nw > 1.0f) { hpx *= mw / nw; nw = mw; }
                        dl->AddText(hf, hpx, ImVec2(mx0 + (mw - nw) * 0.5f, my),
                                    ImGui::ColorConvertFloat4ToU32(kGold), charName.c_str());
                        my += hpx + g;
                        // Level, class and race on their own centered lines
                        // (unknown parts — e.g. from an old cache — are omitted)
                        int lvl = 0;
                        if (auto il = m_CharLevel.find(charName); il != m_CharLevel.end()) lvl = il->second;
                        std::string race;
                        if (auto ir = m_CharRace.find(charName); ir != m_CharRace.end()) race = ir->second;

                        auto centeredLine = [&](const std::string& text)
                        {
                            if (text.empty()) return;
                            ImFont* bf  = ImGui::GetFont();
                            float   spx = ImGui::GetFontSize();
                            float   sw  = bf->CalcTextSizeA(spx, FLT_MAX, 0.0f, text.c_str()).x;
                            if (sw > mw && sw > 1.0f) { spx *= mw / sw; sw = mw; }
                            dl->AddText(bf, spx, ImVec2(mx0 + (mw - sw) * 0.5f, my),
                                        IM_COL32(205, 205, 205, 255), text.c_str());
                            my += spx + g;
                        };
                        if (lvl > 0)
                            centeredLine(std::string(s.tooltipLevel) + " " + std::to_string(lvl));
                        if (!race.empty())
                            centeredLine(Lang::TranslateRace(race.c_str()));
                    }
                }

                // Bottom sections spanning the panel (only when they have gear),
                // with the same collapsible gold header bars as everywhere else
                const float by = (ly > ry ? ly : ry) + g * 2.0f;
                ImGui::SetCursorScreenPos(ImVec2(o.x, by));
                auto slotRow = [&](const char* caption, std::initializer_list<const char*> slots)
                {
                    bool any = false;
                    for (const char* sn : slots) if (m_EquipBySlot.count(sn)) { any = true; break; }
                    if (!any) return;
                    if (SectionHeader(caption))
                    {
                        bool firstSlot = true;
                        for (const char* sn : slots)
                        {
                            auto e = m_EquipBySlot.find(sn);
                            if (e == m_EquipBySlot.end()) continue; // no empty placeholders down here
                            if (!firstSlot) ImGui::SameLine(0.0f, g);
                            firstSlot = false;
                            DrawEquipSlot(e->second.first, e->second.second, searching, S,
                                          tooltipItem, tooltipTex);
                        }
                    }
                    ImGui::Spacing();
                };
                slotRow(s.secAquatic,   {"HelmAquatic", "WeaponAquaticA", "WeaponAquaticB"});
                slotRow(s.secGathering, {"Sickle", "Axe", "Pick", "FishingRod", "FishingBait", "FishingLure"});
                slotRow(s.secJadebot,   {"PowerCore", "JadeBotCore", "SensoryArray", "ServiceChip"});

                // Gear in slots the fixed layout doesn't know (future API slots)
                if (!m_EquipExtra.empty())
                {
                    const float xMax = o.x + panelW;
                    bool firstE = true;
                    for (const auto& [it2, m2] : m_EquipExtra)
                    {
                        if (!firstE && ImGui::GetItemRectMax().x + g + S <= xMax)
                            ImGui::SameLine(0.0f, g);
                        firstE = false;
                        DrawEquipSlot(it2, m2, searching, S, tooltipItem, tooltipTex);
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            // ── Right: shared inventory (top) + character inventory, as
            //    GW2-style slot grids with used/total slot counts ──
            ImGui::BeginChild("##invcol", ImVec2(0.0f, availY), false);
            ImGui::SetWindowFontScale(m_EffFontScale);
            auto slotCounts = [](const std::vector<std::pair<const FoundItem*, bool>>& v,
                                 char* buf, size_t n)
            {
                int used = 0;
                for (const auto& e : v) if (e.first) ++used;
                if (used < static_cast<int>(v.size()))
                    std::snprintf(buf, n, "%d/%d", used, static_cast<int>(v.size()));
                else
                    buf[0] = '\0'; // no capacity info (old cache) -> no counter
            };
            char cntBuf[32];
            if (!m_CharShared.empty())
            {
                slotCounts(m_CharShared, cntBuf, sizeof(cntBuf));
                if (SectionHeader(s.locSharedInventory, cntBuf))
                    RenderItemGrid("##shared", m_CharShared, searching, tooltipItem, tooltipTex);
                ImGui::Spacing();
            }
            slotCounts(m_CharInv, cntBuf, sizeof(cntBuf));
            if (SectionHeader(s.locInventory, cntBuf))
            {
                if (m_CharInv.empty()) ImGui::TextDisabled("-");
                else RenderItemGrid("##inv", m_CharInv, searching, tooltipItem, tooltipTex);
            }
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

        // Per-role text sizes in px — take effect when a slider is released
        // (the window registers new sizes as dedicated crisp atlas fonts).
        // The item size also drives the overall layout scale. Whole px only —
        // fractional sizes rasterize soft. Independent of the game's UI size.
        auto sizeSlider = [&](const char* label, float& editVal, float PluginConfig::* field,
                              float vmin, float vmax)
        {
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderFloat(label, &editVal, vmin, vmax, "%.0f px"))
                editVal = std::round(editVal);
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                PluginConfig live = GetConfig(state);
                live.*field = editVal;
                SetConfig(state, live);
            }
        };
        sizeSlider(s.optFontScale,   config.FontSize(),    &PluginConfig::fontSize,    10.0f, 28.0f);
        sizeSlider(s.optSizeHeading, config.HeadingSize(), &PluginConfig::headingSize, 10.0f, 32.0f);
        sizeSlider(s.optSizeButton,  config.ButtonSize(),  &PluginConfig::buttonSize,  10.0f, 28.0f);
        sizeSlider(s.optSizeTooltip, config.TooltipSize(), &PluginConfig::tooltipSize, 10.0f, 28.0f);
        ImGui::Spacing();

        ImGui::TextDisabled("%s", s.optKeybindHint);
        ImGui::Spacing();

        // GW2-styled like the main-window buttons (follows the button size too)
        if (Gw2Button(s.optSave))
        {
            config.ApplyFromEditBuffer(state);
            outRequestRefresh = true; // validate key + reload right after saving
        }
    }
}

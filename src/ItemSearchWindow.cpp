#include "ItemSearchWindow.h"
#include "Constants.h"
#include "Lang.h"
#include "Utility.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace LegendaryImpactItemSearch
{
    const char* ItemSearchWindow::GetLocationStr(const FoundItem& item)
    {
        const auto& s = Lang::Get();
        switch (item.locationType)
        {
        case ItemLocation::Bank:            return s.locBank;
        case ItemLocation::SharedInventory: return s.locSharedInventory;
        case ItemLocation::Materials:       return s.locMaterials;
        case ItemLocation::Character:       return item.characterName.c_str();
        default:                            return "-";
        }
    }

    bool ItemSearchWindow::MatchesFilter(const FoundItem& item, const char* filterLower)
    {
        if (!filterLower || filterLower[0] == '\0') return true;
        const std::string nameLower = Utility::ToLower(item.name);
        return nameLower.find(filterLower) != std::string::npos;
    }

    void ItemSearchWindow::Render(AppState& state, bool& outRequestRefresh)
    {
        outRequestRefresh = false;
        const auto& s = Lang::Get();

        if (!state.showWindow.load(std::memory_order_relaxed)) return;

        const std::string title = std::string(s.windowTitle) + Constants::WindowId;

        ImGui::SetNextWindowSize(ImVec2(540, 420), ImGuiCond_FirstUseEver);
        bool isOpen = true;
        if (!ImGui::Begin(title.c_str(), &isOpen, ImGuiWindowFlags_NoCollapse))
        {
            if (!isOpen) state.showWindow.store(false, std::memory_order_relaxed);
            ImGui::End();
            return;
        }
        if (!isOpen) state.showWindow.store(false, std::memory_order_relaxed);

        // Header row: account label + refresh button
        {
            std::string accName;
            std::string fetchErr;
            const bool fetching = state.fetching.load(std::memory_order_relaxed);
            {
                std::shared_lock lk(state.statusLock);
                accName  = state.accountName;
                fetchErr = state.fetchError;
            }

            if (!accName.empty())
            {
                char label[272];
                snprintf(label, sizeof(label), s.accountLabel, accName.c_str());
                ImGui::TextUnformatted(label);
                ImGui::SameLine();
            }

            if (fetching)
            {
                ImGui::BeginDisabled();
                ImGui::Button(s.refreshing);
                ImGui::EndDisabled();
            }
            else
            {
                if (ImGui::Button(s.refreshBtn)) outRequestRefresh = true;
            }

            if (!fetchErr.empty())
            {
                char errBuf[320];
                snprintf(errBuf, sizeof(errBuf), s.fetchError, fetchErr.c_str());
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", errBuf);
            }
        }

        // API key guard
        const PluginConfig cfg = GetConfig(state);
        if (cfg.apiKey[0] == '\0')
        {
            ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.2f, 1.0f), "%s", s.noApiKey);
            ImGui::End();
            return;
        }

        // Search bar — rebuild lower-case filter only when text changes
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputText("##search", m_SearchBuf.data(), m_SearchBuf.size()))
            m_FilterLower = Utility::ToLower(m_SearchBuf.data());
        if (m_SearchBuf[0] == '\0') m_FilterLower.clear();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            ImGui::SetTooltip("%s", s.searchHint);

        // Snapshot items
        std::vector<FoundItem> snapshot;
        {
            std::shared_lock lk(state.itemsLock);
            snapshot = state.items;
        }

        // Filter
        std::vector<const FoundItem*> visible;
        visible.reserve(snapshot.size());
        for (const auto& item : snapshot)
            if (MatchesFilter(item, m_FilterLower.c_str()))
                visible.push_back(&item);

        if (visible.empty())
        {
            ImGui::TextDisabled("%s", s.noResults);
            ImGui::End();
            return;
        }

        // Results table
        constexpr ImGuiTableFlags kTableFlags =
            ImGuiTableFlags_BordersOuter  |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY       |
            ImGuiTableFlags_RowBg         |
            ImGuiTableFlags_Sortable      |
            ImGuiTableFlags_SizingStretchProp;

        if (ImGui::BeginTable("##items", 3, kTableFlags))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn(s.colItem,     ImGuiTableColumnFlags_WidthStretch, 0.50f);
            ImGui::TableSetupColumn(s.colLocation, ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn(s.colCount,    ImGuiTableColumnFlags_WidthStretch, 0.15f);
            ImGui::TableHeadersRow();

            // Sorting
            ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs();
            if (sortSpecs && sortSpecs->SpecsDirty && sortSpecs->SpecsCount > 0)
            {
                const ImGuiTableColumnSortSpecs& spec = sortSpecs->Specs[0];
                const bool asc = spec.SortDirection == ImGuiSortDirection_Ascending;
                std::stable_sort(visible.begin(), visible.end(),
                    [&](const FoundItem* a, const FoundItem* b)
                    {
                        int cmp = 0;
                        switch (spec.ColumnIndex)
                        {
                        case 0: cmp = a->name.compare(b->name); break;
                        case 1: cmp = strcmp(GetLocationStr(*a), GetLocationStr(*b)); break;
                        case 2: cmp = a->count - b->count; break;
                        }
                        return asc ? cmp < 0 : cmp > 0;
                    });
                sortSpecs->SpecsDirty = false;
            }

            for (const FoundItem* item : visible)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(item->name.empty() ? "???" : item->name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(GetLocationStr(*item));
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%d", item->count);
            }
            ImGui::EndTable();
        }

        ImGui::End();
    }

    void ItemSearchWindow::RenderOptions(ConfigStore& config)
    {
        const auto& s = Lang::Get();
        ImGui::SeparatorText(s.optGeneralSection);

        ImGui::Checkbox(s.optShowWindow, &config.ShowWindow());

        ImGui::SetNextItemWidth(400.0f);
        ImGui::InputText(s.optApiKey, config.ApiKeyBuffer(), 256, ImGuiInputTextFlags_Password);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip))
            ImGui::SetTooltip("%s", s.optApiKeyHint);

        ImGui::SliderInt(s.optLanguage, &config.Language(), 0, 1);

        ImGui::TextDisabled("%s", s.optKeybindHint);

        if (ImGui::Button(s.optSave))
            config.ApplyFromEditBuffer();
    }
}

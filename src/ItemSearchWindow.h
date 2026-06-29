#pragma once
#include "ConfigStore.h"
#include "Models.h"
#include "SharedState.h"
#include <array>

namespace LegendaryImpactItemSearch
{
    class ItemSearchWindow
    {
    public:
        void Render(AppState& state, bool& outRequestRefresh);
        void RenderOptions(ConfigStore& config);

    private:
        static const char* GetLocationStr(const FoundItem& item);
        static bool MatchesFilter(const FoundItem& item, const char* filterLower);

        std::array<char, 256> m_SearchBuf{};
        std::string           m_FilterLower;
    };
}

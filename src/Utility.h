#pragma once
#include <string>

namespace LegendaryImpactItemSearch::Utility
{
    std::wstring ToWide(const std::string& value);
    std::string  UrlEncode(const std::string& value);
    std::string  ToLower(std::string s);
}

#include "Utility.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace LegendaryImpactItemSearch::Utility
{
    std::wstring ToWide(const std::string& value)
    {
        if (value.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (size <= 0) return L"";
        std::wstring result(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], size);
        if (!result.empty() && result.back() == L'\0') result.pop_back();
        return result;
    }

    std::string UrlEncode(const std::string& value)
    {
        std::ostringstream oss;
        for (unsigned char c : value)
        {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                oss << c;
            else
                oss << '%' << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(c);
        }
        return oss.str();
    }

    std::string ToLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    }
}

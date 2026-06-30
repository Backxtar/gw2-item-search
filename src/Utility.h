#pragma once
#include <string>

namespace ItemSearch::Utility
{
    std::wstring ToWide(const std::string& value);
    std::string  UrlEncode(const std::string& value);
    std::string  ToLower(std::string s);
    std::string  StripHtml(const std::string& s);
}

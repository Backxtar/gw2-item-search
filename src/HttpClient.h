#pragma once
#include <string>

namespace LegendaryImpactItemSearch
{
    class HttpClient
    {
    public:
        HttpClient();
        ~HttpClient();
        HttpClient(const HttpClient&)            = delete;
        HttpClient& operator=(const HttpClient&) = delete;

        bool Get(const std::string& url, std::string& response, std::string& error) const;

    private:
        std::string ReadResponse(void* requestHandle) const;
        void* m_Session = nullptr;
    };
}

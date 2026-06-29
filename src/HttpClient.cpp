#include "HttpClient.h"
#include "Utility.h"
#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

namespace LegendaryImpactItemSearch
{
    HttpClient::HttpClient()
    {
        m_Session = WinHttpOpen(
            L"LegendaryImpactItemSearch/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        if (m_Session)
            WinHttpSetTimeouts(static_cast<HINTERNET>(m_Session), 5000, 5000, 5000, 15000);
    }

    HttpClient::~HttpClient()
    {
        if (m_Session)
            WinHttpCloseHandle(static_cast<HINTERNET>(m_Session));
    }

    std::string HttpClient::ReadResponse(void* requestHandle) const
    {
        auto request = static_cast<HINTERNET>(requestHandle);
        std::string body;
        body.reserve(65536);
        DWORD available = 0;

        while (WinHttpQueryDataAvailable(request, &available) && available > 0)
        {
            const size_t offset = body.size();
            body.resize(offset + available);
            DWORD downloaded = 0;
            if (!WinHttpReadData(request, body.data() + offset, available, &downloaded))
            {
                body.resize(offset);
                break;
            }
            body.resize(offset + downloaded);
        }
        return body;
    }

    bool HttpClient::Get(const std::string& url, std::string& response, std::string& error) const
    {
        response.clear();
        error.clear();

        if (!m_Session) { error = "WinHTTP session unavailable."; return false; }

        std::wstring wideUrl = Utility::ToWide(url);
        if (wideUrl.empty()) { error = "Invalid URL."; return false; }

        URL_COMPONENTS components = {};
        components.dwStructSize = sizeof(components);
        wchar_t host[256]  = {};
        wchar_t path[4096] = {};
        components.lpszHostName      = host;
        components.dwHostNameLength  = ARRAYSIZE(host);
        components.lpszUrlPath       = path;
        components.dwUrlPathLength   = ARRAYSIZE(path);

        if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components))
        {
            error = "Failed to parse URL.";
            return false;
        }

        const bool isHttps = components.nScheme == INTERNET_SCHEME_HTTPS;

        HINTERNET connection = WinHttpConnect(
            static_cast<HINTERNET>(m_Session),
            components.lpszHostName,
            components.nPort, 0);

        if (!connection) { error = "WinHttpConnect failed."; return false; }

        HINTERNET request = WinHttpOpenRequest(
            connection, L"GET", components.lpszUrlPath,
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            isHttps ? WINHTTP_FLAG_SECURE : 0);

        if (!request)
        {
            WinHttpCloseHandle(connection);
            error = "WinHttpOpenRequest failed.";
            return false;
        }

        const BOOL sent = WinHttpSendRequest(
            request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

        if (!sent || !WinHttpReceiveResponse(request, nullptr))
        {
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            error = "HTTP request failed.";
            return false;
        }

        DWORD statusCode = 0;
        DWORD statusSize = sizeof(statusCode);
        WinHttpQueryHeaders(request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode, &statusSize,
            WINHTTP_NO_HEADER_INDEX);

        response = ReadResponse(request);

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);

        if (statusCode < 200 || statusCode >= 300)
        {
            error = "HTTP " + std::to_string(statusCode);
            return false;
        }
        return true;
    }
}

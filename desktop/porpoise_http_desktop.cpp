/* Porpoise on Windows: covers, game info and update checks over HTTPS with
 * WinHTTP, the system's own (its certificates, its proxy settings), behind
 * the same Session the PS5 build makes with libSceHttp.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "porpoise_http.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <string>

#include "trace.hpp"

namespace porpoise::http
{
namespace
{
HINTERNET g_session = nullptr;

std::wstring widen(const std::string &s)
{
    if (s.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
    std::wstring w(std::size_t(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
    return w;
}
} // namespace

bool Session::init()
{
    if (!g_session)
        g_session = WinHttpOpen(L"Porpoise", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS, 0);
    if (!g_session)
    {
        ps5::debug::mark_value("http: WinHttpOpen failed", static_cast<long long>(GetLastError()));
        return false;
    }
    WinHttpSetTimeouts(g_session, 10000, 10000, 15000, 30000);
    pool_ = 1;
    return true;
}

void Session::term()
{
    pool_ = -1; /* the WinHTTP session is shared and stays */
}

int Session::get(const std::string &url, std::vector<std::uint8_t> &out,
                 const std::function<bool(std::size_t)> &progress, std::size_t limit)
{
    out.clear();
    if (pool_ < 0 && !init())
        return -1;
    const std::wstring wurl = widen(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof parts;
    wchar_t host[256] = {}, path[2048] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;
    wchar_t extra[2048] = {};
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts))
        return -1;
    const std::wstring object = std::wstring(path) + extra;
    HINTERNET connection = WinHttpConnect(g_session, host, parts.nPort, 0);
    if (!connection)
        return -1;
    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    int status = -1;
    if (request && WinHttpSendRequest(request, L"Accept: */*\r\n", DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(request, nullptr))
    {
        DWORD code = 0, size = sizeof code;
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &code, &size, WINHTTP_NO_HEADER_INDEX);
        status = int(code);
        if (status == 200)
            for (;;)
            {
                DWORD available = 0;
                if (!WinHttpQueryDataAvailable(request, &available))
                {
                    status = -1;
                    break;
                }
                if (available == 0)
                    break;
                const std::size_t at = out.size();
                if (at + available > limit)
                {
                    status = -1;
                    break;
                }
                out.resize(at + available);
                DWORD read = 0;
                if (!WinHttpReadData(request, out.data() + at, available, &read))
                {
                    status = -1;
                    break;
                }
                out.resize(at + read);
                if (progress && !progress(out.size()))
                {
                    status = -1;
                    break;
                }
            }
    }
    if (request)
        WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    if (status != 200 && status != -1)
        out.clear();
    return status;
}
/* RetroAchievements (the only POST) is PS5-only for now. */
int Session::request(const std::string &url, const std::string *form, std::vector<std::uint8_t> &out,
                     std::size_t limit)
{
    if (form)
    {
        out.clear();
        return -1;
    }
    return get(url, out, {}, limit);
}
} // namespace porpoise::http

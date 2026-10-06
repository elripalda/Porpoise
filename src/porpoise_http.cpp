/* Porpoise - HTTPS through the console's own libSceNet / libSceSsl / libSceHttp2.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * The way the payload SDK's http2_get sample and PS5SX2's cover fetcher
 * (ps5/frontend/fe_ps5.cpp) use these libraries. */
#include "porpoise_http.hpp"

#include <cstdio>
#include <pthread.h>

#include "trace.hpp"

extern "C"
{
    int sceNetInit(void);
    int sceNetPoolCreate(const char *name, int size, int flags);
    int sceNetPoolDestroy(int pool);
    int sceSslInit(std::size_t pool_size);
    int sceSslTerm(int ctx);
    int sceHttp2Init(int net_pool, int ssl_ctx, std::size_t pool_size, int max_requests);
    int sceHttp2Term(int ctx);
    int sceHttp2CreateTemplate(int ctx, const char *user_agent, int http_version, int auto_proxy);
    int sceHttp2DeleteTemplate(int tmpl);
    int sceHttp2CreateRequestWithURL(int tmpl, const char *method, const char *url, std::uint64_t content_length);
    int sceHttp2DeleteRequest(int req);
    int sceHttp2SendRequest(int req, const void *data, std::size_t size);
    int sceHttp2GetStatusCode(int req, int *status);
    int sceHttp2ReadData(int req, void *data, std::size_t size);
    int sceHttp2SetResolveTimeOut(int id, std::uint32_t usec);
    int sceHttp2SetConnectTimeOut(int id, std::uint32_t usec);
    int sceHttp2SetSendTimeOut(int id, std::uint32_t usec);
    int sceHttp2SetRecvTimeOut(int id, std::uint32_t usec);
    int sceHttp2SetTimeOut(int id, std::uint32_t usec);
    int sceHttp2SetAutoRedirect(int id, int enable);
    int sceHttp2AddRequestHeader(int id, const char *name, const char *value, std::uint32_t mode);
    int sceNetCtlInit(void);
    void sceNetCtlTerm(void);
    int sceNetCtlGetState(int *state);
}

namespace porpoise::http
{
namespace
{
pthread_mutex_t g_one = PTHREAD_MUTEX_INITIALIZER; /* one session at a time */
}

bool Session::init()
{
    pthread_mutex_lock(&g_one);
    locked_ = true;
    const int nc = sceNetCtlInit();
    netctl_ = nc == 0;
    int state[4] = {-1, 0, 0, 0};
    const int gs = sceNetCtlGetState(state);
    if (gs == 0 && state[0] >= 0 && state[0] < 3)
    {
        ps5::debug::mark((std::string(name_) + ": the console is not online").c_str());
        return false;
    }
    (void)sceNetInit(); /* an error only means it is up already */
    pool_ = sceNetPoolCreate(name_, 64 * 1024, 0);
    ssl_ = pool_ >= 0 ? sceSslInit(256 * 1024) : -1;
    ctx_ = ssl_ >= 0 ? sceHttp2Init(pool_, ssl_, 256 * 1024, 1) : -1;
    tmpl_ = ctx_ >= 0 ? sceHttp2CreateTemplate(ctx_, agent_, 3, 1) : -1;
    char line[160];
    std::snprintf(line, sizeof line, "%s: https pool %#x ssl %#x http2 %#x template %#x", name_, unsigned(pool_),
                  unsigned(ssl_), unsigned(ctx_), unsigned(tmpl_));
    ps5::debug::mark(line);
    return tmpl_ >= 0;
}

void Session::term()
{
    if (tmpl_ >= 0)
        sceHttp2DeleteTemplate(tmpl_);
    if (ctx_ >= 0)
        sceHttp2Term(ctx_);
    if (ssl_ >= 0)
        sceSslTerm(ssl_);
    if (pool_ >= 0)
        sceNetPoolDestroy(pool_);
    if (netctl_)
        sceNetCtlTerm();
    tmpl_ = ctx_ = ssl_ = pool_ = -1;
    netctl_ = false;
    if (locked_)
    {
        locked_ = false;
        pthread_mutex_unlock(&g_one);
    }
}

int Session::get(const std::string &url, std::vector<std::uint8_t> &out,
                 const std::function<bool(std::size_t)> &progress, std::size_t limit)
{
    out.clear();
    if (tmpl_ < 0)
        return -1;
    const int req = sceHttp2CreateRequestWithURL(tmpl_, "GET", url.c_str(), 0);
    if (req < 0)
        return -1;
    sceHttp2SetResolveTimeOut(req, 10 * 1000 * 1000);
    sceHttp2SetConnectTimeOut(req, 10 * 1000 * 1000);
    sceHttp2SetSendTimeOut(req, 10 * 1000 * 1000);
    sceHttp2SetRecvTimeOut(req, 20 * 1000 * 1000);
    sceHttp2SetTimeOut(req, progress ? 600 * 1000 * 1000 : 25 * 1000 * 1000);
    sceHttp2SetAutoRedirect(req, 1);
    int status = -1;
    if (sceHttp2SendRequest(req, nullptr, 0) != 0 || sceHttp2GetStatusCode(req, &status) != 0)
        status = -1;
    else if (status == 200)
    {
        std::vector<std::uint8_t> buf(64 * 1024);
        for (;;)
        {
            const int n = sceHttp2ReadData(req, buf.data(), buf.size());
            if (n < 0)
            {
                status = -1;
                break;
            }
            if (n == 0)
                break;
            out.insert(out.end(), buf.begin(), buf.begin() + n);
            if (out.size() > limit || (progress && !progress(out.size())))
            {
                status = -1;
                break;
            }
        }
    }
    sceHttp2DeleteRequest(req);
    return status;
}
int Session::request(const std::string &url, const std::string *form, std::vector<std::uint8_t> &out,
                     std::size_t limit)
{
    out.clear();
    if (tmpl_ < 0)
        return -1;
    const int req = sceHttp2CreateRequestWithURL(tmpl_, form ? "POST" : "GET", url.c_str(),
                                                 form ? std::uint64_t(form->size()) : 0);
    if (req < 0)
        return -1;
    sceHttp2SetResolveTimeOut(req, 10 * 1000 * 1000);
    sceHttp2SetConnectTimeOut(req, 10 * 1000 * 1000);
    sceHttp2SetSendTimeOut(req, 10 * 1000 * 1000);
    sceHttp2SetRecvTimeOut(req, 20 * 1000 * 1000);
    sceHttp2SetTimeOut(req, 30 * 1000 * 1000);
    sceHttp2SetAutoRedirect(req, 1);
    if (form)
        sceHttp2AddRequestHeader(req, "Content-Type", "application/x-www-form-urlencoded", 0 /* overwrite */);
    int status = -1;
    if (sceHttp2SendRequest(req, form ? form->data() : nullptr, form ? form->size() : 0) != 0 ||
        sceHttp2GetStatusCode(req, &status) != 0)
        status = -1;
    else
    {
        std::vector<std::uint8_t> buf(16 * 1024);
        for (;;)
        {
            const int n = sceHttp2ReadData(req, buf.data(), buf.size());
            if (n < 0)
            {
                status = -1;
                break;
            }
            if (n == 0)
                break;
            out.insert(out.end(), buf.begin(), buf.begin() + n);
            if (out.size() > limit)
            {
                status = -1;
                break;
            }
        }
    }
    sceHttp2DeleteRequest(req);
    return status;
}
} // namespace porpoise::http

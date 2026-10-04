/* Porpoise - box art from GameTDB, downloaded in the background.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * HTTPS goes through the console's own libSceNet / libSceSsl / libSceHttp2,
 * the way the payload SDK's http2_get sample and PS5SX2's cover fetcher
 * (ps5/frontend/fe_ps5.cpp) use them: one template, one request at a time,
 * every phase with a timeout, and no request at all when the console has no
 * network.
 *
 * The art is GameTDB's (https://www.gametdb.com), the same source Dolphin and
 * USB Loader GX use. The full high-resolution box (back, spine, front) is
 * fetched first and its front - the right-hand 135:190 of it - is kept; the
 * small front-only cover is the fallback. A disc with no art on GameTDB is
 * remembered for a few days so it is not asked for at every start. */
#include "porpoise_covers.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <deque>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "stb/stb_image_write.h"
#pragma clang diagnostic pop

#include "porpoise_gametdb.hpp"
#include "title_threads.hpp"
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
    int sceNetCtlInit(void);
    void sceNetCtlTerm(void);
    int sceNetCtlGetState(int *state);
}

namespace porpoise::covers
{
namespace
{
constexpr long long kMissRetrySeconds = 3LL * 24 * 60 * 60;

/* One thing to fetch. */
struct Job
{
    enum Kind
    {
        Cover,
        Info,
        Disc,
        Back,
        Feed,
        Release,
    } kind;
    std::string id;
};

struct Worker
{
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_t thread{};
    bool running = false;
    std::atomic<bool> stopping{false};
    std::string dir, info_path, info_lang, feed_path, release_path;
    std::vector<Job> jobs;
    std::atomic<int> phase{0};
    std::atomic<bool> info_ready{false};
    std::atomic<bool> feed_ready{false}, release_ready{false};
    std::deque<std::string> ready; /* guarded by mutex */
    std::atomic<int> done{0}, total{0};
    std::atomic<bool> active{false};
};
Worker g;

void log(const std::string &line)
{
    ps5::debug::mark(("covers: " + line).c_str());
}

/* ---- HTTPS -------------------------------------------------------------------------------- */

struct Http
{
    int pool = -1, ssl = -1, ctx = -1, tmpl = -1;
    bool netctl = false;

    bool init()
    {
        const int nc = sceNetCtlInit();
        netctl = nc == 0;
        int state[4] = {-1, 0, 0, 0};
        const int gs = sceNetCtlGetState(state);
        if (gs == 0 && state[0] >= 0 && state[0] < 3)
        {
            log("the console is not online; no covers this time");
            return false;
        }
        (void)sceNetInit(); /* an error only means it is up already */
        pool = sceNetPoolCreate("porpoise-covers", 64 * 1024, 0);
        ssl = pool >= 0 ? sceSslInit(256 * 1024) : -1;
        ctx = ssl >= 0 ? sceHttp2Init(pool, ssl, 256 * 1024, 1) : -1;
        tmpl = ctx >= 0 ? sceHttp2CreateTemplate(ctx, "Porpoise/1.0", 3, 1) : -1;
        char line[160];
        std::snprintf(line, sizeof line, "https: pool %#x ssl %#x http2 %#x template %#x", unsigned(pool),
                      unsigned(ssl), unsigned(ctx), unsigned(tmpl));
        log(line);
        return tmpl >= 0;
    }

    void term()
    {
        if (tmpl >= 0)
            sceHttp2DeleteTemplate(tmpl);
        if (ctx >= 0)
            sceHttp2Term(ctx);
        if (ssl >= 0)
            sceSslTerm(ssl);
        if (pool >= 0)
            sceNetPoolDestroy(pool);
        if (netctl)
            sceNetCtlTerm();
        tmpl = ctx = ssl = pool = -1;
        netctl = false;
    }

    /* The HTTP status, or -1 when the request could not be made. */
    int get(const std::string &url, std::vector<std::uint8_t> &out)
    {
        out.clear();
        const int req = sceHttp2CreateRequestWithURL(tmpl, "GET", url.c_str(), 0);
        if (req < 0)
            return -1;
        sceHttp2SetResolveTimeOut(req, 10 * 1000 * 1000);
        sceHttp2SetConnectTimeOut(req, 10 * 1000 * 1000);
        sceHttp2SetSendTimeOut(req, 10 * 1000 * 1000);
        sceHttp2SetRecvTimeOut(req, 10 * 1000 * 1000);
        sceHttp2SetTimeOut(req, 25 * 1000 * 1000);
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
                if (out.size() > (64u << 20)) /* the game database is the largest download */
                {
                    status = -1;
                    break;
                }
            }
        }
        sceHttp2DeleteRequest(req);
        return status;
    }
};

/* ---- covers ------------------------------------------------------------------------------- */

bool exists(const std::string &path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0 && st.st_size > 0;
}

/* GameTDB's language folders to try for a disc, best first (as USB Loader GX
 * orders them): the disc's own region, then English, then the US set. */
std::vector<std::string> regions_for(const std::string &id)
{
    std::vector<std::string> r;
    const char c = id.size() > 3 ? id[3] : 'E';
    switch (c)
    {
    case 'E': case 'N': r = {"US"}; break;
    case 'J': r = {"JA"}; break;
    case 'K': case 'Q': case 'T': r = {"KO"}; break;
    case 'W': r = {"ZH"}; break;
    case 'D': r = {"DE"}; break;
    case 'F': r = {"FR"}; break;
    case 'H': r = {"NL"}; break;
    case 'I': r = {"IT"}; break;
    case 'R': r = {"RU"}; break;
    case 'S': r = {"ES"}; break;
    case 'U': r = {"AU"}; break;
    case 'V': r = {"DK", "SE", "FI"}; break;
    default: break;
    }
    for (const char *fallback : {"EN", "US", "JA"})
        if (std::find(r.begin(), r.end(), fallback) == r.end())
            r.push_back(fallback);
    return r;
}

bool is_image(const std::vector<std::uint8_t> &d)
{
    return (d.size() > 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') ||
           (d.size() > 3 && d[0] == 0xFF && d[1] == 0xD8);
}

/* Keeps the front of a full box: its right-hand side, 135 wide for 190 high
 * (a GameCube or Wii case). Writes a PNG. */
/* A full box is back | spine | front. Its front is the right-hand side and
 * its back the left-hand side, each 135 wide for 190 high (a GameCube or Wii
 * case). front_path / back_path: "" to skip. Returns whether what was asked
 * for was written. */
bool save_box(const std::vector<std::uint8_t> &data, const std::string &front_path, const std::string &back_path)
{
    int w = 0, h = 0, n = 0;
    unsigned char *px = stbi_load_from_memory(data.data(), int(data.size()), &w, &h, &n, 4);
    if (!px)
        return false;
    const bool front_only = w < h;
    const int fw = front_only ? w : std::min(w, int(h * 135.0 / 190.0 + 0.5));
    auto write = [&](int x0, const std::string &path) {
        std::vector<unsigned char> part(std::size_t(fw) * std::size_t(h) * 4);
        for (int y = 0; y < h; ++y)
            std::copy(px + (std::size_t(y) * w + x0) * 4, px + (std::size_t(y) * w + x0 + fw) * 4,
                      part.begin() + std::ptrdiff_t(std::size_t(y) * fw * 4));
        const std::string tmp = path + ".part";
        return stbi_write_png(tmp.c_str(), fw, h, 4, part.data(), fw * 4) && std::rename(tmp.c_str(), path.c_str()) == 0;
    };
    bool ok = true;
    if (!front_path.empty())
        ok &= write(w - fw, front_path);
    if (!back_path.empty())
        ok &= !front_only && write(0, back_path);
    stbi_image_free(px);
    return ok;
}

bool save_raw(const std::vector<std::uint8_t> &data, const std::string &path)
{
    const std::string tmp = path + ".part";
    std::FILE *f = std::fopen(tmp.c_str(), "wb");
    if (!f)
        return false;
    const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
    std::fclose(f);
    return ok && std::rename(tmp.c_str(), path.c_str()) == 0;
}

/* ID -> unix time of the last miss, from <covers>/missing.txt. */
std::vector<std::pair<std::string, long long>> read_misses(const std::string &file)
{
    std::vector<std::pair<std::string, long long>> out;
    if (std::FILE *f = std::fopen(file.c_str(), "r"))
    {
        char id[16];
        long long t = 0;
        while (std::fscanf(f, "%15s %lld", id, &t) == 2)
            out.push_back({id, t});
        std::fclose(f);
    }
    return out;
}

void write_misses(const std::string &file, const std::vector<std::pair<std::string, long long>> &misses)
{
    if (std::FILE *f = std::fopen(file.c_str(), "w"))
    {
        for (const auto &m : misses)
            std::fprintf(f, "%s %lld\n", m.first.c_str(), m.second);
        std::fclose(f);
    }
}

/* Fetches one disc ID's art. 1 saved, 0 GameTDB has none, -1 the network failed. */
int fetch_art(Http &http, const Job &job)
{
    std::vector<std::uint8_t> data;
    if (job.kind == Job::Cover)
    {
        const std::string png = g.dir + "/" + job.id + ".png";
        for (const std::string &region : regions_for(job.id))
        {
            if (g.stopping.load())
                return -1;
            const int status =
                http.get("https://art.gametdb.com/wii/coverfullHQ/" + region + "/" + job.id + ".png", data);
            if (status < 0)
                return -1;
            if (status == 200 && is_image(data) && save_box(data, png, ""))
            {
                save_box(data, "", g.dir + "/" + job.id + ".back.png"); /* the back too, when it has one */
                log(job.id + ": full box (" + region + ")");
                return 1;
            }
        }
        for (const std::string &region : regions_for(job.id))
        {
            if (g.stopping.load())
                return -1;
            const int status = http.get("https://art.gametdb.com/wii/cover/" + region + "/" + job.id + ".png", data);
            if (status < 0)
                return -1;
            if (status == 200 && is_image(data) && save_raw(data, png))
            {
                log(job.id + ": front cover (" + region + ")");
                return 1;
            }
        }
        return 0;
    }
    if (job.kind == Job::Back)
    {
        /* Only the full box has a back. */
        for (const std::string &region : regions_for(job.id))
        {
            if (g.stopping.load())
                return -1;
            const int status =
                http.get("https://art.gametdb.com/wii/coverfullHQ/" + region + "/" + job.id + ".png", data);
            if (status < 0)
                return -1;
            if (status == 200 && is_image(data) && save_box(data, "", g.dir + "/" + job.id + ".back.png"))
            {
                log(job.id + ": back of the box (" + region + ")");
                return 1;
            }
        }
        return 0;
    }
    /* The disc's label art. */
    const std::string png = g.dir + "/" + job.id + ".disc.png";
    for (const std::string &region : regions_for(job.id))
    {
        if (g.stopping.load())
            return -1;
        const int status = http.get("https://art.gametdb.com/wii/disc/" + region + "/" + job.id + ".png", data);
        if (status < 0)
            return -1;
        if (status == 200 && is_image(data) && save_raw(data, png))
        {
            log(job.id + ": disc (" + region + ")");
            return 1;
        }
    }
    return 0;
}

void *run(void *)
{
    Http http;
    const std::string miss_file = g.dir + "/missing.txt";
    auto misses = read_misses(miss_file);
    const long long now = (long long)std::time(nullptr);
    bool online = false, tried = false;
    int network_failures = 0;
    for (std::size_t j = 0; j < g.jobs.size(); ++j)
    {
        const Job &job = g.jobs[j];
        g.phase = job.kind;
        if (g.stopping.load())
            break;
        const std::string miss_key =
            (job.kind == Job::Disc ? "disc:" : job.kind == Job::Back ? "back:" : "") + job.id;
        bool recent_miss = false;
        for (const auto &m : misses)
            recent_miss |= m.first == miss_key && now - m.second < kMissRetrySeconds;
        if (recent_miss)
        {
            ++g.done;
            continue;
        }
        if (!tried)
        {
            tried = true;
            online = http.init();
        }
        if (!online)
            break;

        if (job.kind == Job::Info)
        {
            std::vector<std::uint8_t> zip;
            const int status = http.get(gametdb::database_url(g.info_lang), zip);
            std::string error;
            const int n = status == 200 ? gametdb::zip_to_table(zip, g.info_path, error, g.info_lang) : -1;
            if (n > 0)
            {
                log("game info: " + std::to_string(n) + " games");
                g.info_ready = true;
            }
            else
                log("game info: " + (status == 200 ? error : "download failed, status " + std::to_string(status)));
            ++g.done;
            continue;
        }

        if (job.kind == Job::Feed || job.kind == Job::Release)
        {
            std::vector<std::uint8_t> data;
            const bool feed = job.kind == Job::Feed;
            const int status = http.get(feed ? kFeedUrl : kReleaseUrl, data);
            std::string text(data.begin(), data.end());
            if (!feed && status == 200)
            {
                /* Only the tag and the page from GitHub's answer. */
                auto field = [&](const char *name) {
                    const std::string key = std::string("\"") + name + "\"";
                    std::size_t at = text.find(key);
                    if (at == std::string::npos)
                        return std::string();
                    at = text.find('"', text.find(':', at + key.size()) + 1);
                    const std::size_t end = at == std::string::npos ? at : text.find('"', at + 1);
                    return end == std::string::npos ? std::string() : text.substr(at + 1, end - at - 1);
                };
                text = field("tag_name") + "\n" + field("html_url") + "\n";
            }
            const std::string path = feed ? g.feed_path : g.release_path;
            if (status == 200 && text.size() > 2 && text.size() < (1u << 20))
            {
                const std::string tmp = path + ".part";
                if (std::FILE *f = std::fopen(tmp.c_str(), "wb"))
                {
                    std::fwrite(text.data(), 1, text.size(), f);
                    std::fclose(f);
                    if (std::rename(tmp.c_str(), path.c_str()) == 0)
                        (feed ? g.feed_ready : g.release_ready) = true;
                }
                log(std::string(feed ? "recommended settings" : "newest release") + ": updated");
            }
            else
                log(std::string(feed ? "recommended settings" : "newest release") + ": status " +
                    std::to_string(status));
            ++g.done;
            continue;
        }

        const int got = fetch_art(http, job);
        if (got > 0)
        {
            network_failures = 0;
            pthread_mutex_lock(&g.mutex);
            g.ready.push_back(job.id);
            pthread_mutex_unlock(&g.mutex);
        }
        else if (got < 0)
        {
            if (g.stopping.load())
                break;
            log(job.id + ": the request failed");
            if (++network_failures >= 2)
            {
                log("giving up until next start");
                break;
            }
        }
        else
        {
            log(miss_key + ": no art on GameTDB");
            misses.erase(std::remove_if(misses.begin(), misses.end(),
                                        [&](const auto &m) { return m.first == miss_key; }),
                         misses.end());
            misses.push_back({miss_key, now});
            write_misses(miss_file, misses);
        }
        ++g.done;
    }
    if (tried)
        http.term();
    g.active = false;
    return nullptr;
}
} // namespace

void start(const Request &request)
{
    if (g.active.load())
        return;
    if (g.running)
    {
        pthread_join(g.thread, nullptr); /* the last run has finished */
        g.running = false;
    }
    g.stopping = false;
    g.dir = request.dir;
    g.info_path = request.info_path;
    g.info_lang = request.info_lang;
    g.feed_path = request.feed_path;
    g.release_path = request.release_path;
    g.jobs.clear();
    /* The small daily ones first. */
    auto stale = [](const std::string &path) {
        struct stat st;
        return stat(path.c_str(), &st) != 0 || (long long)std::time(nullptr) - (long long)st.st_mtime > 20LL * 60 * 60;
    };
    if (!request.feed_path.empty() && stale(request.feed_path))
        g.jobs.push_back({Job::Feed, ""});
    if (!request.release_path.empty() && stale(request.release_path))
        g.jobs.push_back({Job::Release, ""});
    std::vector<std::string> ids;
    for (const std::string &id : request.ids)
        if (id.size() == 6 && std::find(ids.begin(), ids.end(), id) == ids.end())
            ids.push_back(id);
    if (request.covers)
        for (const std::string &id : ids)
            if (!exists(g.dir + "/" + id + ".png") && !exists(g.dir + "/" + id + ".jpg"))
                g.jobs.push_back({Job::Cover, id});
    if (!request.info_path.empty())
    {
        /* Once, then again after a month for new entries. */
        struct stat st;
        const bool fresh = stat(request.info_path.c_str(), &st) == 0 &&
                           (long long)std::time(nullptr) - (long long)st.st_mtime < 30LL * 24 * 60 * 60;
        if (!fresh && !ids.empty())
            g.jobs.push_back({Job::Info, ""});
    }
    if (request.discs)
        for (const std::string &id : ids)
            if (!exists(g.dir + "/" + id + ".disc.png"))
                g.jobs.push_back({Job::Disc, id});
    /* Backs of boxes whose fronts are already here (new covers bring theirs). */
    if (request.covers)
        for (const std::string &id : ids)
            if (exists(g.dir + "/" + id + ".png") && !exists(g.dir + "/" + id + ".back.png"))
                g.jobs.push_back({Job::Back, id});
    if (g.jobs.empty())
        return;
    mkdir(g.dir.c_str(), 0777);
    g.total = int(g.jobs.size());
    g.done = 0;
    g.active = true;
    g.running = create_title_thread(&g.thread, run, nullptr) == 0;
    if (!g.running)
        g.active = false;
    log("fetching " + std::to_string(g.jobs.size()) + " items");
}

bool take_ready(std::string &id)
{
    pthread_mutex_lock(&g.mutex);
    const bool any = !g.ready.empty();
    if (any)
    {
        id = g.ready.front();
        g.ready.pop_front();
    }
    pthread_mutex_unlock(&g.mutex);
    return any;
}

bool take_info_ready()
{
    return g.info_ready.exchange(false);
}

bool take_feed_ready()
{
    return g.feed_ready.exchange(false);
}

bool take_release_ready()
{
    return g.release_ready.exchange(false);
}

std::string status()
{
    if (!g.active.load())
        return "";
    const int phase = g.phase.load();
    if (phase == Job::Info)
        return "Getting game info";
    if (phase == Job::Feed || phase == Job::Release)
        return "";
    return std::string(phase == Job::Disc ? "Getting disc art " : "Getting covers ") +
           std::to_string(std::min(g.done.load() + 1, g.total.load())) + " of " + std::to_string(g.total.load());
}

bool progress(int &phase, int &done, int &total)
{
    if (!g.active.load())
        return false;
    phase = g.phase.load();
    done = std::min(g.done.load() + 1, g.total.load());
    total = g.total.load();
    return true;
}

void stop()
{
    g.stopping = true;
}
} // namespace porpoise::covers

/* Porpoise - updating itself from its GitHub releases.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * PS5SX2's installer payload (ps5/installer in PS5SX2) showed the shape of
 * it: GitHub's size and SHA-256 for the zip, files replaced one by one with
 * eboot.bin last. This does the same from inside the app. */
#include "porpoise_update.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "porpoise_http.hpp"
#include "title_threads.hpp"
#include "trace.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb/stb_image.h"
#pragma clang diagnostic pop

namespace porpoise::update
{
namespace
{
/* ---- SHA-256 (FIPS 180-4) ------------------------------------------------------------- */

struct Sha256
{
    std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    std::uint8_t block[64];
    std::size_t used = 0;
    std::uint64_t bits = 0;

    static std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

    void compress(const std::uint8_t *p)
    {
        static const std::uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i)
            w[i] = std::uint32_t(p[i * 4]) << 24 | std::uint32_t(p[i * 4 + 1]) << 16 | std::uint32_t(p[i * 4 + 2]) << 8 |
                   std::uint32_t(p[i * 4 + 3]);
        for (int i = 16; i < 64; ++i)
        {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i)
        {
            const std::uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    void add(const std::uint8_t *p, std::size_t n)
    {
        bits += std::uint64_t(n) * 8;
        while (n)
        {
            const std::size_t take = std::min(n, sizeof block - used);
            std::memcpy(block + used, p, take);
            used += take;
            p += take;
            n -= take;
            if (used == sizeof block)
            {
                compress(block);
                used = 0;
            }
        }
    }

    std::string hex()
    {
        const std::uint64_t length = bits;
        const std::uint8_t one = 0x80, zero = 0;
        add(&one, 1);
        while (used != 56)
            add(&zero, 1);
        std::uint8_t len[8];
        for (int i = 0; i < 8; ++i)
            len[i] = std::uint8_t(length >> (56 - i * 8));
        add(len, 8);
        char out[65];
        for (int i = 0; i < 8; ++i)
            std::snprintf(out + i * 8, 9, "%08x", h[i]);
        return std::string(out, 64);
    }
};

std::string sha256(const std::uint8_t *p, std::size_t n)
{
    Sha256 s;
    s.add(p, n);
    return s.hex();
}

/* ---- state ------------------------------------------------------------------------------ */

pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
Progress g_progress;
std::atomic<bool> g_busy{false};
Release g_release;
std::string g_app, g_cache;

void set(Phase phase, std::size_t done = 0, std::size_t total = 0, const std::string &error = "")
{
    pthread_mutex_lock(&g_mutex);
    g_progress.phase = phase;
    g_progress.done = done;
    g_progress.total = total;
    g_progress.error = error;
    pthread_mutex_unlock(&g_mutex);
    if (!error.empty())
        ps5::debug::mark(("update: " + error).c_str());
}

std::string field(const std::string &text, std::size_t from, std::size_t to, const char *name)
{
    const std::string key = std::string("\"") + name + "\"";
    const std::size_t at = text.find(key, from);
    if (at == std::string::npos || at >= to)
        return "";
    std::size_t q = text.find(':', at + key.size());
    if (q == std::string::npos)
        return "";
    ++q;
    while (q < text.size() && (text[q] == ' ' || text[q] == '\n' || text[q] == '\r' || text[q] == '\t'))
        ++q;
    if (q >= text.size())
        return "";
    if (text[q] == '"')
    {
        const std::size_t end = text.find('"', q + 1);
        return end == std::string::npos ? "" : text.substr(q + 1, end - q - 1);
    }
    std::size_t end = q;
    while (end < text.size() && text[end] != ',' && text[end] != '}' && text[end] != '\n')
        ++end;
    return text.substr(q, end - q); /* a number, or null */
}

std::uint32_t le32(const std::uint8_t *p)
{
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
std::uint16_t le16(const std::uint8_t *p)
{
    return std::uint16_t(p[0] | p[1] << 8);
}

struct Entry
{
    std::string name;
    std::uint16_t method;
    std::uint32_t csize, usize, local;
};

bool list_zip(const std::vector<std::uint8_t> &zip, std::vector<Entry> &out)
{
    if (zip.size() < 22)
        return false;
    std::size_t eocd = std::string::npos;
    for (std::size_t i = zip.size() - 22 + 1; i-- > 0 && zip.size() - i < 70000;)
        if (le32(&zip[i]) == 0x06054b50)
        {
            eocd = i;
            break;
        }
    if (eocd == std::string::npos)
        return false;
    const unsigned entries = le16(&zip[eocd + 10]);
    std::size_t cd = le32(&zip[eocd + 16]);
    for (unsigned e = 0; e < entries; ++e)
    {
        if (cd + 46 > zip.size() || le32(&zip[cd]) != 0x02014b50)
            return false;
        Entry en;
        en.method = le16(&zip[cd + 10]);
        en.csize = le32(&zip[cd + 20]);
        en.usize = le32(&zip[cd + 24]);
        const std::uint16_t nl = le16(&zip[cd + 28]), xl = le16(&zip[cd + 30]), cl = le16(&zip[cd + 32]);
        en.local = le32(&zip[cd + 42]);
        if (cd + 46 + nl > zip.size())
            return false;
        en.name.assign(reinterpret_cast<const char *>(&zip[cd + 46]), nl);
        cd += 46 + std::size_t(nl) + xl + cl;
        out.push_back(en);
    }
    return true;
}

bool extract(const std::vector<std::uint8_t> &zip, const Entry &en, std::vector<std::uint8_t> &out)
{
    if (std::size_t(en.local) + 30 > zip.size() || le32(&zip[en.local]) != 0x04034b50)
        return false;
    const std::size_t data = en.local + 30 + le16(&zip[en.local + 26]) + le16(&zip[en.local + 28]);
    if (data + en.csize > zip.size())
        return false;
    if (en.method == 0)
    {
        out.assign(zip.begin() + std::ptrdiff_t(data), zip.begin() + std::ptrdiff_t(data + en.csize));
        return true;
    }
    if (en.method != 8)
        return false;
    int len = 0;
    char *p = stbi_zlib_decode_noheader_malloc(reinterpret_cast<const char *>(&zip[data]), int(en.csize), &len);
    if (!p)
        return false;
    out.assign(reinterpret_cast<std::uint8_t *>(p), reinterpret_cast<std::uint8_t *>(p) + len);
    STBI_FREE(p);
    return out.size() == en.usize;
}

/* manifest.sha256: "<hash> *<path>" lines. */
std::map<std::string, std::string> parse_manifest(const std::string &text)
{
    std::map<std::string, std::string> out;
    std::size_t at = 0;
    while (at < text.size())
    {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos)
            end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        const std::size_t star = line.find(" *");
        if (star == 64)
            out[line.substr(star + 2)] = line.substr(0, 64);
    }
    return out;
}

std::string read_file(const std::string &path)
{
    std::string s;
    if (std::FILE *f = std::fopen(path.c_str(), "rb"))
    {
        char buf[16384];
        std::size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
            s.append(buf, n);
        std::fclose(f);
    }
    return s;
}

void make_dirs(const std::string &path)
{
    for (std::size_t i = 1; i < path.size(); ++i)
        if (path[i] == '/')
            mkdir(path.substr(0, i).c_str(), 0777);
}

void *check_worker(void *)
{
    porpoise::http::Session http("porpoise-update");
    std::vector<std::uint8_t> data;
    const int status = http.init() ? http.get("https://api.github.com/repos/elripalda/Porpoise/releases/latest", data)
                                   : -1;
    http.term();
    if (status != 200)
    {
        set(Phase::Failed, 0, 0, status < 0 ? "Porpoise couldn't reach GitHub." : "GitHub didn't answer.");
        g_busy = false;
        return nullptr;
    }
    const std::string tmp = g_cache + ".part";
    if (std::FILE *f = std::fopen(tmp.c_str(), "wb"))
    {
        const bool ok = std::fwrite(data.data(), 1, data.size(), f) == data.size();
        if (std::fclose(f) == 0 && ok)
            std::rename(tmp.c_str(), g_cache.c_str());
        else
            std::remove(tmp.c_str());
    }
    set(Phase::Checked);
    g_busy = false;
    return nullptr;
}

void *install_worker(void *)
{
    const Release r = g_release;
    /* 1. The zip, checked. */
    set(Phase::Downloading, 0, r.size);
    std::vector<std::uint8_t> zip;
    {
        porpoise::http::Session http("porpoise-update");
        const int status = http.init()
                               ? http.get(r.zip_url, zip,
                                          [&](std::size_t n) {
                                              set(Phase::Downloading, n, r.size);
                                              return true;
                                          },
                                          std::size_t(256) << 20)
                               : -1;
        http.term();
        if (status != 200)
        {
            set(Phase::Failed, 0, 0, "The download didn't finish. Nothing was changed.");
            g_busy = false;
            return nullptr;
        }
    }
    if (r.size && zip.size() != r.size)
    {
        set(Phase::Failed, 0, 0, "The download is the wrong size. Nothing was changed.");
        g_busy = false;
        return nullptr;
    }
    const std::string got = sha256(zip.data(), zip.size());
    if (!r.sha256.empty() && got != r.sha256)
    {
        set(Phase::Failed, 0, 0, "The download doesn't match its SHA-256. Nothing was changed.");
        g_busy = false;
        return nullptr;
    }
    ps5::debug::mark(("update: " + r.tag + " downloaded, SHA-256 " + got).c_str());

    /* 2. Its files and its manifest. */
    std::vector<Entry> entries;
    if (!list_zip(zip, entries))
    {
        set(Phase::Failed, 0, 0, "The download isn't a zip Porpoise can read. Nothing was changed.");
        g_busy = false;
        return nullptr;
    }
    std::string prefix;
    for (const Entry &e : entries)
        if (e.name.size() > 16 && e.name.compare(e.name.size() - 16, 16, "/manifest.sha256") == 0)
            prefix = e.name.substr(0, e.name.size() - 15);
    std::map<std::string, std::string> manifest;
    for (const Entry &e : entries)
        if (!prefix.empty() && e.name == prefix + "manifest.sha256")
        {
            std::vector<std::uint8_t> m;
            if (extract(zip, e, m))
                manifest = parse_manifest(std::string(m.begin(), m.end()));
        }
    if (prefix.empty() || manifest.empty() || manifest.find("eboot.bin") == manifest.end())
    {
        set(Phase::Failed, 0, 0, "The download isn't a Porpoise release. Nothing was changed.");
        g_busy = false;
        return nullptr;
    }
    const std::map<std::string, std::string> old_manifest = parse_manifest(read_file(g_app + "/manifest.sha256"));

    /* 3. Every file written beside the old one as <name>.new and checked;
     * nothing is replaced until all of them are there. */
    std::vector<const Entry *> files;
    for (const Entry &e : entries)
        if (e.name.compare(0, prefix.size(), prefix) == 0 && e.name.back() != '/' &&
            e.name.find("..", prefix.size()) == std::string::npos)
            files.push_back(&e);
    for (const auto &kv : manifest)
    {
        bool present = kv.first == "manifest.sha256";
        for (const Entry *e : files)
            present = present || e->name.compare(prefix.size(), std::string::npos, kv.first) == 0;
        if (!present)
        {
            set(Phase::Failed, 0, 0, "The download is missing a file. Nothing was changed.");
            g_busy = false;
            return nullptr;
        }
    }
    std::vector<std::string> written;
    auto undo = [&](const std::string &error) {
        for (const std::string &rel : written)
            std::remove((g_app + "/" + rel + ".new").c_str());
        set(Phase::Failed, 0, 0, error);
        g_busy = false;
    };
    set(Phase::Installing, 0, files.size());
    for (const Entry *e : files)
    {
        const std::string rel = e->name.substr(prefix.size());
        std::vector<std::uint8_t> body;
        const auto want = manifest.find(rel);
        if (!extract(zip, *e, body) ||
            (want != manifest.end() && sha256(body.data(), body.size()) != want->second))
        {
            undo("A file in the download is damaged. Nothing was changed.");
            return nullptr;
        }
        const std::string target = g_app + "/" + rel, part = target + ".new";
        make_dirs(target);
        std::FILE *f = std::fopen(part.c_str(), "wb");
        bool ok = f && std::fwrite(body.data(), 1, body.size(), f) == body.size() && std::fflush(f) == 0 &&
                  fsync(fileno(f)) == 0;
        if (f)
            ok = std::fclose(f) == 0 && ok;
        if (ok)
        {
            /* The old file's permissions, for programs and libraries. */
            struct stat st;
            if (stat(target.c_str(), &st) == 0)
                chmod(part.c_str(), st.st_mode & 07777);
        }
        written.push_back(rel);
        if (!ok)
        {
            undo("Porpoise couldn't write its own folder (the console may be full). Nothing was changed.");
            return nullptr;
        }
        set(Phase::Installing, written.size(), files.size());
    }
    /* 4. All of them in place: eboot.bin, then the manifest, last. */
    set(Phase::Finishing, files.size(), files.size());
    std::stable_sort(written.begin(), written.end(), [](const std::string &a, const std::string &b) {
        auto rank = [](const std::string &rel) { return rel == "manifest.sha256" ? 2 : rel == "eboot.bin" ? 1 : 0; };
        return rank(a) < rank(b);
    });
    for (std::size_t i = 0; i < written.size(); ++i)
    {
        const std::string target = g_app + "/" + written[i];
        if (std::rename((target + ".new").c_str(), target.c_str()) != 0)
        {
            for (std::size_t j = i; j < written.size(); ++j)
                std::remove((g_app + "/" + written[j] + ".new").c_str());
            set(Phase::Failed, 0, 0,
                i == 0 ? "Porpoise couldn't write its own folder. Nothing was changed."
                       : "Porpoise was only partly updated. Download the release from GitHub and copy it in by hand.");
            g_busy = false;
            return nullptr;
        }
    }
    /* 5. What the old build had and the new one doesn't. */
    for (const auto &kv : old_manifest)
        if (manifest.find(kv.first) == manifest.end() && kv.first.find("..") == std::string::npos)
            std::remove((g_app + "/" + kv.first).c_str());
    ps5::debug::mark(("update: " + r.tag + " installed").c_str());
    set(Phase::Done, files.size(), files.size());
    g_busy = false;
    return nullptr;
}
} // namespace

/* The end of the JSON value (object or array) that opens at `open`, minding
 * strings; npos when it doesn't close. */
std::size_t matching(const std::string &json, std::size_t open)
{
    int depth = 0;
    bool in_string = false;
    for (std::size_t i = open; i < json.size(); ++i)
    {
        const char c = json[i];
        if (in_string)
        {
            if (c == '\\')
                ++i;
            else if (c == '"')
                in_string = false;
            continue;
        }
        if (c == '"')
            in_string = true;
        else if (c == '{' || c == '[')
            ++depth;
        else if ((c == '}' || c == ']') && --depth == 0)
            return i;
    }
    return std::string::npos;
}

bool parse(const std::string &json, Release &out)
{
    out = Release{};
    out.tag = field(json, 0, json.size(), "tag_name");
    out.page = field(json, 0, json.size(), "html_url");
    /* The release's own name comes before its assets (theirs are inside them). */
    {
        const std::size_t assets = json.find("\"assets\"");
        const std::string name = field(json, 0, assets == std::string::npos ? json.size() : assets, "name");
        const std::size_t b = name.find("build ");
        if (b != std::string::npos)
            out.build = std::atoi(name.c_str() + b + 6);
    }
    /* The release's Porpoise-*.zip: each asset is looked at on its own, its
     * name taken from the end of its download link. */
    const std::size_t key = json.find("\"assets\"");
    const std::size_t open = key == std::string::npos ? key : json.find('[', key);
    const std::size_t close = open == std::string::npos ? open : matching(json, open);
    if (close != std::string::npos)
        for (std::size_t at = json.find('{', open); at != std::string::npos && at < close;)
        {
            const std::size_t end = matching(json, at);
            if (end == std::string::npos || end > close)
                break;
            const std::string url = field(json, at, end, "browser_download_url");
            const std::string name = url.substr(url.rfind('/') == std::string::npos ? 0 : url.rfind('/') + 1);
            if (name.rfind("Porpoise", 0) == 0 && name.size() > 4 && name.compare(name.size() - 4, 4, ".zip") == 0)
            {
                out.zip_url = url;
                out.size = std::size_t(std::strtoull(field(json, at, end, "size").c_str(), nullptr, 10));
                const std::string digest = field(json, at, end, "digest");
                if (digest.rfind("sha256:", 0) == 0)
                    out.sha256 = digest.substr(7);
                break;
            }
            at = json.find('{', end);
        }
    return !out.tag.empty();
}

bool read_cached(const std::string &path, Release &out)
{
    return parse(read_file(path), out);
}

Progress progress()
{
    pthread_mutex_lock(&g_mutex);
    Progress p = g_progress;
    pthread_mutex_unlock(&g_mutex);
    return p;
}

void start_check(const std::string &cache_path)
{
    if (g_busy.exchange(true))
        return;
    g_cache = cache_path;
    set(Phase::Checking);
    pthread_t t;
    if (create_title_thread(&t, check_worker, nullptr) == 0)
        pthread_detach(t);
    else
        check_worker(nullptr);
}

void start_install(const Release &release, const std::string &app_dir)
{
    if (g_busy.exchange(true))
        return;
    if (release.zip_url.empty())
    {
        set(Phase::Failed, 0, 0, "This release has no Porpoise zip. Download it from GitHub by hand.");
        g_busy = false;
        return;
    }
    g_release = release;
    g_app = app_dir;
    set(Phase::Downloading, 0, release.size);
    pthread_t t;
    if (create_title_thread(&t, install_worker, nullptr) == 0)
        pthread_detach(t);
    else
        install_worker(nullptr);
}

void acknowledge()
{
    if (!g_busy.load())
        set(Phase::Idle);
}
} // namespace porpoise::update

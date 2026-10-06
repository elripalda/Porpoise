/* Porpoise - HTTPS through the console's own libSceNet / libSceSsl / libSceHttp2.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * One session, one request at a time, every phase with a timeout, and no
 * request at all when the console isn't online. Used by the covers worker and
 * the updater; one session at a time across Porpoise (a second waits). */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace porpoise::http
{
class Session
{
public:
    /* agent: the User-Agent its requests carry. */
    explicit Session(const char *name, const char *agent = "Porpoise/2.1") : name_(name), agent_(agent) {}
    ~Session() { term(); }
    /* False when the console is offline or the network can't be set up. */
    bool init();
    void term();
    /* The HTTP status (200 fills out), or -1 when the request failed. progress
     * is told the bytes read so far and may return false to stop; limit caps
     * the answer's size. */
    int get(const std::string &url, std::vector<std::uint8_t> &out,
            const std::function<bool(std::size_t)> &progress = {}, std::size_t limit = std::size_t(64) << 20);
    /* A form POST (form set) or a GET, keeping the answer whatever its status
     * (RetroAchievements explains a refusal in the body). The HTTP status, or
     * -1 when there was no answer. */
    int request(const std::string &url, const std::string *form, std::vector<std::uint8_t> &out,
                std::size_t limit = std::size_t(4) << 20);

private:
    const char *name_;
    const char *agent_;
    int pool_ = -1, ssl_ = -1, ctx_ = -1, tmpl_ = -1;
    bool netctl_ = false, locked_ = false;
};
} // namespace porpoise::http

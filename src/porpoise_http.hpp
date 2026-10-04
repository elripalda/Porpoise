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
    explicit Session(const char *name) : name_(name) {}
    ~Session() { term(); }
    /* False when the console is offline or the network can't be set up. */
    bool init();
    void term();
    /* The HTTP status (200 fills out), or -1 when the request failed. progress
     * is told the bytes read so far and may return false to stop; limit caps
     * the answer's size. */
    int get(const std::string &url, std::vector<std::uint8_t> &out,
            const std::function<bool(std::size_t)> &progress = {}, std::size_t limit = std::size_t(64) << 20);

private:
    const char *name_;
    int pool_ = -1, ssl_ = -1, ctx_ = -1, tmpl_ = -1;
    bool netctl_ = false, locked_ = false;
};
} // namespace porpoise::http

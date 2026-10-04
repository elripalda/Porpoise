/* Porpoise - updating itself from its GitHub releases.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * GitHub's answer for the newest release names its zip, the zip's size and
 * its SHA-256. The update downloads the zip, checks its size and SHA-256, and
 * installs it into the running app's own folder (/app0) in two steps: every
 * file is first written beside the old one as <name>.new and checked against
 * the SHA-256 the zip's manifest.sha256 gives it; only when all of them are
 * there are they renamed over the old ones, eboot.bin and the manifest last.
 * If anything fails before that, the .new files are removed and nothing has
 * changed. Files the old manifest had and the new one doesn't are removed.
 * Games, saves and settings live in /data/porpoise and aren't touched. Then
 * Porpoise has to be opened again: the running one is the old build.
 *
 * Everything runs on a worker thread; progress() says how far it is. */
#pragma once

#include <cstddef>
#include <string>

namespace porpoise::update
{
struct Release
{
    std::string tag;     /* v1.2 */
    std::string page;    /* the release's web page */
    std::string zip_url; /* Porpoise-1.2.zip */
    std::string sha256;  /* of the zip, as GitHub gives it ("" when it doesn't) */
    std::size_t size = 0;
};

/* GitHub's /releases/latest answer. */
bool parse(const std::string &json, Release &out);
bool read_cached(const std::string &path, Release &out);

enum class Phase
{
    Idle,
    Checking,
    Downloading,
    Installing,
    Done,   /* installed: Porpoise must be opened again */
    Failed, /* error() says why; nothing was changed unless installing had begun */
    Checked,
};
struct Progress
{
    Phase phase = Phase::Idle;
    std::size_t done = 0, total = 0;
    std::string error;
};
Progress progress();
/* Looks for a newer release now; the answer is written to cache_path. */
void start_check(const std::string &cache_path);
/* Downloads the release and puts it in app_dir; the zip is held in memory. */
void start_install(const Release &release, const std::string &app_dir = "/app0");
/* Back to Idle once the UI has shown the result. */
void acknowledge();
} // namespace porpoise::update

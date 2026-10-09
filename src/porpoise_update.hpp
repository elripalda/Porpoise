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

#include "porpoise_paths.hpp"
#include <cstddef>
#include <string>
#include <vector>

namespace porpoise::update
{
struct Release
{
    std::string tag;     /* v1.2 */
    std::string page;    /* the release's web page */
    std::string zip_url; /* Porpoise-1.2.zip */
    std::string sha256;  /* of the zip, as GitHub gives it ("" when it doesn't) */
    std::size_t size = 0;
    int build = 0;       /* from the release's name, "Porpoise 1.1 (build 14)"; 0 when it has none */
    int beta = 0;        /* from the tag, "v2.0-beta.1": 1; 0 for a final release; an alpha -1000 + n */
    bool prerelease = false;
    std::string name;    /* "Porpoise 2.0 beta 1" */
};

/* One release object of GitHub's answer. */
bool parse(const std::string &json, Release &out);
/* GitHub's /releases answer (newest first), or a single release object;
 * drafts and releases without a Porpoise zip are left out. */
bool parse_list(const std::string &json, std::vector<Release> &out);
/* The newest release in the cached answer; betas: pre-releases count too. */
bool read_cached(const std::string &path, Release &out, bool betas = false);
bool read_cached_list(const std::string &path, std::vector<Release> &out);
/* Orders versions: (major, minor, patch, beta - a final release after its
 * betas -, build). Returns <0, 0 or >0. */
int compare_versions(int major_a, int minor_a, int patch_a, int beta_a, int build_a, int major_b, int minor_b,
                     int patch_b, int beta_b, int build_b);
/* A tag's numbers: "v1.5.1" -> 1 5 1, "v2.0-beta.1" -> 2 0 0 beta 1,
 * "v3.0-alpha.1" -> 3 0 0 beta -999 (an alpha comes before the betas). */
bool tag_version(const std::string &tag, int &major, int &minor, int &patch, int &beta);

enum class Phase
{
    Idle,
    Checking,
    Downloading,
    Installing,
    Done,   /* installed: Porpoise must be opened again */
    Failed, /* error() says why; nothing was changed unless installing had begun */
    Checked,
    Finishing, /* every file written: swapping them in and tidying up */
};
struct Progress
{
    Phase phase = Phase::Idle;
    std::size_t done = 0, total = 0;
    std::string error;
};
Progress progress();
/* Looks for releases now (GitHub's list, betas and older ones too); the
 * answer is written to cache_path. */
void start_check(const std::string &cache_path);
/* Downloads the release and puts it in app_dir; the zip is held in memory. */
void start_install(const Release &release, const std::string &app_dir = PORPOISE_APP);
/* Back to Idle once the UI has shown the result. */
void acknowledge();
} // namespace porpoise::update

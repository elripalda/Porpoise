/* Porpoise - GameTDB's game database, turned into a small table.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace porpoise::gametdb
{
/* The download: the database in a language (EN, ES, FR, PT, IT), with English
 * where a game has no text in it, GameCube games included. */
inline std::string database_url(const std::string &lang)
{
    return "https://www.gametdb.com/wiitdb.zip?LANG=" + (lang.empty() ? std::string("EN") : lang) +
           "&FALLBACK=TRUE&WIIWARE=FALSE&GAMECUBE=TRUE";
}

/* Reads wiitdb.zip (as downloaded) and writes info.tsv: one disc game per
 * line - id, title, synopsis, developer, publisher, released, genre, players,
 * rating - tabs and newlines escaped. Returns the number of games written,
 * or -1 with a reason. */
/* lang: the description is taken from that language when the game has one
 * there, else from English; titles are always the English ones. */
int zip_to_table(const std::vector<std::uint8_t> &zip, const std::string &tsv_path, std::string &error,
                 const std::string &lang = "EN");

/* The same from the XML itself (for tests). */
int xml_to_table(const std::string &xml, const std::string &tsv_path, std::string &error,
                 const std::string &lang = "EN");
} // namespace porpoise::gametdb

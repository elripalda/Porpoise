/* Porpoise - GameTDB's game database, turned into a small table.
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * GameTDB (https://www.gametdb.com) publishes its Wii and GameCube database
 * as wiitdb.zip holding wiitdb.xml. Each disc is a <game> element with <id>,
 * <type> (empty for Wii discs, "GameCube" for GameCube ones), a <locale
 * lang="EN"> with <title> and <synopsis>, <developer>, <publisher>, <date
 * year month day>, <genre>, <rating type value> and <input players> - the
 * fields USB Loader GX reads (source/xml/GameTDB.cpp). Porpoise keeps those
 * fields for disc games only, in a tab-separated file it can read in a blink.
 *
 * The zip is read through its central directory (sizes in the local header may
 * be zero when the archive was streamed) and its deflate stream inflated with
 * stb_image's zlib decoder. */
#include "porpoise_gametdb.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb/stb_image.h"
#pragma clang diagnostic pop

namespace porpoise::gametdb
{
namespace
{
std::uint32_t le32(const std::uint8_t *p)
{
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}

std::uint16_t le16(const std::uint8_t *p)
{
    return std::uint16_t(p[0] | p[1] << 8);
}

/* The text between <tag> and </tag> inside [from, to), or "". */
std::string child(const std::string &xml, std::size_t from, std::size_t to, const char *tag)
{
    const std::string open = std::string("<") + tag, close = std::string("</") + tag + ">";
    std::size_t at = from;
    for (;;)
    {
        at = xml.find(open, at);
        if (at == std::string::npos || at >= to)
            return "";
        const char after = xml[at + open.size()];
        if (after == '>' || after == ' ')
            break;
        at += open.size();
    }
    const std::size_t gt = xml.find('>', at);
    if (gt == std::string::npos || gt >= to || xml[gt - 1] == '/')
        return "";
    const std::size_t end = xml.find(close, gt);
    if (end == std::string::npos || end > to)
        return "";
    return xml.substr(gt + 1, end - gt - 1);
}

/* An attribute of the first <tag ...> inside [from, to). */
std::string attribute(const std::string &xml, std::size_t from, std::size_t to, const char *tag, const char *name)
{
    const std::string open = std::string("<") + tag + " ";
    const std::size_t at = xml.find(open, from);
    if (at == std::string::npos || at >= to)
        return "";
    const std::size_t gt = xml.find('>', at);
    const std::string key = std::string(" ") + name + "=\"";
    const std::size_t k = xml.find(key, at);
    if (k == std::string::npos || k > gt)
        return "";
    const std::size_t v = k + key.size(), q = xml.find('"', v);
    return q == std::string::npos ? "" : xml.substr(v, q - v);
}

std::string entities(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] != '&')
        {
            out += s[i];
            continue;
        }
        const std::size_t semi = s.find(';', i);
        if (semi == std::string::npos || semi - i > 10)
        {
            out += '&';
            continue;
        }
        const std::string e = s.substr(i + 1, semi - i - 1);
        if (e == "amp") out += '&';
        else if (e == "lt") out += '<';
        else if (e == "gt") out += '>';
        else if (e == "quot") out += '"';
        else if (e == "apos") out += '\'';
        else if (!e.empty() && e[0] == '#')
        {
            const unsigned long cp = e.size() > 1 && (e[1] == 'x' || e[1] == 'X')
                                         ? std::strtoul(e.c_str() + 2, nullptr, 16)
                                         : std::strtoul(e.c_str() + 1, nullptr, 10);
            if (cp < 0x80)
                out += char(cp);
            else if (cp < 0x800)
            {
                out += char(0xC0 | (cp >> 6));
                out += char(0x80 | (cp & 0x3F));
            }
            else
            {
                out += char(0xE0 | (cp >> 12));
                out += char(0x80 | ((cp >> 6) & 0x3F));
                out += char(0x80 | (cp & 0x3F));
            }
        }
        else
        {
            out += '&';
            continue;
        }
        i = semi;
    }
    return out;
}

/* One line of the table: tabs, newlines and backslashes escaped. */
std::string field(const std::string &s)
{
    std::string out;
    for (char c : s)
    {
        if (c == '\t') out += "\\t";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') continue;
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

std::string tidy(std::string s)
{
    /* Collapse runs of spaces; trim. */
    std::string out;
    bool space = false;
    for (char c : s)
    {
        if (c == ' ' || c == '\t')
        {
            space = true;
            continue;
        }
        if (space && !out.empty() && c != '\n')
            out += ' ';
        space = false;
        out += c;
    }
    return out;
}

std::string released(const std::string &y, const std::string &m, const std::string &d)
{
    static const char *const months[] = {"January", "February", "March",     "April",   "May",      "June",
                                         "July",    "August",   "September", "October", "November", "December"};
    const int year = std::atoi(y.c_str()), month = std::atoi(m.c_str()), day = std::atoi(d.c_str());
    if (year <= 0)
        return "";
    char buf[48];
    if (month >= 1 && month <= 12 && day >= 1)
        std::snprintf(buf, sizeof buf, "%s %d, %d", months[month - 1], day, year);
    else if (month >= 1 && month <= 12)
        std::snprintf(buf, sizeof buf, "%s %d", months[month - 1], year);
    else
        std::snprintf(buf, sizeof buf, "%d", year);
    return buf;
}

std::string genres(const std::string &g)
{
    /* "sports,soccer" -> "Sports, Soccer" */
    std::string out;
    bool start = true;
    for (char c : g)
    {
        if (c == ',')
        {
            out += ", ";
            start = true;
            continue;
        }
        out += start ? char(std::toupper(static_cast<unsigned char>(c))) : c;
        start = c == ' ';
    }
    return out;
}
} // namespace

/* xml.find(needle, from), but only before `end`: a game's own entry, not the
 * rest of the database after it. */
static std::size_t find_in(const std::string &xml, const std::string &needle, std::size_t from, std::size_t end)
{
    if (from >= end || end > xml.size())
        return std::string::npos;
    const std::size_t at = std::string_view(xml).substr(from, end - from).find(needle);
    return at == std::string_view::npos ? std::string::npos : from + at;
}

int xml_to_table(const std::string &xml, const std::string &tsv_path, std::string &error, const std::string &lang)
{
    const std::string tmp = tsv_path + ".part";
    std::FILE *f = std::fopen(tmp.c_str(), "w");
    if (!f)
    {
        error = "cannot write " + tmp;
        return -1;
    }
    int count = 0;
    std::size_t at = 0;
    for (;;)
    {
        const std::size_t start = xml.find("<game name=", at);
        if (start == std::string::npos)
            break;
        const std::size_t end = xml.find("</game>", start);
        if (end == std::string::npos)
            break;
        at = end + 7;
        const std::string id = child(xml, start, end, "id");
        const std::string type = child(xml, start, end, "type");
        if (id.size() != 6 || !(type.empty() || type == "GameCube" || type == "Wii"))
            continue;
        /* The English locale, else the first one. */
        std::size_t loc = find_in(xml, "<locale lang=\"EN\"", start, end);
        if (loc == std::string::npos)
            loc = find_in(xml, "<locale", start, end);
        std::size_t loc_end = loc == std::string::npos ? std::string::npos : xml.find("</locale>", loc);
        std::string title, synopsis;
        if (loc != std::string::npos && loc < end && loc_end != std::string::npos && loc_end < end)
        {
            title = entities(child(xml, loc, loc_end, "title"));
            synopsis = tidy(entities(child(xml, loc, loc_end, "synopsis")));
        }
        if (lang != "EN")
        {
            /* The description in the player's language, when there is one. */
            const std::size_t own = find_in(xml, "<locale lang=\"" + lang + "\"", start, end);
            const std::size_t own_end = own == std::string::npos ? own : xml.find("</locale>", own);
            if (own != std::string::npos && own < end && own_end != std::string::npos && own_end < end)
            {
                const std::string text = tidy(entities(child(xml, own, own_end, "synopsis")));
                if (!text.empty())
                    synopsis = text;
            }
        }
        const std::string rating_type = attribute(xml, start, end, "rating", "type");
        const std::string rating_value = attribute(xml, start, end, "rating", "value");
        const std::string players = attribute(xml, start, end, "input", "players");
        std::string rating = rating_type.empty() ? "" : rating_type + " " + rating_value;
        std::fprintf(f, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", field(id).c_str(), field(title).c_str(),
                     field(synopsis).c_str(), field(entities(child(xml, start, end, "developer"))).c_str(),
                     field(entities(child(xml, start, end, "publisher"))).c_str(),
                     field(released(attribute(xml, start, end, "date", "year"), attribute(xml, start, end, "date", "month"),
                                    attribute(xml, start, end, "date", "day")))
                         .c_str(),
                     field(genres(entities(child(xml, start, end, "genre")))).c_str(),
                     players.empty() ? "0" : players.c_str(), field(rating).c_str());
        ++count;
    }
    std::fclose(f);
    if (count == 0)
    {
        std::remove(tmp.c_str());
        error = "no games in the database";
        return -1;
    }
    if (std::rename(tmp.c_str(), tsv_path.c_str()) != 0)
    {
        error = "cannot replace " + tsv_path;
        return -1;
    }
    return count;
}

int zip_to_table(const std::vector<std::uint8_t> &zip, const std::string &tsv_path, std::string &error,
                 const std::string &lang)
{
    /* End of central directory: the last 22+ bytes. */
    if (zip.size() < 22)
    {
        error = "not a zip";
        return -1;
    }
    std::size_t eocd = std::string::npos;
    for (std::size_t i = zip.size() - 22 + 1; i-- > 0 && zip.size() - i < 70000;)
        if (le32(&zip[i]) == 0x06054b50)
        {
            eocd = i;
            break;
        }
    if (eocd == std::string::npos)
    {
        error = "no zip directory";
        return -1;
    }
    const std::uint16_t entries = le16(&zip[eocd + 10]);
    std::size_t cd = le32(&zip[eocd + 16]);
    for (unsigned e = 0; e < entries; ++e)
    {
        if (cd + 46 > zip.size() || le32(&zip[cd]) != 0x02014b50)
            break;
        const std::uint16_t method = le16(&zip[cd + 10]);
        const std::uint32_t csize = le32(&zip[cd + 20]), usize = le32(&zip[cd + 24]);
        const std::uint16_t name_len = le16(&zip[cd + 28]), extra_len = le16(&zip[cd + 30]),
                            comment_len = le16(&zip[cd + 32]);
        const std::uint32_t local = le32(&zip[cd + 42]);
        const std::string name(reinterpret_cast<const char *>(&zip[cd + 46]), name_len);
        cd += 46 + name_len + extra_len + comment_len;
        if (name.size() < 4 || name.compare(name.size() - 4, 4, ".xml") != 0)
            continue;
        if (local + 30 > zip.size() || le32(&zip[local]) != 0x04034b50)
            break;
        const std::size_t data = local + 30 + le16(&zip[local + 26]) + le16(&zip[local + 28]);
        if (data + csize > zip.size())
        {
            error = "zip is cut short";
            return -1;
        }
        std::string xml;
        if (method == 0)
            xml.assign(reinterpret_cast<const char *>(&zip[data]), csize);
        else if (method == 8)
        {
            int out_len = 0;
            char *out = stbi_zlib_decode_noheader_malloc(reinterpret_cast<const char *>(&zip[data]), int(csize),
                                                         &out_len);
            if (!out)
            {
                error = "cannot inflate " + name;
                return -1;
            }
            xml.assign(out, std::size_t(out_len));
            STBI_FREE(out);
        }
        else
        {
            error = "unsupported compression";
            return -1;
        }
        (void)usize;
        return xml_to_table(xml, tsv_path, error, lang);
    }
    error = "no .xml in the zip";
    return -1;
}
} // namespace porpoise::gametdb

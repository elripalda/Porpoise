/* Porpoise UI - Online: Settings > Online's rows, and the Online hub, the one
 * place for Wii games online. Four ways in, each a card on the left:
 *
 * - Custom Server: a community's own server, reached as a Wii reaches one -
 *   through its DNS server. The servers are kept in a list (name and address,
 *   <data>/online-servers.txt), one is chosen, and Test asks it for
 *   Nintendo's login server to see that it answers. With it on, Porpoise adds
 *   the https-to-http code to every Wii game (ui_cheats add_online_code), so
 *   no game needs a cheat file of its own.
 * - WiiLink WFC: a code per game from WiiLink's site, dropped into the cheats
 *   folder; the page lists the games that have one.
 * - Wiimmfi: the game patched on a computer; the page says how.
 * - WiiConnect24: the channels through WiiLink, and which are installed.
 *
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "ui_app.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <memory>
#include <mutex>
#include <sys/stat.h>

#include "porpoise_pad.hpp"
#include "title_threads.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"
#ifndef PORPOISE_DESKTOP
#include "porpoise_dns.hpp"
#endif

namespace porpoise::ui
{
using namespace look;
using namespace porpoise::pad;

struct OnlineTest
{
    std::mutex m;
    std::atomic<bool> done{false};
    int result = -1; /* 1 an address, 0 an answer without one, -1 none */
    int ms = 0;
    std::string server, address;
};

namespace
{
enum Mode
{
    ModeCustom,
    ModeWiiLink,
    ModeWiimmfi,
    ModeChannels,
    ModeCount,
};

/* Nintendo's login server: what a game asks for first. */
constexpr const char *kLoginServer = "naswii.nintendowifi.net";

void *run_test(void *arg)
{
    auto *held = static_cast<std::shared_ptr<OnlineTest> *>(arg);
    std::shared_ptr<OnlineTest> test = *held;
    delete held;
    int result = -1, ms = 0;
    std::string address;
#ifndef PORPOISE_DESKTOP
    result = porpoise::dns::probe(test->server, kLoginServer, ms, address);
#endif
    std::lock_guard<std::mutex> lock(test->m);
    test->result = result;
    test->ms = ms;
    test->address = address;
    test->done = true;
    return nullptr;
}

/* Four numbers 0-255 with dots between. */
bool dotted_quad(const std::string &text)
{
    int parts = 0, value = -1;
    for (char c : text)
    {
        if (c >= '0' && c <= '9')
        {
            if (value == 0)
                return false; /* a leading zero (192.168.001.2): not read as meant */
            value = (value < 0 ? 0 : value) * 10 + (c - '0');
            if (value > 255)
                return false;
        }
        else if (c == '.' && value >= 0 && parts < 3)
        {
            ++parts;
            value = -1;
        }
        else
            return false;
    }
    return parts == 3 && value >= 0;
}

void dots(Gfx &g, float x, float y, double time, bool still, Color c)
{
    for (int i = 0; i < 3; ++i)
    {
        const float wave = still ? 0.0f : std::max(0.0f, std::sin(float(time) * 7.0f - float(i) * 0.9f));
        g.panel(x + 9 + float(i) * 24 - 8, y - 9.0f * wave - 8, 16, 16,
                with_alpha(c, 0.4f + 0.6f * (still ? 1.0f : wave)), 1, 8);
    }
}

/* The WiiConnect24 channels, by their IDs' first three letters. */
struct WcChannel
{
    const char *id, *name;
};
constexpr WcChannel kChannels[] = {
    {"HAF", "Forecast Channel"},     {"HAG", "News Channel"},        {"HAJ", "Everybody Votes Channel"},
    {"HAP", "Check Mii Out Channel"}, {"HAT", "Nintendo Channel"},    {"HAY", "Photo Channel"},
};
} // namespace

/* ---- Settings > Online ---------------------------------------------------------------------- */

void App::add_online_rows(Settings &t, bool per_game)
{
    if (!per_game)
    {
        SettingRow r;
        r.section = "Online";
        r.label = tr("Online Hub");
        r.help = tr("Everything for Wii games online in one place: custom servers (with a test), WiiLink WFC, "
                    "Wiimmfi and the WiiConnect24 channels.");
        r.values = {tr("Open\xE2\x80\xA6")};
        r.action = kRowOnlineHub;
        rows_.push_back(r);
    }
    {
        SettingRow r;
        r.section = "Online";
        r.key = "online_custom";
        r.label = tr("Custom Server");
        r.help = tr("Wii games go online through a community's server: the DNS server below sends them there, and "
                    "Porpoise adds the fix such servers need to every Wii game. Off: each game goes online its own "
                    "way (a game patched for Wiimmfi, or a WiiLink WFC code).");
        r.bool_value = &t.online_custom;
        r.values = {tr("Off"), tr("On")};
        rows_.push_back(r);
    }
#ifndef PORPOISE_DESKTOP
    if (!per_game)
    {
        /* The DNS server a custom server asks for. */
        SettingRow r;
        r.section = "Online";
        r.label = tr("DNS Server for Online Play");
        r.help = tr("Where Wii games look up their online servers. Automatic: the console's own. For a custom "
                    "server, type the DNS address its instructions give, as on a real Wii.");
        r.values = {settings_ && !settings_->online_dns.empty() ? settings_->online_dns : tr("Automatic")};
        r.action = kRowOnlineDns;
        rows_.push_back(r);
    }
#endif
}

/* ---- the hub -------------------------------------------------------------------------------- */

std::string App::hub_servers_path() const
{
    return data_dir_ + "/online-servers.txt";
}

void App::hub_save_servers()
{
    const std::string path = hub_servers_path(), tmp = path + ".part";
    std::FILE *f = std::fopen(tmp.c_str(), "w");
    if (!f)
        return;
    std::fprintf(f, "# Porpoise's online servers: a name, a tab, its DNS server's address\n");
    for (const auto &[name, address] : hub_.servers)
        std::fprintf(f, "%s\t%s\n", name.c_str(), address.c_str());
    const bool ok = std::fclose(f) == 0;
    if (ok)
        std::rename(tmp.c_str(), path.c_str());
}

void App::open_hub()
{
    hub_ = HubPanel{};
    hub_.open = true;
    if (std::FILE *f = std::fopen(hub_servers_path().c_str(), "r"))
    {
        char buf[512];
        while (std::fgets(buf, sizeof buf, f))
        {
            std::string line = buf;
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                line.pop_back();
            if (line.empty() || line[0] == '#')
                continue;
            const std::size_t tab = line.find('\t');
            if (tab == std::string::npos)
                continue;
            const std::string name = line.substr(0, tab), address = line.substr(tab + 1);
            if (!name.empty() && dotted_quad(address))
                hub_.servers.push_back({name, address});
        }
        std::fclose(f);
    }
    /* A server typed in before the hub (Settings' DNS row): in the list. */
    if (settings_ && !settings_->online_dns.empty())
    {
        bool listed = false;
        for (const auto &s : hub_.servers)
            listed |= s.second == settings_->online_dns;
        if (!listed)
        {
            hub_.servers.push_back({tr("My Server"), settings_->online_dns});
            hub_save_servers();
        }
    }
    sfx(Sound::DetailsFlip);
}

/* A setting the hub changes: in effect at once, as Settings' own rows do
 * after Apply (the next game uses it). */
void App::hub_set(bool Settings::*field, bool value)
{
    if (!settings_)
        return;
    settings_->*field = value;
    draft_.*field = value;
    base_.*field = value;
    settings_->save(settings_path_);
}

void App::hub_use_server(const std::string &address)
{
    if (!settings_)
        return;
    settings_->online_dns = address;
    draft_.online_dns = address;
    base_.online_dns = address;
    settings_->online_custom = draft_.online_custom = base_.online_custom = !address.empty();
    settings_->save(settings_path_);
#ifndef PORPOISE_DESKTOP
    porpoise::dns::set_server(settings_->online_custom ? address : std::string());
#endif
}

int App::hub_rows() const
{
    switch (hub_.mode)
    {
    case ModeCustom:
        return 1 + int(hub_.servers.size()) + 3; /* the switch, the servers, Add, Test, the https fix */
    case ModeChannels:
        return 1;
    default:
        return 0;
    }
}

/* The Wii games with a code file of their own (a WiiLink WFC code, most
 * likely, or any the player made): <data>/cheats/<ID>.txt or .ini. */
std::vector<const Game *> App::hub_coded_games() const
{
    std::vector<const Game *> out;
    if (!lib_)
        return out;
    for (const Game &g : lib_->games())
    {
        if (g.platform != "Wii" || !g.kind.empty() || g.id.size() != 6)
            continue;
        struct stat st{};
        for (const char *ext : {".txt", ".ini"})
            if (stat((data_dir_ + "/cheats/" + g.id + ext).c_str(), &st) == 0)
            {
                out.push_back(&g);
                break;
            }
    }
    return out;
}

App::Action App::update_hub(bool up, bool down, bool left, bool right)
{
    HubPanel &p = hub_;
    if (p.typing)
    {
        if (pressed(BtnCircle))
        {
            p.typing = 0;
            p.message.clear();
            sfx(Sound::DetailsFlip);
            return Action::None;
        }
        if (!keyboard_update(p.kb, p.text, p.typing == 1 ? 24 : 15, p.typing == 1, up, down, left, right))
            return Action::None;
        if (p.typing == 1)
        {
            /* The name; then the address on the number pad. */
            if (p.text.empty())
            {
                p.message = tr("Give the server a name first.");
                sfx(Sound::MovingTab);
                return Action::None;
            }
            p.name = p.text;
            p.text = p.edit >= 0 ? p.servers[std::size_t(p.edit)].second : std::string();
            p.typing = 2;
            p.kb = Keyboard{};
            p.kb.numeric = true;
            p.message.clear();
            sfx(Sound::DetailsFlip);
            return Action::None;
        }
        std::string address = p.text;
        while (!address.empty() && address.back() == '.')
            address.pop_back();
        if (!dotted_quad(address))
        {
            p.message = tr("That isn't an address. Type one like 192.168.1.2.");
            p.kb.numeric = true;
            sfx(Sound::MovingTab);
            return Action::None;
        }
        const bool was_chosen = p.edit >= 0 && settings_ && settings_->online_dns == p.servers[std::size_t(p.edit)].second;
        if (p.edit >= 0)
            p.servers[std::size_t(p.edit)] = {p.name, address};
        else
            p.servers.push_back({p.name, address});
        hub_save_servers();
        /* A new server (or the chosen one changed) is the one used. */
        if (p.edit < 0 || was_chosen)
            hub_use_server(address);
        p.row = 1 + (p.edit >= 0 ? p.edit : int(p.servers.size()) - 1);
        p.typing = 0;
        p.message = trf("{name} is ready. Test it to see that it answers.", {{"name", p.name}});
        sfx(Sound::LaunchGame);
        return Action::None;
    }

    if (!p.inside)
    {
        if (pressed(BtnCircle))
        {
            hub_ = HubPanel{};
            sfx(Sound::DetailsFlip);
            build_settings();
            return Action::None;
        }
        const int before = p.mode;
        if (up)
            p.mode = (p.mode + ModeCount - 1) % ModeCount;
        if (down)
            p.mode = (p.mode + 1) % ModeCount;
        if (p.mode != before)
        {
            p.row = 0;
            p.message.clear();
            sfx(Sound::MenuScroll);
        }
        if ((right || pressed(BtnCross)) && hub_rows() > 0)
        {
            p.inside = true;
            p.row = 0;
            sfx(Sound::MovingTab);
        }
        return Action::None;
    }

    const int rows = hub_rows();
    if (left || pressed(BtnCircle))
    {
        p.inside = false;
        sfx(Sound::MovingTab);
        return Action::None;
    }
    if (up && p.row > 0)
    {
        --p.row;
        sfx(Sound::MenuScroll);
    }
    if (down && p.row + 1 < rows)
    {
        ++p.row;
        sfx(Sound::MenuScroll);
    }
    if (p.mode == ModeChannels)
    {
        if (pressed(BtnCross))
        {
            hub_set(&Settings::wii_online, !settings_->wii_online);
            sfx(settings_->wii_online ? Sound::LaunchGame : Sound::MovingTab);
        }
        return Action::None;
    }
    /* Custom Server's rows. */
    const int n = int(p.servers.size());
    const int row_add = 1 + n, row_test = row_add + 1, row_https = row_test + 1;
    const bool on_server = p.row >= 1 && p.row <= n;
    if (pressed(BtnCross))
    {
        if (p.row == 0)
        {
            const bool on = !settings_->online_custom;
            if (on && settings_->online_dns.empty())
            {
                p.message = n > 0 ? tr("Pick a server below first.") : tr("Add your server below first.");
                sfx(Sound::MovingTab);
                return Action::None;
            }
            hub_set(&Settings::online_custom, on);
#ifndef PORPOISE_DESKTOP
            porpoise::dns::set_server(on ? settings_->online_dns : std::string());
#endif
            sfx(on ? Sound::LaunchGame : Sound::MovingTab);
        }
        else if (on_server)
        {
            const auto &s = p.servers[std::size_t(p.row - 1)];
            hub_use_server(s.second);
            p.message = trf("Wii games use {name} now.", {{"name", s.first}});
            sfx(Sound::LaunchGame);
        }
        else if (p.row == row_add)
        {
            p.typing = 1;
            p.edit = -1;
            p.text.clear();
            p.kb = Keyboard{};
            p.message.clear();
            sfx(Sound::DetailsFlip);
        }
        else if (p.row == row_test)
        {
            if (!settings_ || settings_->online_dns.empty())
            {
                p.message = tr("Pick a server to test first.");
                sfx(Sound::MovingTab);
                return Action::None;
            }
            if (!p.test || p.test->done)
            {
                auto test = std::make_shared<OnlineTest>();
                test->server = settings_->online_dns;
                p.test = test;
                p.message.clear();
                pthread_t thread;
                auto *arg = new std::shared_ptr<OnlineTest>(test);
                if (create_title_thread(&thread, run_test, arg) == 0)
                    pthread_detach(thread);
                else
                    run_test(arg);
                sfx(Sound::DetailsFlip);
            }
        }
        else if (p.row == row_https)
        {
            hub_set(&Settings::online_https, !settings_->online_https);
            sfx(settings_->online_https ? Sound::LaunchGame : Sound::MovingTab);
        }
    }
    else if (pressed(BtnTriangle) && on_server)
    {
        /* Change its name and address. */
        p.edit = p.row - 1;
        p.typing = 1;
        p.text = p.servers[std::size_t(p.edit)].first;
        p.kb = Keyboard{};
        p.message.clear();
        sfx(Sound::DetailsFlip);
    }
    else if (pressed(BtnSquare) && on_server)
    {
        /* Off the list; the chosen one goes with it. */
        const auto gone = p.servers[std::size_t(p.row - 1)];
        p.servers.erase(p.servers.begin() + (p.row - 1));
        hub_save_servers();
        if (settings_ && settings_->online_dns == gone.second)
            hub_use_server("");
        p.row = std::min(p.row, int(p.servers.size()) + 1);
        p.message = trf("{name} is off the list.", {{"name", gone.first}});
        sfx(Sound::MovingTab);
    }
    return Action::None;
}

void App::draw_hub()
{
    if (!hub_.open)
        return;
    HubPanel &p = hub_;
    Gfx &g = *g_;
    const bool still = settings_ && settings_->reduced_motion;
    p.anim = still ? 1.0f : std::min(1.0f, p.anim + 1.0f / 10.0f);
    const float t = ease_out(p.anim);
    p.slide = still ? float(p.mode) : smooth(p.slide, float(p.mode), 1.0 / 60.0, 14.0f);
    const Color hint = rgba(0x8A96C8), good = rgba(0x7CF0A6);
    g.set_layer();
    g.panel(0, 0, 1920, 1080, rgba(0x02040C, 0.66f * t), 1, 0);
    g.set_layer(0, 24.0f * (1.0f - t), t);

    const float w = 1640, h = 840, x = 960 - w * 0.5f, y = p.typing ? 26.0f : 96.0f;
    Glass face;
    face.tint = rgba(0x13307F, 0.93f);
    face.rim = rgba(0x8BD9FF);
    face.radius = kR;
    face.rim_w = 2.4f;
    face.glow = 14;
    face.phase = 0.4f;
    const float shown_h = p.typing ? 300.0f : h;
    glass_block(g, 960, y + shown_h * 0.5f, w, shown_h, 18, 0, 0, 42, face);

    /* The title, and where the next game goes online. */
    const bool custom_on = settings_ && settings_->online_custom && !settings_->online_dns.empty();
    std::string chosen_name;
    for (const auto &s : p.servers)
        if (settings_ && s.second == settings_->online_dns)
            chosen_name = s.first;
    g.text_mid(Font::Bold, ts(44), x + 56, y + 62, kWhite, Align::Left, tr("Online"));
    {
        const std::string state = custom_on ? trf("Wii games go online through {name}",
                                                  {{"name", chosen_name.empty() ? settings_->online_dns : chosen_name}})
                                            : tr("Wii games go online their own way");
        g.text_mid(Font::SemiBold, ts(24), x + w - 56, y + 62, custom_on ? good : hint, Align::Right,
                   fit(g, Font::SemiBold, ts(24), state, 760));
    }

    if (p.typing)
    {
        /* Typing a server's name, then its address. */
        const float fx = x + 56, fw = w - 112;
        g.text_mid(Font::SemiBold, ts(28), fx, y + 128, kSoft, Align::Left,
                   p.typing == 1 ? tr("The server's name (any name you like)")
                                 : trf("{name}'s DNS server, as its instructions give it", {{"name", p.name}}));
        g.panel(fx, y + 156, fw, 66, rgba(0x07102E, 0.85f), 1, kR, kIcy, 2.2f, 8, 0.2f);
        float tx = fx + 24;
        if (!p.text.empty())
            tx += g.text_mid(Font::SemiBold, ts(30), tx, y + 189, kWhite, Align::Left, p.text);
        else
            g.text_mid(Font::Regular, ts(26), tx + 6, y + 189, hint, Align::Left,
                       p.typing == 1 ? tr("Like Dawnofthed WFC") : tr("Like 192.168.1.2"));
        if (std::fmod(time_, 1.0) < 0.6)
            g.panel(tx + 3, y + 171, 3, 36, kIcy, 1, 0);
        if (!p.message.empty())
            g.text_mid(Font::SemiBold, ts(24), fx, y + 252, kDanger, Align::Left,
                       fit(g, Font::SemiBold, ts(24), p.message, fw));
        draw_keyboard(p.kb, y + shown_h + 40, face);
        g.set_layer();
        drawing_dialog_ = true;
        draw_prompts({{Glyph::Cross, "Type"}, {Glyph::Square, "Delete"},
                      {Glyph::Triangle, p.kb.numeric ? "Letters" : "Space"}},
                     {{Glyph::Circle, "Cancel"}, {Glyph::Options, p.typing == 1 ? "Next" : "Save"}}, "");
        drawing_dialog_ = false;
        return;
    }

    /* The four ways in, as cards. */
    const std::vector<const Game *> coded = hub_coded_games();
    std::vector<std::string> channels;
    if (lib_)
        for (const WcChannel &c : kChannels)
            for (const Game &gm : lib_->games())
                if (!gm.kind.empty() && gm.id.compare(0, 3, c.id) == 0)
                {
                    channels.push_back(tr(c.name));
                    break;
                }
    const float cx0 = x + 48, cw = 450, ch = 150, cgap = 18, cy0 = y + 120;
    const char *const titles[ModeCount] = {"Custom Server", "WiiLink WFC", "Wiimmfi", "WiiConnect24"};
    const Color accents[ModeCount] = {rgba(0x5CD3FF), rgba(0x7CF0A6), rgba(0xFFC857), rgba(0xB89BFF)};
    for (int m = 0; m < ModeCount; ++m)
    {
        const float cy = cy0 + float(m) * (ch + cgap);
        const bool here = p.mode == m, focus = here && !p.inside;
        g.panel(cx0, cy, cw, ch, focus ? rgba(0x1F63F0, 0.92f) : here ? rgba(0x183C9E, 0.75f) : rgba(0x07102E, 0.55f),
                1, kR, focus ? kIcy : here ? with_alpha(accents[m], 0.9f) : rgba(0x3D5AB0, 0.8f),
                focus ? 2.4f : 1.4f, focus ? 10 : 0, focus ? 0.25f : 0.0f);
        g.panel(cx0 + 18, cy + 22, 8, ch - 44, accents[m], 1, 4);
        g.text_mid(Font::Bold, ts(32), cx0 + 50, cy + 52, kWhite, Align::Left,
                   fit(g, Font::Bold, ts(32), tr(titles[m]), cw - 80));
        std::string status;
        bool lit = false;
        switch (m)
        {
        case ModeCustom:
            lit = custom_on;
            status = custom_on ? trf("On \xE2\x80\xA2 {name}", {{"name", chosen_name.empty() ? settings_->online_dns
                                                                                               : chosen_name}})
                               : p.servers.empty() ? tr("Add your community's server") : tr("Off");
            break;
        case ModeWiiLink:
            lit = !coded.empty();
            status = coded.empty() ? tr("A code for each game")
                                   : plural((long long)coded.size(), "1 game with a code", "{n} games with a code");
            break;
        case ModeWiimmfi:
            status = tr("Games patched on a computer");
            break;
        default:
            lit = settings_ && settings_->wii_online;
            status = lit ? (channels.empty() ? tr("On") : plural((long long)channels.size(), "On \xE2\x80\xA2 1 channel",
                                                                    "On \xE2\x80\xA2 {n} channels"))
                         : tr("Off");
            break;
        }
        g.text_mid(Font::SemiBold, ts(24), cx0 + 50, cy + 102, lit ? good : kSoft, Align::Left,
                   fit(g, Font::SemiBold, ts(24), status, cw - 80));
    }

    /* The chosen card's page. */
    const float px = cx0 + cw + 40, pw = x + w - 56 - px, py = y + 120;
    float ly = py;
    auto paragraph = [&](const std::string &text, Color c, int lines) {
        for (const std::string &l : wrap(g, Font::Regular, ts(25), text, pw, std::size_t(lines)))
        {
            g.text_mid(Font::Regular, ts(25), px, ly + 16, c, Align::Left, l);
            ly += 34;
        }
        ly += 12;
    };
    auto row_box = [&](int index, const std::string &label, const std::string &value, Color value_color,
                       float indent = 0) {
        const bool on = p.inside && p.row == index;
        g.panel(px, ly, pw, 64, on ? rgba(0x1F63F0, 0.9f) : rgba(0x0E1C55, 0.55f), 0.8f, kR,
                on ? kIcy : rgba(0x3D5AB0, 0.7f), on ? 2.0f : 1.2f, on ? 8 : 0, on ? 0.2f : 0.0f);
        const float vw = value.empty() ? 0.0f : std::min(g.measure(Font::SemiBold, ts(26), value), pw * 0.45f);
        g.text_mid(Font::SemiBold, ts(28), px + 26 + indent, ly + 32, on ? kWhite : kSoft, Align::Left,
                   fit(g, Font::SemiBold, ts(28), label, pw - vw - 80 - indent));
        if (!value.empty())
            g.text_mid(Font::SemiBold, ts(26), px + pw - 26, ly + 32, value_color, Align::Right,
                       fit(g, Font::SemiBold, ts(26), value, pw * 0.45f));
        ly += 74;
    };

    if (p.mode == ModeCustom)
    {
        paragraph(tr("Play on a community's own server, as on a real Wii: pick its DNS server here. Porpoise sends "
                     "Wii games to it, and fixes the secure connection they ask for, so no game needs a cheat file."),
                  kSoft, 3);
        row_box(0, tr("Custom Server"), settings_ && settings_->online_custom ? tr("On") : tr("Off"),
                settings_ && settings_->online_custom ? good : hint);
        /* The servers: five at a time around the one in focus. */
        const int n = int(p.servers.size());
        constexpr int kShown = 4;
        int first = 0;
        if (p.inside && p.row >= 1 && p.row <= n)
            first = std::clamp(p.row - 1 - kShown + 1, 0, std::max(0, n - kShown));
        else if (p.inside && p.row > n)
            first = std::max(0, n - kShown);
        for (int i = first; i < n && i < first + kShown; ++i)
        {
            const auto &s = p.servers[std::size_t(i)];
            const bool chosen = settings_ && s.second == settings_->online_dns;
            const float row_y = ly;
            row_box(1 + i, s.first, s.second, chosen ? good : hint, 34);
            /* A dot before its name: filled for the one in use. */
            g.panel(px + 24, row_y + 23, 18, 18, chosen ? good : rgba(0x3D5AB0, 0.0f), 1, 9,
                    chosen ? good : rgba(0x8A96C8), 1.8f);
        }
        if (n > kShown)
            g.text_mid(Font::Regular, ts(22), px + pw, ly - 4, hint, Align::Right,
                       trf("{a}-{b} of {n}", {{"a", std::to_string(first + 1)},
                                              {"b", std::to_string(std::min(n, first + kShown))},
                                              {"n", std::to_string(n)}}));
        row_box(1 + n, tr("Add a Server\xE2\x80\xA6"), "", hint);
        std::string test_value;
        Color test_color = hint;
        if (p.test)
        {
            std::lock_guard<std::mutex> lock(p.test->m);
            if (p.test->done)
            {
                test_value = p.test->result > 0   ? trf("Answers in {ms} ms", {{"ms", std::to_string(p.test->ms)}})
                             : p.test->result == 0 ? tr("Answers, without Nintendo's")
                                                   : tr("No answer");
                test_color = p.test->result > 0 ? good : kDanger;
            }
        }
        const float test_y = ly;
        row_box(2 + n, tr("Test the Server"), test_value, test_color);
        if (p.test && !p.test->done)
            dots(g, px + pw - 110, test_y + 32, time_, still, kIcy);
        row_box(3 + n, tr("Secure Connection Fix"), settings_ && settings_->online_https ? tr("On") : tr("Off"),
                settings_ && settings_->online_https ? good : hint);
        /* What the row in focus does, or what just happened. */
        std::string note = p.message;
        if (note.empty() && p.test && p.test->done && p.inside && p.row == 2 + n)
        {
            std::lock_guard<std::mutex> lock(p.test->m);
            note = p.test->result > 0
                       ? trf("It sends Nintendo's login server to {address}: ready to play.",
                             {{"address", p.test->address}})
                   : p.test->result == 0
                       ? tr("It answered, but doesn't know Nintendo's servers. Check the address in its instructions.")
                       : trf("Nothing came back from {server}. Check the address, and that the console is online.",
                             {{"server", p.test->server}});
        }
        if (note.empty() && p.inside)
        {
            if (p.row == 0)
                note = tr("On, every Wii game goes online through the chosen server. Off, each goes its own way.");
            else if (p.row >= 1 && p.row <= n)
                note = tr("Cross uses this server. Triangle changes it, Square takes it off the list.");
            else if (p.row == 3 + n)
                note = tr("Wii games ask for a secure connection that only Nintendo's servers could make. The fix "
                          "turns it into a plain one, which community servers take. Leave it on unless your "
                          "server's instructions say otherwise.");
        }
        if (!note.empty())
            for (const std::string &l : wrap(g, Font::SemiBold, ts(23), note, pw, 3))
            {
                g.text_mid(Font::SemiBold, ts(23), px, ly + 14, kIcy, Align::Left, l);
                ly += 30;
            }
    }
    else if (p.mode == ModeWiiLink)
    {
        paragraph(tr("WiiLink WFC has a code for each game it runs. On its website, find your game, download the "
                     "code for your version, and save it as the game's ID with .txt (like RMCE01.txt) in "
                     "/data/porpoise/cheats. Porpoise turns it on for that game; it shows in the game's Cheats."),
                  kSoft, 5);
        paragraph(tr("Turn Custom Server off for WiiLink WFC: its code finds the server by itself."), hint, 2);
        g.text_mid(Font::Bold, ts(26), px, ly + 14, kWhite, Align::Left, tr("Your Wii games with a code"));
        ly += 46;
        if (coded.empty())
            paragraph(tr("None yet."), hint, 1);
        for (std::size_t i = 0; i < coded.size() && i < 6; ++i)
        {
            const Game &gm = *coded[i];
            g.panel(px, ly, pw, 54, rgba(0x0E1C55, 0.55f), 0.8f, kR, with_alpha(good, 0.6f), 1.2f);
            g.text_mid(Font::SemiBold, ts(26), px + 22, ly + 27, kWhite, Align::Left,
                       fit(g, Font::SemiBold, ts(26), gm.db_title.empty() ? gm.title : gm.db_title, pw - 200));
            g.text_mid(Font::Regular, ts(22), px + pw - 22, ly + 27, hint, Align::Right, gm.id);
            ly += 62;
        }
        if (coded.size() > 6)
            paragraph(plural((long long)(coded.size() - 6), "and 1 more", "and {n} more"), hint, 1);
    }
    else if (p.mode == ModeWiimmfi)
    {
        paragraph(tr("Wiimmfi is the biggest home for Mario Kart Wii online. On Dolphin, and so in Porpoise, it "
                     "needs two things done on a computer:"),
                  kSoft, 3);
        paragraph(tr("1. Patch your game with Wiimmfi's patcher (or play a pack made for it, like Retro Rewind)."),
                  kWhite, 2);
        paragraph(tr("2. Wiimmfi may also want your own Wii's system data (a NAND backup made with BootMii), "
                     "which Porpoise can't load yet."),
                  kWhite, 3);
        paragraph(tr("Then turn Custom Server off and start the patched game: it finds Wiimmfi by itself. A game "
                     "patched for Wiimmfi says so on its online screen."),
                  hint, 3);
    }
    else
    {
        paragraph(tr("The Wii's WiiConnect24 channels, back through WiiLink's servers: the weather, the news, polls "
                     "and more. The channels themselves are WADs from your own console, in your games."),
                  kSoft, 4);
        row_box(0, tr("WiiConnect24 Channels"), settings_ && settings_->wii_online ? tr("On") : tr("Off"),
                settings_ && settings_->wii_online ? good : hint);
        g.text_mid(Font::Bold, ts(26), px, ly + 14, kWhite, Align::Left, tr("Channels in your library"));
        ly += 46;
        if (channels.empty())
            paragraph(tr("None yet."), hint, 1);
        for (const std::string &c : channels)
        {
            g.text_mid(Font::SemiBold, ts(26), px + 22, ly + 16, kSoft, Align::Left, c);
            ly += 40;
        }
    }

    g.set_layer();
    drawing_dialog_ = true;
    if (!p.inside && hub_rows() > 0)
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Open"}}, {{Glyph::Circle, "Close"}}, "");
    else if (!p.inside)
        draw_prompts({{Glyph::DPad, "Choose"}}, {{Glyph::Circle, "Close"}}, "");
    else if (p.mode == ModeCustom && p.row >= 1 && p.row <= int(p.servers.size()))
        draw_prompts({{Glyph::Cross, "Use"}, {Glyph::Triangle, "Change"}, {Glyph::Square, "Remove"}},
                     {{Glyph::Circle, "Back"}}, "");
    else
        draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Select"}}, {{Glyph::Circle, "Back"}}, "");
    drawing_dialog_ = false;
}
} // namespace porpoise::ui

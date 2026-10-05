/* Porpoise UI - the first start: choose a look to begin with (Porpoise, Star
 * Cube or Revolution), shown live behind the cards; then the welcome and the
 * setup check. Settings > Interface > Reinitialize Porpoise brings it back. */

#include "ui_app.hpp"
#include "porpoise_pad.hpp"
#include "ui_app_common.hpp"
#include "ui_i18n.hpp"

#include <algorithm>
#include <cmath>

namespace porpoise::ui
{

using namespace look;
using namespace porpoise::pad;

namespace
{
/* The three looks offered, in order. */
constexpr int kWelcomeThemes[3] = {int(ThemeId::Porpoise), int(ThemeId::StarCube), int(ThemeId::Revolution)};

float ease(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

void App::start_welcome()
{
    screen_ = Screen::Welcome;
    welcome_time_ = 0;
    welcome_pick_ = 0;
    for (int i = 0; i < 3; ++i)
        if (settings_->ui_theme == kWelcomeThemes[i])
            welcome_pick_ = i;
    welcome_glide_ = float(welcome_pick_);
}

void App::restart_fresh()
{
    tab_ = Tab::Library;
    screen_ = Screen::Main;
    dialog_ = Dialog{};
    selected_ = 0;
    scroll_ = 0;
    on_rail_ = true;
    rail_ = 0;
    mc_wii_ = false;
    sc_home_ = true;
    sc_face_ = 0;
    sc_calendar_ = false;
    sc_zoom_ = 0;
    home_page_ = 0;
    home_scroll_ = 0;
    home_synced_ = false;
    theme_seen_ = settings_->ui_theme;
    apply_look();
    build_settings();
}

App::Action App::update_welcome(bool left, bool right, double dt)
{
    const bool calm = settings_->reduced_motion;
    welcome_time_ = std::min(10.0f, welcome_time_ + float(dt));
    welcome_glide_ = calm ? float(welcome_pick_) : smooth(welcome_glide_, float(welcome_pick_), dt, 10.0f);
    const int before = welcome_pick_;
    if (left && welcome_pick_ > 0)
        --welcome_pick_;
    if (right && welcome_pick_ < 2)
        ++welcome_pick_;
    if (welcome_pick_ != before)
    {
        /* The look changes behind the cards as you choose. */
        const int was = settings_->ui_theme;
        settings_->ui_theme = kWelcomeThemes[welcome_pick_];
        look_changed(was);
        apply_look();
        sfx(Sound::MenuScroll);
    }
    if (pressed(BtnCross) && welcome_time_ > 0.6f)
    {
        const int was = settings_->ui_theme;
        settings_->ui_theme = kWelcomeThemes[welcome_pick_];
        if (was != settings_->ui_theme)
            look_changed(was);
        settings_->setup_checked = true;
        settings_->save(settings_path_);
        restart_fresh();
        sfx(Sound::LaunchGame);
        show_setup_check(true); /* the welcome and what Porpoise can see */
        return Action::SettingsChanged;
    }
    return Action::None;
}

/* One look's card: a little picture of it, its name and what it is. */
void App::welcome_card(int which, float cx, float cy, float w, float h, float lift, float alpha, bool on, double time)
{
    Gfx &g = *g_;
    const float x = cx - w * 0.5f, y = cy - h * 0.5f - lift;
    const float t = settings_->reduced_motion ? 0.0f : float(time);
    g.panel(x, y, w, h, rgba(0x0B1440, 0.82f * alpha), 0.85f, kR, on ? rgba(0xBDF1FF, alpha) : rgba(0x4C6FD8, 0.8f * alpha),
            on ? 2.6f : 1.6f, on ? 16.0f : 0.0f, 0.15f);
    /* The picture. */
    const float px = x + 22, py = y + 22, pw = w - 44, ph = h * 0.50f;
    if (which == 0)
    {
        g.panel(px, py, pw, ph, rgba(0x0A1A5C, alpha), 0.55f, kR * 0.7f);
        for (int i = -1; i <= 1; ++i)
        {
            const float tw = i == 0 ? 92.0f : 70.0f, th = tw * 1.38f;
            const float tx = px + pw * 0.5f + float(i) * 104.0f - tw * 0.5f, ty = py + ph * 0.5f - th * 0.5f - 6;
            g.panel(tx, ty, tw, th, rgba(i == 0 ? 0x2F7BFF : 0x1E46C8, 0.85f * alpha), 0.6f, 8,
                    i == 0 ? rgba(0x9DEBFF, alpha) : rgba(0x4C8DFF, 0.8f * alpha), i == 0 ? 2.2f : 1.4f, i == 0 ? 8 : 0);
        }
        draw_mark(px + pw * 0.5f, py + ph * 0.5f - 6, 56, with_alpha(kWhite, alpha));
        g.panel(px + 20, py + ph - 26, pw - 40, 2, rgba(0x5CD3FF, 0.4f * alpha), 1, 0);
    }
    else if (which == 1)
    {
        g.panel(px, py, pw, ph, rgba(0x020208, alpha), 1, kR * 0.7f, rgba(0x2A2A50, 0.8f * alpha), 1.2f);
        draw_glass_cube(px + pw * 0.5f, py + ph * 0.5f, ph * 0.46f, 0.5f + t * 0.5f, 0.45f + 0.1f * std::sin(t * 0.7f),
                        0.1f, rgba(0x3A2FD0, 0.45f), rgba(0xD0C6FF), nullptr, alpha, on);
        for (int i = 0; i < 5; ++i)
        {
            const float fx = px + 30 + std::fmod(float(i) * 97.0f + t * 12.0f, pw - 60);
            const float fy = py + 28 + std::fmod(float(i) * 53.0f, ph - 56);
            draw_glass_cube(fx, fy, 16, t * 0.6f + float(i), t * 0.4f + float(i), 0, rgba(0x2A3FD8, 0.35f),
                            rgba(0x8FA0FF, 0.6f), nullptr, alpha * 0.6f, false);
        }
    }
    else
    {
        g.panel(px, py, pw, ph, rgba(0xF2F4F7, alpha), 1, kR * 0.7f);
        const float tw = (pw - 70) / 3, th = (ph - 60) / 2;
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 3; ++c)
            {
                const bool lit = r == 0 && c == 1;
                g.panel(px + 20 + float(c) * (tw + 15), py + 20 + float(r) * (th + 20), tw, th, rgba(0xFFFFFF, alpha), 1,
                        12, lit ? rgba(0x3DB8EC, alpha) : rgba(0xC6CCD4, alpha), lit ? 3.0f : 1.6f);
            }
    }
    /* Its name and what it is. */
    const Theme &theme_info = theme(kWelcomeThemes[which]);
    g.text_mid(Font::Bold, ts(40), cx, py + ph + 52, with_alpha(kWhite, alpha), Align::Center, tr(theme_info.name));
    const auto lines = wrap(g, Font::Regular, ts(24), tr(theme_info.about), w - 60, 4);
    float ly = py + ph + 92;
    for (const std::string &line : lines)
    {
        g.text(Font::Regular, ts(24), cx, ly, with_alpha(kSoft, alpha), Align::Center, line);
        ly += ts(24) * 1.35f;
    }
}

void App::draw_welcome(double time)
{
    Gfx &g = *g_;
    g.set_layer();
    g.set_tone(false);
    if (revolution())
        draw_room();
    else
        g.background();
    const bool calm = settings_->reduced_motion;
    const float in = calm ? 1.0f : ease(welcome_time_ / 0.6f);
    g.set_intro(intro_fade_, intro_dy_);

    /* The heading, in the theme's ink. */
    if (revolution())
        g.set_tone(true);
    g.set_layer(0, 24.0f * (1.0f - in), in);
    draw_mark(960, 118, 120, kWhite);
    g.text_mid(Font::ExtraBold, ts(58), 960, 222, kWhite, Align::Center, tr("Welcome to Porpoise"));
    g.text_mid(Font::SemiBold, ts(28), 960, 282, kSoft, Align::Center,
               tr("Porpoise plays your GameCube and Wii games on PS5, powered by Dolphin."));
    g.text_mid(Font::Regular, ts(26), 960, 326, kLavender, Align::Center,
               tr("Choose a look to start with. You can change it any time in Settings > Interface."));
    g.set_tone(false);

    /* The three cards; the chosen one lifts, and a light glides under it. */
    const float w = 500, h = 560, gap = 560, cy = 660;
    g.set_layer();
    g.blob(960 + (welcome_glide_ - 1.0f) * gap, cy + h * 0.5f + 26, w * 1.3f, 70, rgba(0x5CD3FF, 0.30f * in));
    for (int i = 0; i < 3; ++i)
    {
        const float arrive = calm ? 1.0f : ease((welcome_time_ - 0.15f - 0.12f * float(i)) / 0.5f);
        const bool on = i == welcome_pick_;
        const float lift = on ? 22.0f : 0.0f;
        const float x = 960 + float(i - 1) * gap;
        g.set_layer(0, 60.0f * (1.0f - arrive), arrive * (on ? 1.0f : 0.72f));
        welcome_card(i, x, cy + 30.0f * (1.0f - arrive), on ? w * 1.04f : w, on ? h * 1.04f : h, lift, 1.0f, on, time);
    }
    g.set_layer();
    if (revolution())
        g.set_tone(true);
    draw_prompts({{Glyph::DPad, "Choose"}, {Glyph::Cross, "Confirm"}}, {}, "");
    g.set_tone(false);
    g.set_intro(1, 0);
}

} // namespace porpoise::ui

/* Porpoise - launch arguments from a home screen forwarder: --rom <file> starts
 * that game directly instead of the library (docs/FORWARDER.md).
 * Copyright (C) 2026 Ruben (Project Porpoise)
 * SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <string>
#include <string_view>

namespace porpoise::forward
{
struct Args
{
    std::string rom;              /* --rom <file> or --rom=<file>; empty when not given */
    bool exit_after_game = false; /* --exit-after-game: close Porpoise when that game ends */
};

/* Unknown arguments are ignored (the desktop build's --fullscreen among them). */
inline Args parse(int argc, char **argv)
{
    Args result;
    if (argc <= 0 || argv == nullptr)
        return result;
    constexpr std::string_view rom_flag = "--rom";
    constexpr std::string_view rom_prefix = "--rom=";
    for (int i = 0; i < argc; ++i)
    {
        if (argv[i] == nullptr)
            break;
        const std::string_view arg{argv[i]};
        if (arg == "--exit-after-game")
            result.exit_after_game = true;
        else if (arg == rom_flag)
        {
            if (i + 1 < argc && argv[i + 1] != nullptr)
                result.rom = argv[++i];
        }
        else if (arg.starts_with(rom_prefix))
            result.rom = std::string{arg.substr(rom_prefix.size())};
    }
    return result;
}

/* An absolute path as it is; a relative one inside games_dir (Porpoise's
 * <data>/games), never above it: "" for a path with a ".." part. */
inline std::string resolve(std::string_view rom, std::string_view games_dir)
{
    while (!rom.empty() && (rom.back() == ' ' || rom.back() == '\r' || rom.back() == '\n'))
        rom.remove_suffix(1);
    if (rom.empty())
        return {};
    if (rom.front() == '/' || rom.front() == '\\' || (rom.size() > 1 && rom[1] == ':'))
        return std::string{rom}; /* "/mnt/usb0/...", or a Windows drive on the desktop build */
    for (std::string_view rest = rom; !rest.empty();)
    {
        const auto slash = rest.find_first_of("/\\");
        if (rest.substr(0, slash) == "..")
            return {};
        if (slash == std::string_view::npos)
            break;
        rest.remove_prefix(slash + 1);
    }
    std::string path{games_dir};
    if (!path.empty() && path.back() != '/')
        path += '/';
    path += rom;
    return path;
}
} // namespace porpoise::forward

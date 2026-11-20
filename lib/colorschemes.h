// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace term {

// A palette the settings offer: 0xRRGGBB values, the eight colours and then
// their bright versions.
struct ColorScheme
{
    // What terminal.conf stores.
    std::string_view name;
    std::string_view label;
    uint32_t background;
    uint32_t foreground;
    std::array<uint32_t, 16> colors;
};

// In the order the settings list them.
std::span<const ColorScheme> colorSchemes();
// Unknown names give the default, Tokyo Night.
const ColorScheme& colorScheme(std::string_view name);

}

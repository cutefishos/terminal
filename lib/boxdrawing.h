// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

#include "base/geometry.h"

namespace term {

// Box drawing (U+2500 to U+257F) and block elements (U+2580 to U+259F) drawn
// as whole device pixels, so lines meet across cells at any scale instead of
// leaving the seams and blur a font's glyphs have between rows.
class BoxDrawing
{
public:
    static bool covers(char32_t character) { return character >= 0x2500 && character <= 0x259f; }

    struct Shape
    {
        // Device pixels from the cell's top left, filled in the text colour.
        std::vector<cutefish::base::Rect> rects;
        // A shade fills the whole cell at this opacity instead.
        double shade = 0;
    };

    // `light` is the width of a light line in device pixels.
    const Shape& shape(char32_t character, int width, int height, int light);

private:
    std::map<std::tuple<char32_t, int, int, int>, Shape> m_cache;
};

}

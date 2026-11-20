// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "boxdrawing.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace term {

using cutefish::base::Rect;

// The four arms of U+2500 to U+257F, up, right, down and left: 0 none,
// 1 light, 2 heavy, 3 double; "d" marks the dashed, arc and diagonal
// characters drawn on their own.
static constexpr std::string_view kArms[] = {
    "0101", "0202", "1010", "2020", "d",    "d",    "d",    "d",    "d",    "d",    "d",    "d",    // 2500
    "0110", "0210", "0120", "0220", "0011", "0012", "0021", "0022", "1100", "1200", "2100", "2200", // 250C
    "1001", "1002", "2001", "2002", "1110", "1210", "2110", "1120", "2120", "2210", "1220", "2220", // 2518
    "1011", "1012", "2011", "1021", "2021", "2012", "1022", "2022", "0111", "0112", "0211", "0212", // 2524
    "0121", "0122", "0221", "0222", "1101", "1102", "1201", "1202", "2101", "2102", "2201", "2202", // 2530
    "1111", "1112", "1211", "1212", "2111", "1121", "2121", "2112", "2211", "1122", "1221", "2212", // 253C
    "1222", "2122", "2221", "2222", "d",    "d",    "d",    "d",    "0303", "3030", "0310", "0130", // 2548
    "0330", "0013", "0031", "0033", "1300", "3100", "3300", "1003", "3001", "3003", "1310", "3130", // 2554
    "3330", "1013", "3031", "3033", "0313", "0131", "0333", "1303", "3101", "3303", "1313", "3131", // 2560
    "3333", "d",    "d",    "d",    "d",    "d",    "d",    "d",    "0001", "1000", "0100", "0010", // 256C
    "0002", "2000", "0200", "0020", "0201", "1020", "0102", "2010",                                 // 2578
};

class Mask
{
public:
    Mask(int width, int height)
        : m_width(width)
        , m_height(height)
        , m_bits(size_t(width * height), false)
    {
    }

    void set(int x0, int y0, int x1, int y1, bool value = true)
    {
        x0 = std::clamp(x0, 0, m_width);
        x1 = std::clamp(x1, 0, m_width);
        y0 = std::clamp(y0, 0, m_height);
        y1 = std::clamp(y1, 0, m_height);
        for (int y = y0; y < y1; ++y)
            std::fill(m_bits.begin() + y * m_width + x0, m_bits.begin() + y * m_width + x1, value);
    }

    void point(int x, int y)
    {
        if (x >= 0 && y >= 0 && x < m_width && y < m_height)
            m_bits[size_t(y * m_width + x)] = true;
    }

    // Runs of set pixels a row at a time, runs of equal rows merged.
    std::vector<Rect> rects() const
    {
        std::vector<Rect> result;
        std::vector<Rect> open;
        for (int y = 0; y < m_height; ++y) {
            std::vector<Rect> row;
            for (int x = 0; x < m_width;) {
                if (!m_bits[size_t(y * m_width + x)]) {
                    ++x;
                    continue;
                }
                const int start = x;
                while (x < m_width && m_bits[size_t(y * m_width + x)])
                    ++x;
                row.push_back(Rect(start, y, x - start, 1));
            }
            std::vector<Rect> next;
            for (Rect& run : row) {
                auto above = std::find_if(open.begin(), open.end(), [&run](const Rect& rect) {
                    return rect.x() == run.x() && rect.width() == run.width();
                });
                if (above != open.end()) {
                    next.push_back(Rect(above->x(), above->y(), above->width(), above->height() + 1));
                    open.erase(above);
                } else {
                    next.push_back(run);
                }
            }
            result.insert(result.end(), open.begin(), open.end());
            open = std::move(next);
        }
        result.insert(result.end(), open.begin(), open.end());
        return result;
    }

private:
    int m_width;
    int m_height;
    std::vector<bool> m_bits;
};

struct Strokes
{
    int width;
    int height;
    int light;

    int thickness(int weight) const { return weight == 2 ? 2 * light : weight == 3 ? 3 * light : light; }
    // Where a stroke of this thickness starts, centred in the cell.
    int x(int thick) const { return (width - thick) / 2; }
    int y(int thick) const { return (height - thick) / 2; }
};

static void drawArms(Mask& mask, const Strokes& s, std::string_view arms)
{
    const int up = arms[0] - '0';
    const int right = arms[1] - '0';
    const int down = arms[2] - '0';
    const int left = arms[3] - '0';
    // The thickest arm across, which the arms along reach through.
    const int across = std::max(right, left) ? s.thickness(std::max(right, left)) : 0;
    const int along = std::max(up, down) ? s.thickness(std::max(up, down)) : 0;

    auto vertical = [&](int weight, bool top) {
        const int t = s.thickness(weight);
        const int reach = across ? across : t;
        if (top)
            mask.set(s.x(t), 0, s.x(t) + t, s.y(reach) + reach);
        else
            mask.set(s.x(t), s.y(reach), s.x(t) + t, s.height);
    };
    auto horizontal = [&](int weight, bool start) {
        const int t = s.thickness(weight);
        const int reach = along ? along : t;
        if (start)
            mask.set(0, s.y(t), s.x(reach) + reach, s.y(t) + t);
        else
            mask.set(s.x(reach), s.y(t), s.width, s.y(t) + t);
    };
    if (up)
        vertical(up, true);
    if (down)
        vertical(down, false);
    if (left)
        horizontal(left, true);
    if (right)
        horizontal(right, false);

    // A double line is a band of three light widths with its middle cleared,
    // the clearing stopping where the arms across begin, so the inner lines
    // turn the corner and single lines pass straight through.
    const int t = s.light;
    const int gapX = s.x(3 * t) + t;
    const int gapY = s.y(3 * t) + t;
    const bool doubleAcross = right == 3 || left == 3;
    const bool doubleAlong = up == 3 || down == 3;
    const int singleAcross = across && !doubleAcross ? across : 0;
    const int singleAlong = along && !doubleAlong ? along : 0;
    if (up == 3) {
        const int end = doubleAcross ? gapY + t : singleAcross ? s.y(singleAcross) : s.y(3 * t) + 3 * t;
        mask.set(gapX, 0, gapX + t, end, false);
    }
    if (down == 3) {
        const int start = doubleAcross ? gapY : singleAcross ? s.y(singleAcross) + singleAcross : s.y(3 * t);
        mask.set(gapX, start, gapX + t, s.height, false);
    }
    if (left == 3) {
        const int end = doubleAlong ? gapX + t : singleAlong ? s.x(singleAlong) : s.x(3 * t) + 3 * t;
        mask.set(0, gapY, end, gapY + t, false);
    }
    if (right == 3) {
        const int start = doubleAlong ? gapX : singleAlong ? s.x(singleAlong) + singleAlong : s.x(3 * t);
        mask.set(start, gapY, s.width, gapY + t, false);
    }
}

// U+2504 to U+250B and U+254C to U+254F: a line in `count` dashes.
static void drawDashes(Mask& mask, const Strokes& s, bool vertical, int weight, int count)
{
    const int t = s.thickness(weight);
    const int length = vertical ? s.height : s.width;
    for (int i = 0; i < count; ++i) {
        const int start = length * i / count;
        const int end = length * (i + 1) / count;
        // A gap of a third of the segment, at least one pixel, split around it.
        const int gap = std::max(1, (end - start) / 3);
        const int a = start + gap / 2;
        const int b = end - (gap - gap / 2);
        if (vertical)
            mask.set(s.x(t), a, s.x(t) + t, b);
        else
            mask.set(a, s.y(t), b, s.y(t) + t);
    }
}

// U+256D to U+2570: a light line turning a rounded corner, from the cell's
// middle to two of its edges.
static void drawArc(Mask& mask, const Strokes& s, char32_t character)
{
    const int t = s.light;
    const int x0 = s.x(t);
    const int y0 = s.y(t);
    const bool toRight = character == 0x256d || character == 0x2570;
    const bool toBottom = character == 0x256d || character == 0x256e;
    // The radius: as far as the nearer of the two edges the arc turns towards.
    const double cx = x0 + t / 2.0;
    const double cy = y0 + t / 2.0;
    const double radius = std::min(toRight ? s.width - cx : cx, toBottom ? s.height - cy : cy);
    const double centreX = toRight ? cx + radius : cx - radius;
    const double centreY = toBottom ? cy + radius : cy - radius;

    for (int y = 0; y < s.height; ++y) {
        for (int x = 0; x < s.width; ++x) {
            const double px = x + 0.5;
            const double py = y + 0.5;
            // Only the quarter between the two straight runs.
            if ((toRight ? px > centreX : px < centreX) || (toBottom ? py > centreY : py < centreY))
                continue;
            const double distance = std::hypot(px - centreX, py - centreY);
            if (std::abs(distance - radius) <= t / 2.0)
                mask.point(x, y);
        }
    }
    // Straight on to the edges past the curve.
    if (toBottom)
        mask.set(x0, int(std::floor(centreY)), x0 + t, s.height);
    else
        mask.set(x0, 0, x0 + t, int(std::ceil(centreY)));
    if (toRight)
        mask.set(int(std::floor(centreX)), y0, s.width, y0 + t);
    else
        mask.set(0, y0, int(std::ceil(centreX)), y0 + t);
}

// U+2571 to U+2573: corner to corner.
static void drawDiagonals(Mask& mask, const Strokes& s, bool rising, bool falling)
{
    const double half = s.light / 2.0 * std::hypot(s.width, s.height) / s.height;
    for (int y = 0; y < s.height; ++y) {
        const double py = y + 0.5;
        const double down = py * s.width / s.height;
        for (int x = 0; x < s.width; ++x) {
            const double px = x + 0.5;
            if ((falling && std::abs(px - down) <= half) || (rising && std::abs(px - (s.width - down)) <= half))
                mask.point(x, y);
        }
    }
}

// U+2580 to U+259F.
static void drawBlock(Mask& mask, BoxDrawing::Shape& shape, const Strokes& s, char32_t character)
{
    const int w = s.width;
    const int h = s.height;
    auto eighthsDown = [h](int n) { return h * n / 8; };
    auto eighthsAcross = [w](int n) { return w * n / 8; };
    if (character == 0x2580) {
        mask.set(0, 0, w, h / 2);
    } else if (character >= 0x2581 && character <= 0x2588) {
        mask.set(0, h - eighthsDown(int(character - 0x2580)), w, h);
    } else if (character >= 0x2589 && character <= 0x258f) {
        mask.set(0, 0, eighthsAcross(int(0x2590 - character)), h);
    } else if (character == 0x2590) {
        mask.set(w / 2, 0, w, h);
    } else if (character >= 0x2591 && character <= 0x2593) {
        shape.shade = double(character - 0x2590) / 4;
    } else if (character == 0x2594) {
        mask.set(0, 0, w, eighthsDown(1));
    } else if (character == 0x2595) {
        mask.set(w - eighthsAcross(1), 0, w, h);
    } else {
        // Quadrants, as bits: upper left 1, upper right 2, lower left 4, lower right 8.
        static constexpr uint8_t kQuadrants[] = {4, 8, 1, 13, 9, 7, 11, 2, 6, 14};
        const uint8_t quadrants = kQuadrants[character - 0x2596];
        if (quadrants & 1)
            mask.set(0, 0, w / 2, h / 2);
        if (quadrants & 2)
            mask.set(w / 2, 0, w, h / 2);
        if (quadrants & 4)
            mask.set(0, h / 2, w / 2, h);
        if (quadrants & 8)
            mask.set(w / 2, h / 2, w, h);
    }
}

const BoxDrawing::Shape& BoxDrawing::shape(char32_t character, int width, int height, int light)
{
    const auto key = std::make_tuple(character, width, height, light);
    if (const auto found = m_cache.find(key); found != m_cache.end())
        return found->second;
    if (m_cache.size() > 4096)
        m_cache.clear();

    Shape shape;
    Mask mask(std::max(1, width), std::max(1, height));
    const Strokes strokes{std::max(1, width), std::max(1, height), std::max(1, light)};
    if (character >= 0x2580) {
        drawBlock(mask, shape, strokes, character);
    } else if (const std::string_view arms = kArms[character - 0x2500]; arms != "d") {
        drawArms(mask, strokes, arms);
    } else if (character >= 0x2504 && character <= 0x250b) {
        const int index = int(character - 0x2504);
        drawDashes(mask, strokes, index % 4 >= 2, index % 2 ? 2 : 1, index < 4 ? 3 : 4);
    } else if (character >= 0x254c && character <= 0x254f) {
        const int index = int(character - 0x254c);
        drawDashes(mask, strokes, index >= 2, index % 2 ? 2 : 1, 2);
    } else if (character >= 0x256d && character <= 0x2570) {
        drawArc(mask, strokes, character);
    } else {
        drawDiagonals(mask, strokes, character != 0x2572, character != 0x2571);
    }
    shape.rects = mask.rects();
    return m_cache.emplace(key, std::move(shape)).first->second;
}

}

// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <vector>

#include "ui/painter.h"
#include "ui/view.h"
#include "ui/window.h"

#include "boxdrawing.h"
#include "cellfont.h"

using namespace cutefish;
using term::BoxDrawing;
using term::CellFont;

// Which device pixels of a cell a character covers.
static std::vector<std::string> pixels(BoxDrawing& boxes, char32_t character, int width, int height, int light)
{
    std::vector<std::string> rows(size_t(height), std::string(size_t(width), '.'));
    for (const base::Rect& rect : boxes.shape(character, width, height, light).rects) {
        EXPECT_GE(rect.x(), 0);
        EXPECT_GE(rect.y(), 0);
        EXPECT_LE(rect.right(), width);
        EXPECT_LE(rect.bottom(), height);
        for (int y = rect.y(); y < rect.bottom(); ++y) {
            for (int x = rect.x(); x < rect.right(); ++x) {
                EXPECT_EQ(rows[size_t(y)][size_t(x)], '.') << "overlap at " << x << "," << y;
                rows[size_t(y)][size_t(x)] = '#';
            }
        }
    }
    return rows;
}

TEST(BoxDrawingTest, LinesReachTheCellEdgesSoNeighboursMeet)
{
    BoxDrawing boxes;
    // A cell at 1.75 of a 8x17 cell, lines two pixels wide.
    const auto horizontal = pixels(boxes, U'─', 14, 30, 2);
    EXPECT_EQ(horizontal[14], std::string(14, '#'));
    EXPECT_EQ(horizontal[15], std::string(14, '#'));
    EXPECT_EQ(horizontal[13], std::string(14, '.'));
    EXPECT_EQ(horizontal[16], std::string(14, '.'));

    const auto vertical = pixels(boxes, U'│', 14, 30, 2);
    for (const std::string& row : vertical)
        EXPECT_EQ(row, "......##......");

    // The cross is both, the same columns and rows as its neighbours.
    const auto cross = pixels(boxes, U'┼', 14, 30, 2);
    for (int y = 0; y < 30; ++y) {
        const std::string expected = y == 14 || y == 15 ? std::string(14, '#') : "......##......";
        EXPECT_EQ(cross[size_t(y)], expected) << y;
    }
}

TEST(BoxDrawingTest, HeavyLinesAreTwiceTheLightOnes)
{
    BoxDrawing boxes;
    const auto heavy = pixels(boxes, U'━', 10, 20, 1);
    int rows = 0;
    for (const std::string& row : heavy)
        rows += row == std::string(10, '#');
    EXPECT_EQ(rows, 2);
}

TEST(BoxDrawingTest, DoubleLinesTurnCornersInside)
{
    BoxDrawing boxes;
    const auto corner = pixels(boxes, U'╔', 9, 9, 1);
    const std::vector<std::string> expected = {
        ".........",
        ".........",
        ".........",
        "...######",
        "...#.....",
        "...#.####",
        "...#.#...",
        "...#.#...",
        "...#.#...",
    };
    EXPECT_EQ(corner, expected);

    const auto both = pixels(boxes, U'═', 9, 9, 1);
    EXPECT_EQ(both[3], std::string(9, '#'));
    EXPECT_EQ(both[4], std::string(9, '.'));
    EXPECT_EQ(both[5], std::string(9, '#'));
}

TEST(BoxDrawingTest, BlocksFillTheirPart)
{
    BoxDrawing boxes;
    const auto full = pixels(boxes, U'█', 7, 15, 1);
    for (const std::string& row : full)
        EXPECT_EQ(row, std::string(7, '#'));
    const auto upper = pixels(boxes, U'▀', 7, 15, 1);
    EXPECT_EQ(upper[6], std::string(7, '#'));
    EXPECT_EQ(upper[7], std::string(7, '.'));
    EXPECT_EQ(boxes.shape(U'▒', 7, 15, 1).shade, 0.5);
}

TEST(BoxDrawingTest, EveryCharacterDrawsInsideItsCell)
{
    BoxDrawing boxes;
    for (const auto& [width, height, light] : {std::tuple{8, 17, 1}, {12, 25, 2}, {14, 30, 2}, {16, 34, 2}}) {
        for (char32_t character = 0x2500; character <= 0x259f; ++character) {
            const BoxDrawing::Shape& shape = boxes.shape(character, width, height, light);
            EXPECT_TRUE(!shape.rects.empty() || shape.shade > 0) << std::hex << uint32_t(character);
            pixels(boxes, character, width, height, light);
        }
    }
}

namespace {

class Canvas : public ui::View
{
public:
    explicit Canvas(std::function<void(ui::Painter&)> draw)
        : m_draw(std::move(draw))
    {
    }

protected:
    void paint(ui::Painter& painter) override { m_draw(painter); }

private:
    std::function<void(ui::Painter&)> m_draw;
};

}

TEST(CellFontTest, CellsAreWholeDevicePixels)
{
    CellFont font;
    font.setFont("Monospace", 10);
    for (double scale : {1.0, 1.25, 1.5, 1.75, 2.0}) {
        font.setScale(scale);
        const term::CellMetrics& cell = font.metrics();
        EXPECT_GT(cell.width, 0);
        EXPECT_GT(cell.height, cell.baseline);
        EXPECT_LT(cell.baseline + cell.underline + cell.lineWidth, cell.height + 1) << scale;
    }
}

TEST(CellFontTest, ShapedClustersStayBounded)
{
    CellFont font;
    font.setFont("Monospace", 10);

    // Distinct ideographs, a hundred a call, as output of every code point would draw them.
    auto draw = [&font](ui::Painter& painter) {
        char32_t character = 0x4E00;
        std::vector<std::u32string> texts;
        for (int call = 0; call < 200; ++call) {
            texts.clear();
            for (int i = 0; i < 100; ++i)
                texts.emplace_back(1, character++);
            std::vector<ui::GlyphCluster> clusters;
            for (const std::u32string& text : texts)
                clusters.push_back({text, 0});
            font.draw(painter, CellFont::Regular, ui::Color::rgb(0), 12, clusters);
        }
    };

    ui::Window window;
    window.resize({100, 60});
    window.setContentView(std::make_unique<Canvas>(draw));
    window.show();
    window.grab();

    EXPECT_LE(font.cachedClusters(), 8192u + 100u);
}

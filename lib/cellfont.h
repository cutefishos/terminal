// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "ui/glyph_run.h"

namespace cutefish::ui {
class Painter;
}

namespace term {

// A cell of the grid in whole device pixels, so every column and row is the
// same size on the screen and lines drawn across cells meet.
struct CellMetrics
{
    int width = 1;
    int height = 1;
    // From the cell's top to the baseline.
    int baseline = 1;
    // Below the baseline, to the line's top.
    int underline = 1;
    // Above the baseline, to the line's top.
    int strikeout = 1;
    int lineWidth = 1;
};

// The terminal's font, its cells and its glyphs at one window scale.
class CellFont
{
public:
    enum Style {
        Regular = 0,
        Bold = 1,
        Italic = 2,
    };

    CellFont();
    ~CellFont();

    // Monospaced families, for the settings.
    static std::vector<std::string> fixedPitchFamilies();

    void setFont(const std::string& family, double pointSize);
    void setScale(double scale);
    double scale() const { return m_scale; }
    const CellMetrics& metrics() const { return m_metrics; }

    // Clusters of one style and colour, their origins at device x from the
    // painter's origin, on a baseline in device pixels.
    void draw(cutefish::ui::Painter& painter, int style, cutefish::ui::Color color, double baseline,
              std::span<const cutefish::ui::GlyphCluster> clusters);

    // Shaped clusters kept for all styles.
    size_t cachedClusters() const;

private:
    cutefish::ui::FontSpec spec(int style) const;
    void updateMetrics();

    std::string m_family = "Monospace";
    double m_pointSize = 10;
    double m_scale = 1;
    CellMetrics m_metrics;
    std::unique_ptr<cutefish::ui::GlyphRenderer> m_renderers[4];
};

}

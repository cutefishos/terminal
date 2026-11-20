// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "cellfont.h"

#include <algorithm>
#include <cmath>

namespace ui = cutefish::ui;

namespace term {

// Output can name any of the million code points and endless combining
// sequences, so a renderer's cache starts over past this many clusters.
static constexpr size_t kMaximumCachedClusters = 8192;

CellFont::CellFont()
{
    updateMetrics();
}

CellFont::~CellFont() = default;

std::vector<std::string> CellFont::fixedPitchFamilies()
{
    std::vector<std::string> families;
    for (const ui::FontFamily& family : ui::fontFamilies()) {
        if (family.fixedPitch)
            families.push_back(family.name);
    }
    return families;
}

void CellFont::setFont(const std::string& family, double pointSize)
{
    if (family == m_family && pointSize == m_pointSize)
        return;
    m_family = family;
    m_pointSize = pointSize;
    for (auto& renderer : m_renderers)
        renderer.reset();
    updateMetrics();
}

void CellFont::setScale(double scale)
{
    if (scale <= 0 || scale == m_scale)
        return;
    m_scale = scale;
    for (auto& renderer : m_renderers)
        renderer.reset();
    updateMetrics();
}

ui::FontSpec CellFont::spec(int style) const
{
    return ui::FontSpec{m_family, m_pointSize, bool(style & Bold), bool(style & Italic)};
}

void CellFont::updateMetrics()
{
    // Metrics are hinted only at scale 1, as the glyphs are.
    const ui::FontMetrics font(spec(Regular), m_scale, m_scale == 1);
    CellMetrics metrics;
    metrics.width = std::max(1, int(std::lround(font.horizontalAdvanceF("M") * m_scale)));
    metrics.baseline = std::max(1, int(std::lround(font.ascentF() * m_scale)));
    metrics.height = std::max(metrics.baseline + 1, int(std::lround(font.heightF() * m_scale)));
    metrics.lineWidth = std::max(1, int(std::lround(font.lineWidthF() * m_scale)));
    metrics.underline = std::clamp(int(std::lround(font.underlinePosF() * m_scale)), 1,
                                   std::max(1, metrics.height - metrics.baseline - metrics.lineWidth));
    metrics.strikeout = std::max(1, int(std::lround(font.strikeOutPosF() * m_scale)));
    m_metrics = metrics;
}

void CellFont::draw(ui::Painter& painter, int style, ui::Color color, double baseline,
                    std::span<const ui::GlyphCluster> clusters)
{
    std::unique_ptr<ui::GlyphRenderer>& renderer = m_renderers[style & 3];
    if (!renderer)
        renderer = std::make_unique<ui::GlyphRenderer>(spec(style), m_scale);
    else if (renderer->cachedClusters() > kMaximumCachedClusters)
        renderer->clearCache();
    renderer->draw(painter, color, baseline, clusters);
}

size_t CellFont::cachedClusters() const
{
    size_t count = 0;
    for (const auto& renderer : m_renderers) {
        if (renderer)
            count += renderer->cachedClusters();
    }
    return count;
}

}

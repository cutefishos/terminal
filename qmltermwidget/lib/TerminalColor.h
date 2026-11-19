// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TERMINALCOLOR_H
#define TERMINALCOLOR_H

#include "ui/geometry.h"

#include "CharacterColor.h"

namespace Konsole
{

using Color = cutefish::ui::Color;

inline Color toColor(const Rgba& color)
{
    return {color.r, color.g, color.b, color.a};
}

} // namespace Konsole

#endif // TERMINALCOLOR_H

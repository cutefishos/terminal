// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace term {

// A link written out in terminal text: a URL with a scheme, a file path, or,
// written without a scheme, "www...." or an e-mail address. `begin` and
// `end` are byte offsets of what was written; `url` is what opens, with
// "http://" or "mailto:" added where the text has none. A path opens
// relative to the shell's directory.
struct Link
{
    size_t begin = 0;
    size_t end = 0;
    std::string url;
    bool path = false;

    bool isValid() const { return end > begin; }
};

// The link that covers the byte at `offset`, if any.
Link linkAt(std::string_view text, size_t offset);

// Each URL and path in the text, as [begin, end) byte offsets, left to right.
std::vector<std::pair<size_t, size_t>> linkMatches(std::string_view text);

}

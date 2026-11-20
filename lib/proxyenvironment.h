// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <optional>
#include <string>

namespace term {

// The session proxy for a program started in the terminal: the
// org.gnome.system.proxy settings of the GSettings keyfile, the store the
// session's proxy settings write to, as the variables curl, wget and most
// other clients read. Every variable the proxy owns is in the result, set or
// removed (no value), so a stale one inherited from the terminal never leaks.
std::map<std::string, std::optional<std::string>> proxyEnvironment();
// The same from a keyfile's text, for the tests.
std::map<std::string, std::optional<std::string>> proxyEnvironment(const std::string& keyfile);

}

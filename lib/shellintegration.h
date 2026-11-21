// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#pragma once

#include <string>

#include "pty.h"

namespace term {

// Starts bash, zsh or fish with the scripts in `directory` that mark prompts
// (OSC 133) and report the working directory (OSC 7), keeping the user's own
// startup files: bash in POSIX mode reading the script through ENV, zsh with
// ZDOTDIR pointing at it, fish finding it first in XDG_DATA_DIRS. Other
// programs, and bash running a command or already in POSIX mode, start as
// they are. Returns whether a shell was integrated.
bool integrateShell(const std::string& directory, Pty::Options& options);

}

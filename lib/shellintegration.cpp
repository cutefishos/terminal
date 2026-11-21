// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "shellintegration.h"

#include <unistd.h>

namespace term {

static std::string baseName(const std::string& path)
{
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static bool readable(const std::string& path)
{
    return ::access(path.c_str(), R_OK) == 0;
}

static bool integrateBash(const std::string& directory, Pty::Options& options)
{
    const std::string script = directory + "/bash/cutefish-terminal.bash";
    if (!readable(script))
        return false;
    std::vector<std::string> arguments{options.arguments.front(), "--posix"};
    // "1" tells the script it was started this way, then the options it
    // has to honour itself as it reads the startup files.
    std::string inject = "1";
    std::string rcfile;
    for (size_t i = 1; i < options.arguments.size(); ++i) {
        const std::string& argument = options.arguments[i];
        if (argument == "--posix") {
            return false;
        } else if (argument == "--norc" || argument == "--noprofile") {
            inject += " " + argument;
        } else if (argument == "--rcfile" || argument == "--init-file") {
            if (i + 1 < options.arguments.size())
                rcfile = options.arguments[++i];
        } else if (argument.size() > 1 && argument[0] == '-' && argument[1] != '-') {
            // A command is never interactive.
            if (argument.find('c') != std::string::npos)
                return false;
            arguments.push_back(argument);
        } else if (argument == "-" || argument == "--") {
            arguments.insert(arguments.end(), options.arguments.begin() + long(i), options.arguments.end());
            break;
        } else {
            arguments.push_back(argument);
        }
    }
    options.arguments = std::move(arguments);
    options.copies["CUTEFISH_TERMINAL_BASH_ENV"] = "ENV";
    options.environment["ENV"] = script;
    options.environment["CUTEFISH_TERMINAL_BASH_INJECT"] = inject;
    if (!rcfile.empty())
        options.environment["CUTEFISH_TERMINAL_BASH_RCFILE"] = rcfile;
    return true;
}

static bool integrateZsh(const std::string& directory, Pty::Options& options)
{
    const std::string zdotdir = directory + "/zsh";
    if (!readable(zdotdir + "/.zshenv"))
        return false;
    options.copies["CUTEFISH_TERMINAL_ZSH_ZDOTDIR"] = "ZDOTDIR";
    options.environment["ZDOTDIR"] = zdotdir;
    return true;
}

static bool integrateFish(const std::string& directory, Pty::Options& options)
{
    if (!readable(directory + "/fish/vendor_conf.d/cutefish-terminal-shell-integration.fish"))
        return false;
    options.environment["CUTEFISH_TERMINAL_SHELL_INTEGRATION_XDG_DIR"] = directory;
    // What XDG_DATA_DIRS means when it is not set.
    options.prefixes["XDG_DATA_DIRS"] = {directory, "/usr/local/share:/usr/share"};
    return true;
}

bool integrateShell(const std::string& directory, Pty::Options& options)
{
    if (directory.empty() || options.arguments.empty())
        return false;
    const std::string shell = baseName(options.arguments.front());
    if (shell == "bash")
        return integrateBash(directory, options);
    if (shell == "zsh")
        return integrateZsh(directory, options);
    if (shell == "fish")
        return integrateFish(directory, options);
    return false;
}

}

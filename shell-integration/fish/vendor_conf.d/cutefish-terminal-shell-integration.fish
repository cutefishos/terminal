# SPDX-FileCopyrightText: 2024 Mitchell Hashimoto, Ghostty contributors
# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: MIT
#
# Marks the prompt, the command line and its output (OSC 133) and reports
# the working directory (OSC 7) for cutefish-terminal. fish reads it from
# vendor_conf.d because the terminal puts this directory first in
# XDG_DATA_DIRS; it takes the directory out again before anything else.
# Adapted from the reference terminal's fish integration; see the README.

function __cft_restore_xdg_data_dirs
    if not set -q CUTEFISH_TERMINAL_SHELL_INTEGRATION_XDG_DIR
        return
    end
    if not set -q XDG_DATA_DIRS
        return
    end

    # As a list, to take ours out of it.
    set --function --path xdg_data_dirs "$XDG_DATA_DIRS"
    if set --function index (contains --index "$CUTEFISH_TERMINAL_SHELL_INTEGRATION_XDG_DIR" $xdg_data_dirs)
        set --erase --function xdg_data_dirs[$index]
    end
    if set -q xdg_data_dirs[1]
        set --global --export --unpath XDG_DATA_DIRS "$xdg_data_dirs"
    else
        set --erase --global XDG_DATA_DIRS
    end
    set --erase CUTEFISH_TERMINAL_SHELL_INTEGRATION_XDG_DIR
end

function __cft_exit
    functions -e __cft_restore_xdg_data_dirs
    functions -e __cft_exit
    exit 0
end

__cft_restore_xdg_data_dirs

status --is-interactive || __cft_exit

# Set up at the first prompt, after the user's configuration and anything
# else that changes the prompt.
function __cft_setup --on-event fish_prompt
    functions -e __cft_setup

    set -l fish_major 0
    set -l fish_minor 0
    if set -q version[1]
        set -l fish_version (string match -r '(\d+)\.(\d+)' -- $version[1])
        if set -q fish_version[2]; and test -n "$fish_version[2]"
            set fish_major "$fish_version[2]"
        end
        if set -q fish_version[3]; and test -n "$fish_version[3]"
            set fish_minor "$fish_version[3]"
        end
    end

    # fish 4.1 and later moves its cursor itself for clicks it is sent.
    set -g __cft_prompt_start "\e]133;A\a"
    if test "$fish_major" -gt 4; or test "$fish_major" -eq 4 -a "$fish_minor" -ge 1
        set -g __cft_prompt_start "\e]133;A;click_events=1\a"
    end

    function __cft_mark_prompt_start --on-event fish_prompt --on-event fish_posterror
        # A command whose end never arrived ends here.
        if test "$__cft_prompt_state" != prompt-start
            echo -en "\e]133;D\a"
        end
        set --global __cft_prompt_state prompt-start
        echo -en $__cft_prompt_start
    end

    function __cft_mark_output_start --on-event fish_preexec
        set --global __cft_prompt_state pre-exec
        echo -en "\e]133;C\a"
    end

    function __cft_mark_output_end --on-event fish_postexec
        set --global __cft_prompt_state post-exec
        echo -en "\e]133;D;$status\a"
    end

    # fish reports the directory only to terminals it knows.
    function __cft_report_directory --on-variable PWD
        if status --is-command-substitution || set -q INSIDE_EMACS
            return
        end
        printf \e\]7\;file://%s%s\a $hostname (string escape --style=url $PWD)
    end

    # The terminal redraws no prompt on a resize; fish does.
    set --global fish_handle_reflow 1

    __cft_mark_prompt_start
    __cft_report_directory
end

__cft_exit

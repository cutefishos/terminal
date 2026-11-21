# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: MIT
#
# Marks the prompt, the command line and its output (OSC 133) and reports
# the working directory (OSC 7) for cutefish-terminal.
#
# The terminal starts bash in POSIX mode with ENV naming this file, so bash
# reads it in place of its startup files; it leaves POSIX mode and reads
# those files itself, as bash would have.

[[ $- == *i* ]] || builtin return 0

if [[ -n ${CUTEFISH_TERMINAL_BASH_INJECT-} ]]; then
    builtin declare __cft_flags=$CUTEFISH_TERMINAL_BASH_INJECT
    builtin unset ENV CUTEFISH_TERMINAL_BASH_INJECT
    if [[ -n ${CUTEFISH_TERMINAL_BASH_ENV+x} ]]; then
        builtin export ENV=$CUTEFISH_TERMINAL_BASH_ENV
        builtin unset CUTEFISH_TERMINAL_BASH_ENV
    fi

    builtin set +o posix
    builtin shopt -u inherit_errexit 2>/dev/null

    # POSIX mode names ~/.sh_history when the environment names no history file.
    if [[ $HISTFILE == "$HOME/.sh_history" && $(builtin declare -p HISTFILE 2>/dev/null) != "declare -x"* ]]; then
        HISTFILE=$HOME/.bash_history
    fi

    # The startup files bash reads itself, as INVOCATION in bash(1) lists them.
    if builtin shopt -q login_shell; then
        if [[ $__cft_flags != *--noprofile* ]]; then
            [[ -r /etc/profile ]] && builtin source /etc/profile
            for __cft_file in "$HOME/.bash_profile" "$HOME/.bash_login" "$HOME/.profile"; do
                if [[ -r $__cft_file ]]; then
                    builtin source "$__cft_file"
                    break
                fi
            done
        fi
    elif [[ $__cft_flags != *--norc* ]]; then
        # Distributions build bash with their own system-wide file.
        for __cft_file in /etc/bash.bashrc /etc/bash/bashrc /etc/bashrc; do
            if [[ -r $__cft_file ]]; then
                builtin source "$__cft_file"
                break
            fi
        done
        __cft_file=${CUTEFISH_TERMINAL_BASH_RCFILE:-$HOME/.bashrc}
        [[ -r $__cft_file ]] && builtin source "$__cft_file"
    fi
    builtin unset __cft_file __cft_flags CUTEFISH_TERMINAL_BASH_RCFILE
fi

# PS0 needs bash 4.4.
if (( BASH_VERSINFO[0] < 4 || (BASH_VERSINFO[0] == 4 && BASH_VERSINFO[1] < 4) )); then
    builtin return 0
fi

__cft_prompted=
__cft_directory=

__cft_return() {
    builtin return "$1"
}

__cft_prompt() {
    builtin local status=$1

    # A command line ran since the last prompt: its status ends its output.
    [[ -n $__cft_prompted ]] && builtin printf '\e]133;D;%s\a' "$status"
    __cft_prompted=1

    # A new prompt, on a line of its own; the terminal moves the cursor
    # within the command line with arrow keys.
    builtin printf '\e]133;A;cl=line\a'

    if [[ $__cft_directory != "$PWD" ]]; then
        __cft_directory=$PWD
        builtin printf '\e]7;kitty-shell-cwd://%s%s\a' "${HOSTNAME//[[:cntrl:]]/}" "${PWD//[[:cntrl:]]/}"
    fi

    # The prompt as prompt text, the rest of the line as the command; again
    # whenever something else has set the prompt anew.
    if [[ $PS1 != *'133;P;k=i'* ]]; then
        PS1='\[\e]133;P;k=i\a\]'$PS1'\[\e]133;B\a\]'
    fi
    if [[ $PS2 != *'133;P;k=s'* ]]; then
        PS2='\[\e]133;P;k=s\a\]'$PS2'\[\e]133;B\a\]'
    fi
    # Output starts as the command runs.
    if [[ $PS0 != *'133;C'* ]]; then
        PS0=$PS0'\e]133;C\a'
    fi
}

# Last among the prompt commands, with the status of the command line; a
# shell that inherits the variable without these functions stays quiet.
if [[ ${PROMPT_COMMAND[*]-} != *__cft_prompt* ]]; then
    if [[ $(builtin declare -p PROMPT_COMMAND 2>/dev/null) == "declare -a"* ]]; then
        PROMPT_COMMAND+=('__cft_prompt "$__cft_status" 2>/dev/null')
        PROMPT_COMMAND=('__cft_status=$?; __cft_return "$__cft_status" 2>/dev/null' "${PROMPT_COMMAND[@]}")
    else
        PROMPT_COMMAND='__cft_status=$?; __cft_return "$__cft_status" 2>/dev/null; '${PROMPT_COMMAND:+$PROMPT_COMMAND$'\n'}'__cft_prompt "$__cft_status" 2>/dev/null'
    fi
fi

# SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
# SPDX-License-Identifier: MIT
#
# zsh reads this first because cutefish-terminal points ZDOTDIR here. It puts
# the user's ZDOTDIR back, reads the user's .zshenv in its place, and in an
# interactive shell loads the integration. Everything is quoted so that
# aliases a .zshenv defines cannot change it.

if [[ -n ${CUTEFISH_TERMINAL_ZSH_ZDOTDIR+x} ]]; then
    'builtin' 'export' ZDOTDIR="$CUTEFISH_TERMINAL_ZSH_ZDOTDIR"
    'builtin' 'unset' 'CUTEFISH_TERMINAL_ZSH_ZDOTDIR'
else
    'builtin' 'unset' 'ZDOTDIR'
fi

{
    # An unset ZDOTDIR means HOME to zsh.
    'builtin' 'typeset' __cft_file=${ZDOTDIR-$HOME}/.zshenv
    [[ ! -r $__cft_file ]] || 'builtin' 'source' '--' "$__cft_file"
} always {
    if [[ -o 'interactive' ]]; then
        # The directory of this file.
        'builtin' 'typeset' __cft_file=${${(%):-%x}:A:h}/cutefish-terminal-integration
        [[ ! -r $__cft_file ]] || 'builtin' 'source' '--' "$__cft_file"
    fi
    'builtin' 'unset' '__cft_file'
}

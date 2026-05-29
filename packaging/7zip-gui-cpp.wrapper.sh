#!/bin/bash
# Launcher for file-manager integration: ensure DISPLAY/XAUTHORITY and log invocations.
set -euo pipefail

LIBDIR="/usr/lib/7zip-gui-cpp"
DEFAULT_REAL="${LIBDIR}/7zip-gui-cpp"
LOG=/tmp/7zip-gui-cpp.log

log() {
    printf '%s %s %s\n' "$(date -Iseconds)" "$$" "$*" >>"$LOG"
}

resolve_real_bin() {
    local candidate=""
    for candidate in \
        "${SEVENZIP_GUI_REAL:-}" \
        "${DEFAULT_REAL}" \
        "$(dirname "$(readlink -f "$0" 2>/dev/null || echo "$0")")/7zip-gui-cpp" \
        "$(command -v 7zip-gui-cpp-real 2>/dev/null || true)"
    do
        [ -n "$candidate" ] || continue
        if [ -x "$candidate" ] && file "$candidate" 2>/dev/null | grep -q ELF; then
            printf '%s' "$candidate"
            return 0
        fi
    done
    return 1
}

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ] && command -v loginctl >/dev/null 2>&1; then
    uid="$(id -u)"
    while read -r session _; do
        [ -n "$session" ] || continue
        su="$(loginctl show-session "$session" -p User --value 2>/dev/null || true)"
        [ "$su" = "$uid" ] || continue
        d="$(loginctl show-session "$session" -p Display --value 2>/dev/null || true)"
        if [ -n "$d" ] && [ "$d" != "''" ]; then
            export DISPLAY="$d"
            break
        fi
    done < <(loginctl list-sessions --no-legend 2>/dev/null || true)
fi

if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    export DISPLAY="${DISPLAY:-:0}"
fi

if [ -z "${XAUTHORITY:-}" ] && [ -f "${HOME}/.Xauthority" ]; then
    export XAUTHORITY="${HOME}/.Xauthority"
fi

if [ -z "${XDG_RUNTIME_DIR:-}" ] && [ -d "/run/user/$(id -u)" ]; then
    export XDG_RUNTIME_DIR="/run/user/$(id -u)"
fi

log "DISPLAY=${DISPLAY:-} WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-} XAUTHORITY=${XAUTHORITY:-} argv=$(printf '%q ' "$@")"

REAL_BIN="$(resolve_real_bin || true)"
if [ -z "${REAL_BIN:-}" ] || [ ! -x "$REAL_BIN" ]; then
    log "ERROR: real binary not found (expected ${DEFAULT_REAL})"
    if command -v zenity >/dev/null 2>&1; then
        zenity --error --text="7-Zip File Manager is not installed correctly (missing ${DEFAULT_REAL})." 2>/dev/null || true
    elif command -v kdialog >/dev/null 2>&1; then
        kdialog --error "7-Zip File Manager is not installed correctly (missing ${DEFAULT_REAL})." 2>/dev/null || true
    fi
    exit 127
fi

log "REAL_BIN=$REAL_BIN"
exec "$REAL_BIN" "$@"

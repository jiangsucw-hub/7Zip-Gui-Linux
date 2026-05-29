#!/bin/bash
# Thunar UCA helper: add selected paths to a new archive.
args=()
for f in "$@"; do
    [ -n "$f" ] || continue
    args+=("$f")
done
if [ "${#args[@]}" -eq 0 ]; then
    exec /usr/bin/7zip-gui-cpp --add
else
    exec /usr/bin/7zip-gui-cpp --add "${args[@]}"
fi

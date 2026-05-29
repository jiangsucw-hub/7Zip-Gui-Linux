#!/bin/bash
# Thunar UCA helper: extract each selected archive (handles spaces in paths).
for f in "$@"; do
    [ -n "$f" ] || continue
    /usr/bin/7zip-gui-cpp --extract-here "$f"
done

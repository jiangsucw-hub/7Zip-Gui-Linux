#!/bin/bash
# Thunar UCA helper: test each selected archive.
for f in "$@"; do
    [ -n "$f" ] || continue
    /usr/bin/7zip-gui-cpp --test "$f"
done

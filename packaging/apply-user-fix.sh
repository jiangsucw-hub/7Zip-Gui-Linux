#!/bin/bash
# Fix Thunar UCA + optional local wrapper without full deb rebuild.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build/7zip-gui-cpp"
UCA="${HOME}/.config/Thunar/uca.xml"

echo "==> Patch Thunar uca.xml (commands + submenu)"
python3 "${ROOT}/packaging/patch-thunar-uca.py" "$UCA" 2>/dev/null || true
[ -f /etc/xdg/Thunar/uca.xml ] && sudo python3 "${ROOT}/packaging/patch-thunar-uca.py" /etc/xdg/Thunar/uca.xml || true

if [ -x "$BUILD" ]; then
  echo "==> Install user wrapper ~/.local/bin/7zip-gui-cpp-dev"
  mkdir -p "${HOME}/.local/bin"
  cat > "${HOME}/.local/bin/7zip-gui-cpp-dev" <<EOF
#!/bin/bash
export DISPLAY="\${DISPLAY:-:0}"
export XAUTHORITY="\${XAUTHORITY:-\${HOME}/.Xauthority}"
export XDG_RUNTIME_DIR="\${XDG_RUNTIME_DIR:-/run/user/\$(id -u)}"
exec "$BUILD" "\$@"
EOF
  chmod +x "${HOME}/.local/bin/7zip-gui-cpp-dev"
  echo "To test: 7zip-gui-cpp-dev --extract-here /path/to/file.7z"
fi

echo "==> Restart Thunar: thunar --quit; thunar &"
echo "Done. Log: /tmp/7zip-gui-cpp.log (when using system wrapper)"

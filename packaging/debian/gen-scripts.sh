#!/usr/bin/env bash
# Regenerate postinst/prerm from 7zip-gui (avoid double-replacing 7zip-gui -> 7zip-gui-cpp-cpp).
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
SRC="/home/bwz/code/7zip-gui/deb-root/DEBIAN"

sed -e 's|/usr/bin/7zip-gui"|/usr/bin/7zip-gui-cpp"|g' \
    -e 's|7zip-gui --|7zip-gui-cpp --|g' \
    -e 's|exec 7zip-gui |exec 7zip-gui-cpp |g' \
    -e 's|Icon-Name=7zip-gui|Icon-Name=7zip-gui-cpp|g' \
    -e 's|<icon>7zip-gui</icon>|<icon>7zip-gui-cpp</icon>|g' \
    -e "s|'7zip-gui'|'7zip-gui-cpp'|g" \
    -e 's|# 7zip-gui postinst|# 7zip-gui-cpp postinst|' \
    -e '/^SCRIPT_DIR=/d' \
    -e 's|48x48/apps/7zip-gui\.png|scalable/apps/7zip-gui-cpp.svg|' \
    "${SRC}/postinst" > "${DIR}/postinst"

sed -e "s|'7zip-gui'|'7zip-gui-cpp'|g" \
    -e '/rm -rf \/usr\/share\/7zip-gui\/vendor/d' \
    "${SRC}/prerm" > "${DIR}/prerm"
chmod 755 "${DIR}/postinst" "${DIR}/prerm"

#!/usr/bin/env bash
# Build 7zip-gui-cpp .deb (amd64, dynamic Qt from system packages).
set -euo pipefail

ROOT="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)"
VERSION="${VERSION:-0.1.0}"
REV="${REV:-70}"
DEB_ROOT="${ROOT}/debian-staging"
OUT_DEB="${ROOT}/7zip-gui-cpp_${VERSION}-${REV}_amd64.deb"
ICON_SRC_SVG="${ROOT}/packaging/icons/7zip-gui-cpp.svg"
ICON_SRC_PNG="${ROOT}/packaging/icons/icons-480.png"
ICON_FALLBACK_PNG_1="${ROOT}/packaging/icons/icons8-7-zip-480-whitebg-preview.png"
ICON_FALLBACK_PNG_2="${ROOT}/packaging/icons/icons8-7-zip-480.png"

if [ ! -f "${ICON_SRC_PNG}" ] && [ -f "${ICON_FALLBACK_PNG_1}" ]; then
    ICON_SRC_PNG="${ICON_FALLBACK_PNG_1}"
elif [ ! -f "${ICON_SRC_PNG}" ] && [ -f "${ICON_FALLBACK_PNG_2}" ]; then
    ICON_SRC_PNG="${ICON_FALLBACK_PNG_2}"
fi

echo "==> Configure & build"
cmake -S "${ROOT}" -B "${ROOT}/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${ROOT}/build" -j"$(nproc 2>/dev/null || echo 2)"

echo "==> Stage files"
rm -rf "${DEB_ROOT}"
mkdir -p "${DEB_ROOT}"

DESTDIR="${DEB_ROOT}" cmake --install "${ROOT}/build"

echo "==> Icons (hicolor)"
if [ -f "${ICON_SRC_SVG}" ]; then
    install -Dm644 "${ICON_SRC_SVG}" "${DEB_ROOT}/usr/share/icons/hicolor/scalable/apps/7zip-gui-cpp.svg"
fi
if [ -f "${ICON_SRC_PNG}" ]; then
    install -Dm644 "${ICON_SRC_PNG}" "${DEB_ROOT}/usr/share/icons/hicolor/256x256/apps/7zip-gui-cpp.png"
fi
install -Dm644 "${ROOT}/packaging/icons/NOTICE" "${DEB_ROOT}/usr/share/doc/7zip-gui-cpp/icons.NOTICE"

if [ -f "${ICON_SRC_SVG}" ] && command -v rsvg-convert >/dev/null 2>&1; then
    for size in 16 22 24 32 48 64 128 256; do
        mkdir -p "${DEB_ROOT}/usr/share/icons/hicolor/${size}x${size}/apps"
        rsvg-convert -w "${size}" -h "${size}" "${ICON_SRC_SVG}" \
            -o "${DEB_ROOT}/usr/share/icons/hicolor/${size}x${size}/apps/7zip-gui-cpp.png"
    done
elif [ -f "${ICON_SRC_PNG}" ] && command -v convert >/dev/null 2>&1; then
    for size in 16 22 24 32 48 64 128 256; do
        mkdir -p "${DEB_ROOT}/usr/share/icons/hicolor/${size}x${size}/apps"
        convert -background none -resize "${size}x${size}" "${ICON_SRC_PNG}" \
            "${DEB_ROOT}/usr/share/icons/hicolor/${size}x${size}/apps/7zip-gui-cpp.png"
    done
else
    echo "Note: install librsvg2-bin or imagemagick to generate all icon sizes."
fi

INT="${ROOT}/packaging/integration"
echo "==> File manager context menus"
mkdir -p "${DEB_ROOT}/usr/share/nemo/actions"
cp "${INT}"/nemo/*.nemo_action "${DEB_ROOT}/usr/share/nemo/actions/"
mkdir -p "${DEB_ROOT}/usr/share/nautilus/scripts"
cp "${INT}"/nautilus-scripts/* "${DEB_ROOT}/usr/share/nautilus/scripts/"
chmod 755 "${DEB_ROOT}/usr/share/nautilus/scripts/"*
mkdir -p "${DEB_ROOT}/usr/share/caja/actions"
cp "${INT}"/caja/*.caja_action "${DEB_ROOT}/usr/share/caja/actions/"
mkdir -p "${DEB_ROOT}/usr/share/kservices5/ServiceMenus"
cp "${INT}"/dolphin/*.desktop "${DEB_ROOT}/usr/share/kservices5/ServiceMenus/"
mkdir -p "${DEB_ROOT}/usr/share/nautilus-python/extensions"
cp "${INT}"/nautilus/7zip_gui_cpp.py "${DEB_ROOT}/usr/share/nautilus-python/extensions/"
mkdir -p "${DEB_ROOT}/usr/share/7zip-gui-cpp"
cp "${ROOT}/packaging/patch-thunar-uca.py" "${DEB_ROOT}/usr/share/7zip-gui-cpp/"

mkdir -p "${DEB_ROOT}/DEBIAN"
sed "s/@VERSION@/${VERSION}/; s/@REV@/${REV}/" "${ROOT}/packaging/debian/control" > "${DEB_ROOT}/DEBIAN/control"
cp "${ROOT}/packaging/debian/postinst" "${ROOT}/packaging/debian/prerm" "${DEB_ROOT}/DEBIAN/"
chmod 755 "${DEB_ROOT}/DEBIAN/postinst" "${DEB_ROOT}/DEBIAN/prerm"

echo "==> Build package: ${OUT_DEB}"
dpkg-deb --build --root-owner-group "${DEB_ROOT}" "${OUT_DEB}"
echo "Done: ${OUT_DEB}"
ls -lh "${OUT_DEB}"

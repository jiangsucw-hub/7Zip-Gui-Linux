#!/usr/bin/env python3
# Nautilus / GNOME Files: 7-Zip File Manager context menu (requires python3-nautilus)
import os
from gi.repository import GObject, Nautilus, GLib

ARCHIVE_SUFFIXES = (
    ".7z", ".zip", ".tar", ".gz", ".bz2", ".xz", ".rar", ".iso", ".tgz",
    ".tbz2", ".txz", ".deb", ".rpm", ".jar", ".apk", ".zst", ".lz4",
    ".lz", ".lzo", ".lzh", ".lzma", ".arj", ".cab", ".wim", ".cpio",
    ".ar", ".z", ".dmg",
    ".tar.gz", ".tar.bz2", ".tar.xz",
)


def _path(file_info):
    return file_info.get_location().get_path()


def _is_archive(file_info):
    name = file_info.get_name().lower()
    return any(name.endswith(s) for s in ARCHIVE_SUFFIXES)


def _spawn(argv):
    GLib.spawn_async(
        None,
        argv,
        None,
        GLib.SpawnFlags.SEARCH_PATH | GLib.SpawnFlags.DO_NOT_REAP_CHILD,
        None,
        None,
    )


class SevenZipMenuProvider(GObject.GObject, Nautilus.MenuProvider):
    def _item(self, name, label, tip, callback, files):
        item = Nautilus.MenuItem.new(name=name, label=label, tip=tip)
        item.connect("activate", callback, files)
        return item

    def _paths(self, files):
        return [_path(f) for f in files]

    def _on_extract(self, _menu, files):
        for p in self._paths(files):
            _spawn(["7zip-gui-cpp", "--extract-here", p])

    def _on_test(self, _menu, files):
        for p in self._paths(files):
            _spawn(["7zip-gui-cpp", "--test", p])

    def _on_add(self, _menu, files):
        _spawn(["7zip-gui-cpp", "--add"] + self._paths(files))

    def get_file_items(self, files):
        if not files:
            return []

        submenu = Nautilus.Menu()
        if all(_is_archive(f) for f in files):
            submenu.append_item(
                self._item(
                    "7zip-cpp-test",
                    "Test archive",
                    "Test archive integrity",
                    self._on_test,
                    files,
                )
            )
            submenu.append_item(
                self._item(
                    "7zip-cpp-extract",
                    "Extract Here",
                    "Extract archive to current folder",
                    self._on_extract,
                    files,
                )
            )
        if len(files) >= 1:
            submenu.append_item(
                self._item(
                    "7zip-cpp-add",
                    "Add to archive...",
                    "Create a new archive",
                    self._on_add,
                    files,
                )
            )

        root = Nautilus.MenuItem.new(
            name="7zip-cpp-menu",
            label="7-Zip File Manager",
            tip="7-Zip archive manager",
            submenu=submenu,
        )
        # Ensure root menu entry uses our packaged icon name.
        for prop in ("icon", "icon-name"):
            try:
                root.set_property(prop, "7zip-gui-cpp")
                break
            except Exception:
                continue
        return [root]


def load_module():
    return SevenZipMenuProvider()

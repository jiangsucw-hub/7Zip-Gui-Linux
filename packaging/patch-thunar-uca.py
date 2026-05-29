#!/usr/bin/env python3
"""Ensure Thunar UCA entries use 7-Zip File Manager submenu, English labels, and working commands."""
import os
import sys
import xml.etree.ElementTree as ET

SUBMENU = "7-Zip File Manager"
LABELS = {
    "extract": "Extract Here",
    "test": "Test archive",
    "add": "Add to archive...",
}


def _kind(cmd: str) -> str | None:
    if "--extract-here" in cmd or "thunar-extract-here" in cmd:
        return "extract"
    if "--test" in cmd or "thunar-test" in cmd:
        return "test"
    if "--add" in cmd or "thunar-add" in cmd:
        return "add"
    return None


def _command_for(kind: str) -> str:
    lib = "/usr/lib/7zip-gui-cpp"
    helpers = {
        "extract": f"{lib}/thunar-extract-here.sh",
        "test": f"{lib}/thunar-test.sh",
        "add": f"{lib}/thunar-add.sh",
    }
    helper = helpers.get(kind, "")
    if helper and os.path.isfile(helper) and os.access(helper, os.X_OK):
        return f"{helper} %F"
    flags = {"extract": "--extract-here", "test": "--test", "add": "--add"}
    return f"/usr/bin/7zip-gui-cpp {flags[kind]} %F"


def patch_file(path: str) -> None:
    try:
        tree = ET.parse(path)
    except (OSError, ET.ParseError) as e:
        print(f"skip {path}: {e}", file=sys.stderr)
        return
    root = tree.getroot()
    changed = False
    for action in root.findall("action"):
        cmd_el = action.find("command")
        if cmd_el is None or not cmd_el.text:
            continue
        cmd = cmd_el.text
        if not any(
            token in cmd
            for token in ("7zip-gui-cpp", "thunar-extract-here", "thunar-test", "thunar-add")
        ):
            continue
        kind = _kind(cmd)
        if not kind:
            continue
        new_cmd = _command_for(kind)
        if cmd_el.text != new_cmd:
            cmd_el.text = new_cmd
            changed = True
        name_el = action.find("name")
        sub_el = action.find("submenu")
        icon_el = action.find("icon")
        if sub_el is not None:
            if sub_el.text != SUBMENU:
                sub_el.text = SUBMENU
                changed = True
        else:
            sub_el = ET.SubElement(action, "submenu")
            sub_el.text = SUBMENU
            changed = True
        if name_el is not None and name_el.text != LABELS[kind]:
            name_el.text = LABELS[kind]
            changed = True
        if icon_el is not None and icon_el.text != "7zip-gui-cpp":
            icon_el.text = "7zip-gui-cpp"
            changed = True
    if changed:
        tree.write(path, encoding="UTF-8", xml_declaration=True)
        print(f"patched {path}")


def main() -> None:
    paths = list(sys.argv[1:]) or [
        "/etc/xdg/Thunar/uca.xml",
        f"{os.environ.get('HOME', '')}/.config/Thunar/uca.xml",
    ]
    for p in paths:
        if p:
            patch_file(p)


if __name__ == "__main__":
    main()

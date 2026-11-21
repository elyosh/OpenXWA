#!/usr/bin/env python3
"""Writes tools/mod_importer/stock_keys.c from the game's default keyboard
bindings (input.keyboard in resources/remaster/config.yaml).

Run it again when the default bindings change:

    python3 tools/mod_importer/stock_keys.py
"""

import pathlib

import yaml

ROOT = pathlib.Path(__file__).resolve().parents[2]

# The names of XWAU's Active Cockpit files for the keys that the game's
# configuration spells in another way. Letters, digits and function keys are
# the same in both. A key that XWAU has no name for is left out.
NAMES = {
    "Left": "LEFT_ARROW",
    "Right": "RIGHT_ARROW",
    "Up": "UP_ARROW",
    "Down": "DOWN_ARROW",
    "Tab": "TAB",
    "Return": "ENTER",
    "Space": "SPACE",
    "Backspace": "BACKSPACE",
    ",": "COMMA",
    ".": "PERIOD",
    "=": "PLUS",
    "-": "MINUS",
    ";": ";",
    "'": "'",
}


def key_name(key):
    if key in NAMES:
        return NAMES[key]
    if len(key) == 1 and key.isalnum():
        return key.upper()
    if key[0] == "F" and key[1:].isdigit():
        return key
    return None


def main():
    config = yaml.safe_load((ROOT / "resources/remaster/config.yaml").read_text())
    rows = []
    for action, bindings in config["input"]["keyboard"].items():
        for binding in bindings or []:
            name = key_name(str(binding["key"]))
            if name is None:
                continue
            modifiers = binding.get("modifiers") or []
            prefix = "".join(m.upper() + "+" for m in ("ctrl", "shift", "alt") if m in modifiers)
            rows.append((prefix + name, action))
    rows.sort()
    lines = [
        "/* The game's default keyboard bindings, for the import of XWAU's Active",
        " * Cockpit buttons: each key, spelled as XwauKeyAction wants it, and the",
        " * input action it does. Written by stock_keys.py from",
        " * resources/remaster/config.yaml. Don't edit it by hand. */",
        "",
        '#include "stock_tables.h"',
        "",
        "const XwauKeyAction g_importStockKeys[] = {",
    ]
    for key, action in rows:
        escaped = key.replace("\\", "\\\\").replace('"', '\\"')
        lines.append('\t{ "%s", "%s" },' % (escaped, action))
    lines += [
        "};",
        "",
        "const int g_importStockKeyCount = (int)(sizeof g_importStockKeys / sizeof g_importStockKeys[0]);",
        "",
    ]
    (ROOT / "tools/mod_importer/stock_keys.c").write_text("\n".join(lines))
    print("%d keys" % len(rows))


if __name__ == "__main__":
    main()

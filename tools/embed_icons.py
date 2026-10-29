#!/usr/bin/env python3
"""Embed LVUI-owned, static SVG design assets in a deterministic C++ table."""
import argparse
import os
from pathlib import Path
import re
import xml.etree.ElementTree as ET

MAX_BYTES = 65536
MAX_ASSETS = 4096
NAME = re.compile(r"[A-Za-z][A-Za-z0-9_.-]{0,127}\Z")
SVG = "http://www.w3.org/2000/svg"


def collect(folder):
    if not folder.is_dir():
        raise ValueError("icon resource directory does not exist")
    files = sorted(folder.glob("*.svg"), key=lambda path: path.name)
    if not files or len(files) > MAX_ASSETS:
        raise ValueError("icon resource count must be within 1..4096")
    entries = []
    for path in files:
        if path.is_symlink() or not path.is_file() or not NAME.fullmatch(path.stem):
            raise ValueError("invalid built-in icon file: " + path.name)
        raw = path.read_bytes()
        if not raw or len(raw) > MAX_BYTES or b"\0" in raw:
            raise ValueError("invalid icon byte length: " + path.name)
        data = raw.decode("utf-8")
        if "<!DOCTYPE" in data.upper() or "<!ENTITY" in data.upper():
            raise ValueError("document declarations are not allowed: " + path.name)
        root = ET.fromstring(data)
        if root.tag != "{" + SVG + "}svg":
            raise ValueError("icon root must be an SVG: " + path.name)
        for element in root.iter():
            tag = element.tag.rsplit("}", 1)[-1].lower()
            if tag in {"script", "style", "foreignobject", "filter", "image", "use", "animate", "set"}:
                raise ValueError("unsupported SVG element in " + path.name)
            for key, value in element.attrib.items():
                if key.rsplit("}", 1)[-1].lower() == "href" or "url(" in value.lower():
                    raise ValueError("external SVG references are not allowed: " + path.name)
        entries.append((path.stem, data))
    return entries


def render(entries):
    lines = ["#pragma once", "#include <array>", "#include <string_view>",
             "namespace lvui::detail {", "struct BuiltinIconAsset final { std::string_view name, svg; };",
             "inline constexpr std::array<BuiltinIconAsset, " + str(len(entries)) + "> builtin_icon_assets{{"]
    for index, (name, svg) in enumerate(entries):
        delimiter = "LVUI" + str(index)
        if ")" + delimiter + '"' in svg:
            raise ValueError("SVG collides with generated C++ delimiter: " + name)
        lines.append('    {"' + name + '", R"' + delimiter + '(' + svg + ')' + delimiter + '"},')
    lines.extend(["}};", "} // namespace lvui::detail", ""])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    content = render(collect(args.input))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists() and args.output.read_text(encoding="utf-8") == content:
        return
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as output:
        output.write(content)
    os.replace(temporary, args.output)


if __name__ == "__main__":
    main()

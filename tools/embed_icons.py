#!/usr/bin/env python3
"""Embed LVUI-owned, static SVG design assets in a deterministic C++ table."""
import argparse
import json
import math
import os
from pathlib import Path
import re
import xml.etree.ElementTree as ET

MAX_BYTES = 65536
MAX_ASSETS = 4096
NAME = re.compile(r"[A-Za-z][A-Za-z0-9_.-]{0,127}\Z")
SVG = "http://www.w3.org/2000/svg"
TOKENS = Path(__file__).resolve().parents[1] / "catalog/lvui-style-tokens.json"


def resolve_svg_tokens(svg, catalog):
    """Resolve explicitly bound paint attributes while retaining the authored SVG."""
    collections = {c["id"]: c for c in catalog["collections"]}
    by_id = {v["id"]: v for v in catalog["variables"]}
    by_name = {collections[v["collectionId"]]["name"] + "/" + v["name"]: v
               for v in catalog["variables"]}
    if len(by_id) != len(catalog["variables"]) or len(by_name) != len(catalog["variables"]):
        raise ValueError("duplicate style token identity")

    def color(name):
        token = by_name.get(name)
        seen = set()
        while token is not None:
            if token["id"] in seen or token["type"] != "COLOR":
                raise ValueError("invalid or cyclic icon color token: " + name)
            seen.add(token["id"])
            mode = collections[token["collectionId"]]["defaultModeId"]
            value = token["valuesByMode"].get(mode)
            if isinstance(value, dict) and value.get("type") == "VARIABLE_ALIAS":
                token = by_id.get(value.get("id"))
                continue
            if not isinstance(value, dict) or set(value) != {"r", "g", "b", "a"}:
                break
            if not all(isinstance(x, (float, int)) and not isinstance(x, bool) and math.isfinite(x) and 0 <= x <= 1 for x in value.values()) or value["a"] != 1:
                break
            return "#" + "".join(f"{int(value[k] * 255 + .5):02X}" for k in ("r", "g", "b"))
        raise ValueError("unknown or non-opaque icon color token: " + name)

    def tag(match):
        text = match.group()
        for paint in ("fill", "stroke"):
            binding = re.findall(r'(?<![\w:-])data-lvui-' + paint + r'-token\s*=\s*["\']([^"\']+)["\']', text)
            if len(binding) > 1:
                raise ValueError("duplicate icon token binding")
            if binding:
                attribute = re.compile(r'(?<![\w:-])(' + paint + r'\s*=\s*)(["\'])([^"\']*)(\2)')
                text, count = attribute.subn(lambda m: m.group(1) + m.group(2) + color(binding[0]) + m.group(4), text)
                if count != 1:
                    raise ValueError("icon token requires exactly one " + paint + " attribute")
        return text

    return re.sub(r'<[A-Za-z][^>]*>', tag, svg)


def collect(folder, catalog=None):
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
        entries.append((path.stem, resolve_svg_tokens(data, catalog) if catalog is not None else data))
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
    parser.add_argument("--tokens", type=Path, default=TOKENS)
    args = parser.parse_args()
    catalog = json.loads(args.tokens.read_text(encoding="utf-8"))
    content = render(collect(args.input, catalog))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists() and args.output.read_text(encoding="utf-8") == content:
        return
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as output:
        output.write(content)
    os.replace(temporary, args.output)


if __name__ == "__main__":
    main()

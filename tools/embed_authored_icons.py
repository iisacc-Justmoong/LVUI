#!/usr/bin/env python3
"""Embed LVUI's locally owned authored SVG snapshot and its lookup aliases."""

import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import xml.etree.ElementTree as ET

MAX_ASSETS = 4096
MAX_SVG = 1024 * 1024
MAX_TOTAL = 16 * 1024 * 1024
SVG = "{http://www.w3.org/2000/svg}svg"
URL = re.compile(r"url\s*\(\s*([\"']?)(.*?)\1\s*\)", re.IGNORECASE)


def checked_svg(path, expected):
    if path.is_symlink() or not path.is_file():
        raise ValueError("Invalid authored icon file: " + path.name)
    raw = path.read_bytes()
    if not raw or len(raw) > MAX_SVG or b"\0" in raw:
        raise ValueError("Invalid authored icon size: " + path.name)
    if expected["bytes"] != len(raw) or expected["sha256"] != hashlib.sha256(raw).hexdigest():
        raise ValueError("Authored icon differs from its snapshot: " + path.name)
    text = raw.decode("utf-8")
    if "<!DOCTYPE" in text.upper() or "<!ENTITY" in text.upper():
        raise ValueError("Authored icon document declarations are forbidden: " + path.name)
    root = ET.fromstring(text)
    if root.tag != SVG:
        raise ValueError("Authored icon root is not SVG: " + path.name)
    for element in root.iter():
        tag = element.tag.rsplit("}", 1)[-1].lower()
        if tag in {"script", "style", "foreignobject", "set"} or tag.startswith("animate"):
            raise ValueError("Active SVG content is forbidden: " + path.name)
        # Filters are retained verbatim for fidelity. The host explicitly reports
        # an unsupported result until it can render them; it must not ignore one.
        for key, value in element.attrib.items():
            if key.rsplit("}", 1)[-1].lower() == "href":
                if not value.startswith("#") and not (tag == "image" and value.startswith("data:image/png;base64,")):
                    raise ValueError("External SVG reference is forbidden: " + path.name)
            if re.search(r"url\s*\(", value, re.IGNORECASE):
                matches = list(URL.finditer(value))
                if len(matches) != len(re.findall(r"url\s*\(", value, re.IGNORECASE)) or any(not item.group(2).startswith("#") for item in matches):
                    raise ValueError("External SVG URL is forbidden: " + path.name)
    return raw, text


def cpp_string(value):
    return json.dumps(value, ensure_ascii=False)


def checked_fallbacks(folder, manifest, originals):
    if manifest.get("schema") != 1 or not isinstance(manifest.get("entries"), list):
        raise ValueError("Invalid authored icon fallback manifest")
    entries = manifest["entries"]
    if len(entries) > MAX_ASSETS:
        raise ValueError("Too many authored icon fallbacks")
    names = [entry["name"] for entry in entries]
    if names != sorted(set(names)):
        raise ValueError("Authored icon fallback names must be sorted and unique")
    filenames = []
    result = []
    total = 0
    for entry in entries:
        name = entry["name"]
        filename = entry["file"]
        if name not in originals or entry["source_sha256"] != originals[name]:
            raise ValueError("Authored icon fallback does not match its original")
        if filename != name + ".svg" or Path(filename).name != filename:
            raise ValueError("Invalid authored icon fallback filename")
        filenames.append(filename)
        raw, text = checked_svg(folder / filename, entry)
        total += len(raw)
        if total > MAX_TOTAL:
            raise ValueError("Authored icon fallback snapshot exceeds its aggregate budget")
        root = ET.fromstring(text)
        children = list(root)
        width, height = entry["width"], entry["height"]
        if (type(width) is not int or type(height) is not int or
                not 1 <= width <= 512 or not 1 <= height <= 512 or
                root.attrib != {"width": str(width), "height": str(height),
                                "viewBox": f"0 0 {width} {height}"} or
                len(children) != 1 or children[0].tag != "{http://www.w3.org/2000/svg}image" or
                len(children[0]) != 0):
            raise ValueError("Invalid authored icon fallback geometry")
        attrs = children[0].attrib
        prefix = "data:image/png;base64,"
        if (set(attrs) != {"width", "height", "href"} or attrs["width"] != str(width) or
                attrs["height"] != str(height) or not attrs["href"].startswith(prefix)):
            raise ValueError("Invalid authored icon fallback image")
        try:
            png = base64.b64decode(attrs["href"][len(prefix):], validate=True)
        except ValueError as error:
            raise ValueError("Invalid authored icon fallback encoding") from error
        if (len(png) < 33 or png[:8] != b"\x89PNG\r\n\x1a\n" or png[12:16] != b"IHDR" or
                struct.unpack(">II", png[16:24]) != (width, height) or
                hashlib.sha256(png).hexdigest() != entry["png_sha256"]):
            raise ValueError("Invalid authored icon fallback PNG")
        references = entry.get("references", [])
        if not isinstance(references, list) or len(references) > 16:
            raise ValueError("Invalid authored icon reference list")
        reference_names = set()
        for reference in references:
            filename = reference["file"]
            if (not isinstance(filename, str) or Path(filename).name != filename or
                    not filename.endswith(".png") or filename in reference_names):
                raise ValueError("Invalid authored icon reference filename")
            reference_names.add(filename)
            path = folder / filename
            if path.is_symlink() or not path.is_file():
                raise ValueError("Invalid authored icon reference file")
            reference_png = path.read_bytes()
            total += len(reference_png)
            if total > MAX_TOTAL:
                raise ValueError("Authored icon fallback snapshot exceeds its aggregate budget")
            reference_width, reference_height = reference["width"], reference["height"]
            icon_size = reference["icon_size"]
            if (type(reference_width) is not int or type(reference_height) is not int or type(icon_size) is not int or
                    not 1 <= icon_size <= min(reference_width, reference_height) or
                    max(reference_width, reference_height) > 512 or len(reference_png) < 33 or
                    len(reference_png) > MAX_SVG or reference_png[:8] != b"\x89PNG\r\n\x1a\n" or
                    reference_png[12:16] != b"IHDR" or
                    struct.unpack(">II", reference_png[16:24]) != (reference_width, reference_height) or
                    hashlib.sha256(reference_png).hexdigest() != reference["sha256"]):
                raise ValueError("Invalid authored icon reference PNG")
        result.append((name, text))
    if sorted(path.name for path in folder.glob("*.svg")) != sorted(filenames):
        raise ValueError("Authored icon fallback files do not match the manifest")
    return result


def checked_design_icons(folder, manifest):
    if manifest.get("schema") != 1 or not isinstance(manifest.get("files"), list):
        raise ValueError("Invalid design icon manifest")
    files = sorted(manifest["files"], key=lambda item: item["name"])
    if not files or len(files) > MAX_ASSETS:
        raise ValueError("Invalid design icon inventory size")
    names = [item["name"] for item in files]
    if names != sorted(set(names)) or any(
            not isinstance(name, str) or not name or Path(name).name != name or "\0" in name for name in names):
        raise ValueError("Invalid or duplicate design icon name")
    if sorted(path.stem for path in folder.glob("*.svg")) != names:
        raise ValueError("Design icon files do not match the manifest")
    result = []
    total = 0
    for item in files:
        raw, text = checked_svg(folder / (item["name"] + ".svg"), item)
        total += len(raw)
        if total > MAX_TOTAL:
            raise ValueError("Design icon snapshot exceeds its aggregate budget")
        root = ET.fromstring(text)
        if (root.attrib.get("width") != "18" or root.attrib.get("height") != "18" or
                root.attrib.get("viewBox", "").split() != ["0", "0", "18", "18"]):
            raise ValueError("Invalid design icon viewport: " + item["name"])
        result.append((item["name"], raw, text))
    return result


def generate(folder, manifest, fallback_folder=None, fallback_manifest=None,
             design_folder=None, design_manifest=None):
    if manifest.get("schema") != 1 or not isinstance(manifest.get("files"), list):
        raise ValueError("Invalid authored icon manifest")
    files = sorted(manifest["files"], key=lambda item: item["name"])
    if not files or len(files) > MAX_ASSETS:
        raise ValueError("Invalid authored icon inventory size")
    names = [item["name"] for item in files]
    if names != sorted(set(names)):
        raise ValueError("Authored icon names must be unique")
    if any(not isinstance(name, str) or not name or Path(name).name != name or "\0" in name for name in names):
        raise ValueError("Invalid authored icon name")
    found = sorted(path.stem for path in folder.glob("*.svg"))
    if found != names:
        raise ValueError("Authored icon files do not match the manifest")
    assets = []
    total = 0
    for item in files:
        raw, text = checked_svg(folder / (item["name"] + ".svg"), item)
        total += len(raw)
        if total > MAX_TOTAL:
            raise ValueError("Authored icon snapshot exceeds its aggregate budget")
        assets.append((item["name"], raw, text))
    if (fallback_folder is None) != (fallback_manifest is None):
        raise ValueError("Both authored icon fallback inputs are required")
    fallbacks = checked_fallbacks(fallback_folder, fallback_manifest,
                                  {item["name"]: item["sha256"] for item in files}) if fallback_folder else []
    if (design_folder is None) != (design_manifest is None):
        raise ValueError("Both design icon inputs are required")
    design = checked_design_icons(design_folder, design_manifest) if design_folder else []
    resource_aliases = manifest.get("resource_aliases")
    name_aliases = manifest.get("name_aliases")
    if not isinstance(resource_aliases, dict) or not isinstance(name_aliases, dict):
        raise ValueError("Invalid authored alias tables")
    canonical = set(names)
    for alias, target in resource_aliases.items():
        if not alias or alias in canonical or target not in canonical:
            raise ValueError("Invalid authored resource alias")
    for alias, target in name_aliases.items():
        if not alias or not alias.isascii() or alias.lower() != alias or target not in canonical | set(resource_aliases):
            raise ValueError("Invalid authored name alias")
    def ascii_lower(value):
        return "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in value)
    for name, _ in fallbacks:
        resolved = name.strip(" \t\r\n\f\v")
        if ascii_lower(resolved).endswith(".svg"):
            resolved = resolved[:-4]
        resolved = resolved.replace("/", "")
        resolved = name_aliases.get(ascii_lower(resolved), resolved)
        resolved = resource_aliases.get(resolved, resolved)
        if resolved != name:
            raise ValueError("Authored icon fallback name resolves to a different original: " + name)
    lines = ["// Generated from LVUI-owned authored resources; do not edit.",
             '#include "Media/AuthoredIconData.h"', "namespace lvui::detail {"]
    for index, (_, raw, text) in enumerate(assets):
        if len(raw) <= 60000:
            delimiter = "LVUI" + str(index)
            if ")" + delimiter + '"' in text:
                raise ValueError("SVG raw-string delimiter collision")
            lines.append(f'static constexpr std::string_view svg_{index}=R"{delimiter}({text}){delimiter}";')
        else:
            lines.append(f"static constexpr char svg_{index}_bytes[]={{")
            for begin in range(0, len(raw), 64):
                lines.append(",".join(f"char({value})" for value in raw[begin:begin + 64]) + ",")
            lines.append(f"}}; static constexpr std::string_view svg_{index}{{svg_{index}_bytes,sizeof(svg_{index}_bytes)}};")
    for index, (_, text) in enumerate(fallbacks):
        raw = text.encode("utf-8")
        if len(raw) <= 60000:
            delimiter = "LVUIF" + str(index)
            if ")" + delimiter + '"' in text:
                raise ValueError("Fallback SVG raw-string delimiter collision")
            lines.append(f'static constexpr std::string_view fallback_{index}=R"{delimiter}({text}){delimiter}";')
        else:
            lines.append(f"static constexpr char fallback_{index}_bytes[]={{")
            for begin in range(0, len(raw), 64):
                lines.append(",".join(f"char({value})" for value in raw[begin:begin + 64]) + ",")
            lines.append(f"}}; static constexpr std::string_view fallback_{index}{{fallback_{index}_bytes,sizeof(fallback_{index}_bytes)}};")
    for index, (_, raw, text) in enumerate(design):
        if len(raw) <= 60000:
            delimiter = "LVUID" + str(index)
            if ")" + delimiter + '"' in text:
                raise ValueError("Design SVG raw-string delimiter collision")
            lines.append(f'static constexpr std::string_view design_{index}=R"{delimiter}({text}){delimiter}";')
        else:
            lines.append(f"static constexpr char design_{index}_bytes[]={{")
            for begin in range(0, len(raw), 64):
                lines.append(",".join(f"char({value})" for value in raw[begin:begin + 64]) + ",")
            lines.append(f"}}; static constexpr std::string_view design_{index}{{design_{index}_bytes,sizeof(design_{index}_bytes)}};")
    lines += ["std::span<const AuthoredIconRecord> authored_icon_records() noexcept {",
              "static constexpr AuthoredIconRecord entries[]={"]
    lines += ["{" + cpp_string(name) + f",svg_{index}" + "}," for index, (name, _, _) in enumerate(assets)]
    lines += ["}; return entries; }", "std::span<const std::string_view> authored_icon_record_names() noexcept {",
              "static constexpr std::string_view entries[]={"]
    lines += [cpp_string(name) + "," for name in names]
    lines += ["}; return entries; }"]
    lines += ["std::span<const AuthoredIconRecord> design_icon_records() noexcept {",
              f"static constexpr std::array<AuthoredIconRecord, {len(design)}> entries{{{{"]
    lines += ["{" + cpp_string(name) + f",design_{index}" + "}," for index, (name, _, _) in enumerate(design)]
    lines += ["}}; return entries; }", "std::span<const std::string_view> design_icon_record_names() noexcept {",
              f"static constexpr std::array<std::string_view, {len(design)}> entries{{{{"]
    lines += [cpp_string(name) + "," for name, _, _ in design]
    lines += ["}}; return entries; }"]
    lines += ["std::span<const AuthoredIconFallback> authored_icon_fallbacks() noexcept {",
              f"static constexpr std::array<AuthoredIconFallback, {len(fallbacks)}> entries{{{{"]
    lines += ["{" + cpp_string(name) + f",fallback_{index}" + "}," for index, (name, _) in enumerate(fallbacks)]
    lines += ["}}; return entries; }"]
    for function, aliases in (("authored_resource_aliases", resource_aliases), ("authored_name_aliases", name_aliases)):
        lines += [f"std::span<const AuthoredIconAlias> {function}() noexcept {{",
                  f"static constexpr std::array<AuthoredIconAlias, {len(aliases)}> entries{{{{"]
        lines += ["{" + cpp_string(alias) + "," + cpp_string(target) + "}," for alias, target in sorted(aliases.items())]
        lines += ["}}; return entries; }"]
    lines += ["} // namespace lvui::detail", ""]
    return "#include <array>\n" + "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--fallback-input", type=Path)
    parser.add_argument("--fallback-manifest", type=Path)
    parser.add_argument("--design-input", type=Path)
    parser.add_argument("--design-manifest", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    fallback_manifest = json.loads(args.fallback_manifest.read_text(encoding="utf-8")) if args.fallback_manifest else None
    design_manifest = json.loads(args.design_manifest.read_text(encoding="utf-8")) if args.design_manifest else None
    content = generate(args.input, json.loads(args.manifest.read_text(encoding="utf-8")),
                       args.fallback_input, fallback_manifest, args.design_input, design_manifest)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists() and args.output.read_text(encoding="utf-8") == content:
        return
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as out:
        out.write(content)
    os.replace(temporary, args.output)


if __name__ == "__main__":
    main()

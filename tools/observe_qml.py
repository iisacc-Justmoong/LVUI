#!/usr/bin/env python3
"""Record lexical QML usage as read-only input to the LVUI completion audit.

This is an observation tool, not a QML parser, translator, or proof that the
observed constructs can already be compiled by LVUI.
"""
from __future__ import annotations

import argparse
from bisect import bisect_right
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = ROOT.parent.parent / "Product/Dreamscapes/src/App"
DEFAULT_OUTPUT = ROOT / "catalog/dreamscapes-qml.json"


def mask_noncode(source: str) -> str:
    """Preserve positions/newlines while hiding comments and quoted literals."""
    masked = list(source)
    i = 0
    while i < len(source):
        start = i
        if source.startswith("//", i):
            i = source.find("\n", i)
            if i < 0:
                i = len(source)
        elif source.startswith("/*", i):
            end = source.find("*/", i + 2)
            i = len(source) if end < 0 else end + 2
        elif source[i] in "\"'`":
            quote = source[i]
            i += 1
            while i < len(source):
                if source[i] == "\\":
                    i = min(len(source), i + 2)
                elif source[i] == quote:
                    i += 1
                    break
                else:
                    i += 1
        else:
            i += 1
            continue
        for j in range(start, i):
            if source[j] not in "\r\n":
                masked[j] = " "
    return "".join(masked)


def inspect_source(source: str) -> dict:
    code = mask_noncode(source)
    newlines = [-1] + [i for i, c in enumerate(source) if c == "\n"]

    def line(position: int) -> int:
        return bisect_right(newlines, position)

    imports = []
    pattern = re.compile(
        r'^\s*(import)\s+("[^"\r\n]+"|[A-Za-z_]\w*(?:\.\w+)*)'
        r'(?:\s+(\d+(?:\.\d+)*))?(?:\s+as\s+([A-Za-z_]\w*))?', re.M)
    for match in pattern.finditer(source):
        if code[match.start(1):match.end(1)] != "import":
            continue
        imports.append({"module": match[2].strip('"'), "version": match[3],
                        "alias": match[4], "line": line(match.start(1))})
    aliases = {entry["alias"]: entry["module"] for entry in imports if entry["alias"]}
    components = []
    for match in re.finditer(r'\b([A-Z]\w*(?:\.[A-Z]\w*)*)\s*\{', code):
        spelling = match[1]
        parts = spelling.split(".")
        components.append({"name": spelling, "type": parts[-1],
                           "module": aliases.get(parts[0]) if len(parts) > 1 else None,
                           "line": line(match.start())})
    properties = []
    for match in re.finditer(
            r'\b(?:(readonly|required)\s+)?property\s+'
            r'(\w+(?:<[^>\r\n]+>)?)\s+([A-Za-z_]\w*)', code):
        properties.append({"name": match[3], "type": match[2],
                           "qualifier": match[1], "line": line(match.start())})
    declarations = []
    for match in re.finditer(r'\bcomponent\s+(\w+)\s*:\s*([\w.]+)\s*\{', code):
        declarations.append({"name": match[1], "base": match[2], "line": line(match.start())})
    handlers = []
    for match in re.finditer(r'\bon[A-Z]\w*\s*(?=:|\()', code):
        handlers.append({"name": match[0].strip(), "line": line(match.start())})
    functions = []
    for match in re.finditer(r'\bfunction\s+(\w+)\s*\(', code):
        functions.append({"name": match[1], "line": line(match.start())})
    operators = Counter(re.findall(r'===|!==|==|!=|<=|>=|&&|\|\||[?:+*/%<>!\-]', code))
    features = {
        "inline_components": len(declarations),
        "property_aliases": sum(p["type"] == "alias" for p in properties),
        "required_properties": sum(p["qualifier"] == "required" for p in properties),
        "readonly_properties": sum(p["qualifier"] == "readonly" for p in properties),
        "conditional_operators": operators["?"],
        "model_delegates": sum(c["type"] in {"Repeater", "ListView", "GridView", "Instantiator"}
                               for c in components),
        "loaders": sum(c["type"] == "Loader" for c in components),
        "scrolling": sum(c["type"] in {"Flickable", "ScrollView", "ListView", "GridView"}
                         for c in components),
        "canvas_and_shapes": sum(c["type"] in {"Canvas", "Shape", "ShapePath"} for c in components),
        "runtime_qml_construction": len(re.findall(r'\bQt\s*\.\s*(?:createQmlObject|createComponent)\s*\(', code)),
    }
    return {"imports": imports, "components": components, "properties": properties,
            "inline_components": declarations, "handlers": handlers, "functions": functions,
            "lexical_operators": dict(sorted(operators.items())), "features": features}


def observe(root: Path) -> dict:
    if not root.is_dir():
        raise ValueError(f"QML source directory does not exist: {root}")
    files = []
    type_uses = defaultdict(list)
    features = Counter()
    operators = Counter()
    for path in sorted(root.rglob("*.qml")):
        if path.is_symlink():
            raise ValueError(f"Refusing a symlink in observed source: {path}")
        raw = path.read_bytes()
        row = inspect_source(raw.decode("utf-8"))
        name = path.relative_to(root).as_posix()
        for component in row["components"]:
            key = ((component["module"] + ".") if component["module"] else "") + component["type"]
            type_uses[key].append({"file": name, "line": component["line"], "spelling": component["name"]})
        features.update(row["features"])
        operators.update(row["lexical_operators"])
        files.append({"file": name, "sha256": hashlib.sha256(raw).hexdigest(), **row})
    return {
        "schema": 1,
        "source": "Product/Dreamscapes/src/App",
        "purpose": "Read-only lexical observations for LVUI language and module completion; no conversion.",
        "limitations": ["This inventory is not a complete QML semantic analysis.",
                        "Type occurrences include visual and nonvisual object declarations.",
                        "Operator counts are lexical, including types and script bodies.",
                        "Quoted QML strings are not interpreted or converted."],
        "summary": {"qml_files": len(files), "component_occurrences": sum(map(len, type_uses.values())),
                    "distinct_component_spellings": len(type_uses),
                    "properties": sum(len(f["properties"]) for f in files),
                    "handlers": sum(len(f["handlers"]) for f in files),
                    "functions": sum(len(f["functions"]) for f in files),
                    "features": dict(sorted(features.items())),
                    "lexical_operators": dict(sorted(operators.items()))},
        "component_uses": dict(sorted(type_uses.items())), "files": files,
    }


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args(argv)
    try:
        result = observe(args.source.resolve())
        text = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
        if args.check:
            if not args.output.is_file() or args.output.read_text(encoding="utf-8") != text:
                print("QML observations differ; inspect source changes and refresh the read-only inventory.", file=sys.stderr)
                return 1
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(text, encoding="utf-8")
        print(json.dumps(result["summary"], ensure_ascii=False))
        return 0
    except (ValueError, OSError, UnicodeError) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

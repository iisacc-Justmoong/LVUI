#!/usr/bin/env python3
"""Validate and embed LVUI's static color-button design image."""
import argparse
import os
from pathlib import Path
import struct
import zlib

SIGNATURE = b"\x89PNG\r\n\x1a\n"


def validate(data):
    if len(data) > 8 * 1024 * 1024 or not data.startswith(SIGNATURE):
        raise ValueError("Invalid PNG signature or byte limit")
    position = len(SIGNATURE)
    header = False
    image = False
    while position < len(data):
        if len(data) - position < 12:
            raise ValueError("Truncated PNG chunk")
        size = struct.unpack_from(">I", data, position)[0]
        end = position + 12 + size
        if end > len(data):
            raise ValueError("Truncated PNG data")
        kind = data[position + 4:position + 8]
        payload = data[position + 8:position + 8 + size]
        crc = struct.unpack_from(">I", data, position + 8 + size)[0]
        if zlib.crc32(kind + payload) & 0xFFFFFFFF != crc:
            raise ValueError("Invalid PNG chunk checksum")
        if not header:
            if kind != b"IHDR" or size != 13:
                raise ValueError("PNG must begin with IHDR")
            width, height, depth, mode, compression, filtering, interlace = struct.unpack(">IIBBBBB", payload)
            if not (1 <= width <= 2048 and 1 <= height <= 2048 and width == height and depth == 8 and mode == 6 and
                    compression == 0 and filtering == 0 and interlace == 0):
                raise ValueError("Unsupported hue ring dimensions or PNG format")
            header = True
        elif kind == b"IHDR" or kind in {b"acTL", b"fcTL", b"fdAT"}:
            raise ValueError("Invalid or animated PNG")
        if kind == b"IDAT":
            image = True
        if kind == b"IEND":
            if size != 0 or not image or end != len(data):
                raise ValueError("Invalid PNG ending")
            return
        position = end
    raise ValueError("PNG ending missing")


def render(data):
    validate(data)
    lines = ["#pragma once", "#include <array>", "#include <cstdint>", "namespace lvui::detail {",
             f"inline constexpr std::array<std::uint8_t, {len(data)}> color_picker_hue_ring{{{{"]
    for position in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{value:02x}" for value in data[position:position + 16]) + ",")
    lines.extend(["}};", "} // namespace lvui::detail", ""])
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    content = render(args.input.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists() and args.output.read_text(encoding="utf-8") == content:
        return
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    with temporary.open("w", encoding="utf-8", newline="\n") as output:
        output.write(content)
    os.replace(temporary, args.output)


if __name__ == "__main__":
    main()

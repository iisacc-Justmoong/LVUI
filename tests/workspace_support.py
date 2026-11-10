"""Validate the private resource workspace supplied to a test process."""
from pathlib import Path
import re

import ii_file_provider as provider

ROOT = Path(__file__).resolve().parents[1]


def checked_workspace(path):
    base = ROOT / "resources" / ".t"
    path = Path(path)
    if base.is_symlink() or path.is_symlink():
        raise ValueError("The resource workspace cannot be a symlink")
    base = base.resolve(strict=True)
    resolved = path.resolve(strict=True)
    if resolved.parent != base or not re.fullmatch(r"[0-9a-f]{4}", resolved.name):
        raise ValueError("Expected a private resource workspace")
    return resolved

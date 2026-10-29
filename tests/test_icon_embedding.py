import os
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
script = root / "tools" / "embed_icons.py"
icons = root / "resources" / "icons"
unsafe = root / "resources" / "icon-test-fixtures" / "unsafe"
with tempfile.TemporaryDirectory(prefix="lvui-icon-test-") as directory:
    target = Path(directory) / "BuiltinIconAssets.h"
    command = [sys.executable, str(script), "--input", str(icons), "--output", str(target)]
    subprocess.run(command, check=True, env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"})
    first = target.read_text(encoding="utf-8")
    timestamp = target.stat().st_mtime_ns
    subprocess.run(command, check=True, env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"})
    assert target.read_text(encoding="utf-8") == first and target.stat().st_mtime_ns == timestamp
    names = sorted(path.stem for path in icons.glob("*.svg"))
    assert all(('"' + name + '"') in first for name in names)
    assert [first.index('"' + name + '"') for name in names] == sorted(first.index('"' + name + '"') for name in names)
    rejected = subprocess.run([sys.executable, str(script), "--input", str(unsafe), "--output", str(target)],
                              capture_output=True, env={**os.environ, "PYTHONDONTWRITEBYTECODE": "1"})
    assert rejected.returncode != 0 and target.read_text(encoding="utf-8") == first

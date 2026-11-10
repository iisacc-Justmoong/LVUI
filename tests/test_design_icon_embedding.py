import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

from workspace_support import provider

root = Path(__file__).resolve().parents[1]
script = root / "tools" / "embed_authored_icons.py"
env = {**os.environ, "PYTHONDONTWRITEBYTECODE": "1"}

with provider.TemporaryDirectory(prefix="lvui-design-icons-", dir=Path.cwd()) as workspace:
    workspace = Path(workspace)
    original = workspace / "original"
    design = workspace / "design"
    original.mkdir()
    design.mkdir()
    result = workspace / "icons.cpp"
    original_manifest = workspace / "original.json"
    design_manifest = workspace / "design.json"
    original_svg = b'<svg xmlns="http://www.w3.org/2000/svg" viewBox="-1 -1 18 18"/>'
    (original / "brush.svg").write_bytes(original_svg)
    original_manifest.write_text(json.dumps({
        "schema": 1,
        "files": [{"name": "brush", "sha256": hashlib.sha256(original_svg).hexdigest(),
                   "bytes": len(original_svg)}],
        "resource_aliases": {}, "name_aliases": {},
    }))

    def write_design(data):
        (design / "brush.svg").write_bytes(data)
        value = {"schema": 1, "files": [{"name": "brush", "sha256": hashlib.sha256(data).hexdigest(),
                                         "bytes": len(data)}]}
        design_manifest.write_text(json.dumps(value))

    command = [sys.executable, str(script), "--input", str(original), "--manifest", str(original_manifest),
               "--design-input", str(design), "--design-manifest", str(design_manifest), "--output", str(result)]
    good = (b'<svg xmlns="http://www.w3.org/2000/svg" width="18" height="18" viewBox="0 0 18 18">'
            b'<path d="M1 1L17 17"/></svg>')
    write_design(good)
    subprocess.run(command, check=True, env=env)
    accepted = result.read_bytes()
    assert b'authored_icon_records()' in accepted and b'design_icon_records()' in accepted
    assert original_svg in accepted and good in accepted
    timestamp = result.stat().st_mtime_ns
    subprocess.run(command, check=True, env=env)
    assert result.read_bytes() == accepted and result.stat().st_mtime_ns == timestamp

    for invalid in (
        good.replace(b'viewBox="0 0 18 18"', b'viewBox="-1 0 18 18"'),
        good.replace(b'<path', b'<script>unsafe</script><path'),
        good.replace(b'<path', b'<image href="https://example.invalid/x"/><path'),
    ):
        write_design(invalid)
        assert subprocess.run(command, capture_output=True, env=env).returncode != 0
        assert result.read_bytes() == accepted

    write_design(good)
    (design / "brush.svg").write_bytes(good + b" ")
    assert subprocess.run(command, capture_output=True, env=env).returncode != 0
    assert result.read_bytes() == accepted

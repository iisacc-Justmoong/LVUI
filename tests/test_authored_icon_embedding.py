import hashlib
import base64
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
from workspace_support import provider

root = Path(__file__).resolve().parents[1]
script = root / "tools" / "embed_authored_icons.py"
env = {**os.environ, "PYTHONDONTWRITEBYTECODE": "1"}

with provider.TemporaryDirectory(prefix="lvui-authored-icons-", dir=Path.cwd()) as workspace:
    folder = Path(workspace) / "icons"
    folder.mkdir()
    manifest = Path(workspace) / "manifest.json"
    result = Path(workspace) / "icons.cpp"
    asset = folder / "test.svg"

    def write_asset(text):
        data = text.encode("utf-8")
        asset.write_bytes(data)
        value = {"schema": 1, "files": [{"name": "test", "sha256": hashlib.sha256(data).hexdigest(),
                                        "bytes": len(data)}],
                 "resource_aliases": {"old": "test"}, "name_aliases": {"small": "test"}}
        manifest.write_text(json.dumps(value), encoding="utf-8")

    command = [sys.executable, str(script), "--input", str(folder), "--manifest", str(manifest), "--output", str(result)]
    write_asset('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1 1"><defs><linearGradient id="g"/></defs><rect fill="url(#g)" width="1" height="1"/></svg>')
    subprocess.run(command, check=True, env=env)
    first = result.read_bytes()
    assert b'"old","test"' in first and b'"small","test"' in first
    timestamp = result.stat().st_mtime_ns
    subprocess.run(command, check=True, env=env)
    assert result.read_bytes() == first and result.stat().st_mtime_ns == timestamp

    for invalid in (
        '<svg xmlns="http://www.w3.org/2000/svg"><image href="https://example.invalid/a.png"/></svg>',
        '<svg xmlns="http://www.w3.org/2000/svg"><script>unsafe</script></svg>',
        '<svg xmlns="http://www.w3.org/2000/svg"><rect fill="url(https://example.invalid/a)"/></svg>',
        '<svg xmlns="http://www.w3.org/2000/svg"><rect fill="url(#g) url (https://example.invalid/a)"/></svg>',
    ):
        write_asset(invalid)
        failed = subprocess.run(command, capture_output=True, env=env)
        assert failed.returncode != 0 and result.read_bytes() == first

    write_asset('<svg xmlns="http://www.w3.org/2000/svg"/>')
    asset.write_text('<svg xmlns="http://www.w3.org/2000/svg"><rect/></svg>', encoding="utf-8")
    failed = subprocess.run(command, capture_output=True, env=env)
    assert failed.returncode != 0 and result.read_bytes() == first

    write_asset('<svg xmlns="http://www.w3.org/2000/svg"/>')
    fallback_folder = Path(workspace) / "fallbacks"
    fallback_folder.mkdir()
    fallback_manifest = fallback_folder / "manifest.json"
    png = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAusB9WlWcQAAAABJRU5ErkJggg==")
    fallback = fallback_folder / "test.svg"
    (fallback_folder / "test-reference.png").write_bytes(png)
    svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1" viewBox="0 0 1 1">'
           '<image width="1" height="1" href="data:image/png;base64,' + base64.b64encode(png).decode() + '"/></svg>')

    def write_fallback(text=svg, **overrides):
        data = text.encode("utf-8")
        fallback.write_bytes(data)
        entry = {"name": "test", "file": "test.svg", "source_sha256": hashlib.sha256(asset.read_bytes()).hexdigest(),
                 "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
                 "png_sha256": hashlib.sha256(png).hexdigest(), "width": 1, "height": 1,
                 "references": [{"file": "test-reference.png", "sha256": hashlib.sha256(png).hexdigest(),
                                 "width": 1, "height": 1, "icon_size": 1}]}
        entry.update(overrides)
        fallback_manifest.write_text(json.dumps({"schema": 1, "entries": [entry]}), encoding="utf-8")

    fallback_command = command + ["--fallback-input", str(fallback_folder), "--fallback-manifest", str(fallback_manifest)]
    write_fallback()
    subprocess.run(fallback_command, check=True, env=env)
    accepted = result.read_bytes()
    assert b'authored_icon_fallbacks()' in accepted and b'{"test",fallback_0}' in accepted
    module_spec = importlib.util.spec_from_file_location("authored_icon_embedder_test", script)
    module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    module.MAX_TOTAL = len(svg.encode("utf-8")) + len(png) - 1
    try:
        module.checked_fallbacks(fallback_folder, json.loads(fallback_manifest.read_text()),
                                 {"test": hashlib.sha256(asset.read_bytes()).hexdigest()})
    except ValueError:
        pass
    else:
        raise AssertionError("Reference image bytes escaped the aggregate resource budget")
    large_svg = svg.replace('><image', '>' + ' ' * 61000 + '<image')
    write_fallback(large_svg)
    subprocess.run(fallback_command, check=True, env=env)
    assert b'fallback_0_bytes[]' in result.read_bytes()
    write_fallback()
    subprocess.run(fallback_command, check=True, env=env)
    accepted = result.read_bytes()
    other = folder / "other.svg"
    other_data = b'<svg xmlns="http://www.w3.org/2000/svg"/>'
    other.write_bytes(other_data)
    ambiguous = json.loads(manifest.read_text())
    ambiguous["files"].append({"name": "other", "sha256": hashlib.sha256(other_data).hexdigest(), "bytes": len(other_data)})
    ambiguous["name_aliases"]["test"] = "other"
    manifest.write_text(json.dumps(ambiguous), encoding="utf-8")
    failed = subprocess.run(fallback_command, capture_output=True, env=env)
    assert failed.returncode != 0 and result.read_bytes() == accepted
    other.unlink()
    write_asset('<svg xmlns="http://www.w3.org/2000/svg"/>')
    for invalid, overrides in (
        (svg, {"source_sha256": "0" * 64}),
        (svg, {"png_sha256": "0" * 64}),
        (svg, {"width": 2}),
        (svg, {"references": [{"file": "test-reference.png", "sha256": "0" * 64,
                                "width": 1, "height": 1, "icon_size": 1}]}),
        (svg.replace("data:image/png;base64,", "https://example.invalid/"), {}),
        (svg.replace('width="1" height="1" href=', 'width="1" height="1" onclick="x" href='), {}),
    ):
        write_fallback(invalid, **overrides)
        failed = subprocess.run(fallback_command, capture_output=True, env=env)
        assert failed.returncode != 0 and result.read_bytes() == accepted

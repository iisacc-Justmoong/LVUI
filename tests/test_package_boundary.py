"""The installed LVUI catalog contains only LVUI-owned public data."""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def main():
    build = Path(sys.argv[1]).resolve()
    source = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix="lvui-package-boundary-", dir=build) as temporary:
        prefix = Path(temporary)
        # Preserve the build's existing install manifest: run the same generated
        # install rules with only their manifest destination redirected here.
        script = (build / "cmake_install.cmake").read_text(encoding="utf-8")
        original = f'{build.as_posix()}/${{CMAKE_INSTALL_MANIFEST}}'
        if original not in script:
            raise AssertionError("Unable to isolate the generated install manifest")
        script = script.replace(original, f'{prefix.as_posix()}/${{CMAKE_INSTALL_MANIFEST}}')
        local_script = prefix / "cmake_install.cmake"
        local_script.write_text(script, encoding="utf-8")
        subprocess.run([os.environ.get("CMAKE_COMMAND", "cmake"), f"-DCMAKE_INSTALL_PREFIX={prefix}", "-P", str(local_script)],
                       check=True, stdout=subprocess.DEVNULL)
        catalog = prefix / "share" / "LVUI" / "catalog"
        names = {entry.name for entry in catalog.iterdir()}
        expected = {"lvui-keywords.json", "lvui-style-tokens.json"}
        if names != expected:
            raise AssertionError(f"Unexpected installed catalog: {sorted(names)}")
        for name in expected:
            if (catalog / name).read_bytes() != (source / "catalog" / name).read_bytes():
                raise AssertionError(f"Installed catalog differs from the LVUI source: {name}")
        headers = list((prefix / "include" / "LVUI").rglob("*.h"))
        if not headers:
            raise AssertionError("No LVUI public headers were installed")
        include = re.compile(
            r'^\s*#\s*include\s*[<"](?:LVRS/|Qt[A-Za-z0-9_]*/|'
            r'Qt[A-Za-z0-9_]*\b|Q[A-Z][A-Za-z0-9_]*\b)',
            re.M)
        for header in headers:
            text = header.read_text(encoding="utf-8")
            if include.search(text) or re.search(r'\blvrs::', text):
                raise AssertionError(f"Installed header exposes a legacy UI dependency: {header}")
        packages = list(prefix.glob("**/cmake/LVUI/*.cmake"))
        if not packages:
            raise AssertionError("No LVUI CMake package was installed")
        dependency = re.compile(r'find_(?:package|dependency)\s*\(\s*(?:LVRS\b|Qt[56]?[A-Za-z0-9_]*\b)|LVRS::|Qt[56]?::', re.I)
        for package in packages:
            if dependency.search(package.read_text(encoding="utf-8")):
                raise AssertionError(f"Installed package exposes a legacy UI dependency: {package}")


if __name__ == "__main__":
    main()

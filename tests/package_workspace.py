"""Validate isolated installed-package checks and redirect their manifests."""
from pathlib import Path, PureWindowsPath
import errno
import re

from workspace_support import checked_workspace


def cleanup_owned_temporary(temporary):
    """Retry only ENOTEMPTY on an exclusively owned package-test directory."""
    for attempt in range(4):
        try:
            temporary.cleanup()
            return
        except OSError as error:
            if error.errno != errno.ENOTEMPTY or attempt == 3:
                raise


def checked_paths(source, raw_build, raw_workspace):
    source = Path(source).resolve(strict=True)
    raw_build = Path(raw_build)
    root = source / "build"
    if raw_build.is_symlink() or root.is_symlink():
        raise ValueError("Build roots must be direct directories")
    build = raw_build.resolve(strict=True)
    if not build.is_relative_to(root.resolve(strict=True)):
        raise ValueError("Package check must use this project's build")
    return build, checked_workspace(raw_workspace)


def redirect_manifest(content, build, workspace):
    for name in ("install_local_manifest.txt", "${CMAKE_INSTALL_MANIFEST}"):
        old = build.as_posix() + "/" + name
        if content.count(old) != 1:
            raise ValueError("Cannot safely redirect the install manifest")
        content = content.replace(old, workspace.as_posix() + "/" + name)
    return content


def validate_install_dirs(cache):
    for name in ("BINDIR", "DATADIR", "DATAROOTDIR", "INCLUDEDIR", "LIBDIR"):
        match = re.search(r"^CMAKE_INSTALL_" + name + r":[^=]*=(.*)$", cache, re.M)
        if match and match[1]:
            native, windows = Path(match[1]), PureWindowsPath(match[1])
            if native.is_absolute() or windows.drive or windows.root or ".." in native.parts or ".." in windows.parts:
                raise ValueError("Unsafe install directory: " + name)

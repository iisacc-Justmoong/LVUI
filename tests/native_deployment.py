"""Verify that an installed LVUI app is relocatable without launching its GUI."""
from pathlib import Path
import argparse
import hashlib
import os
import re
import shutil
import subprocess
import tempfile

from native_install import install_paths
from workspace_support import checked_workspace


def run(command, environment, check=True):
    result = subprocess.run(command, env=environment, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    if result.returncode:
        print(result.stdout[-12000:], end="", flush=True)
    if check:
        result.check_returncode()
    return result


def macho_closure(app, environment):
    executable = app / "Contents/MacOS/Deployable"
    app = app.resolve()
    pending = [(executable.resolve(), ())]
    seen = set()
    while pending:
        binary, inherited = pending.pop()
        if binary in seen:
            continue
        if not binary.is_relative_to(app):
            raise AssertionError(f"A dependency escaped the relocated app: {binary}")
        seen.add(binary)
        header = run(["otool", "-arch", "arm64", "-l", str(binary)], environment).stdout
        paths = re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset \d+\)', header)
        def expand(value):
            return value.replace("@loader_path", str(binary.parent)).replace(
                "@executable_path", str(executable.parent))
        rpaths = tuple(expand(path) for path in paths) + inherited
        for path in rpaths:
            if not Path(path).resolve().is_relative_to(app):
                raise AssertionError(f"An external search path remains: {path}")
        links = run(["otool", "-arch", "arm64", "-L", str(binary)], environment).stdout
        for link in re.findall(r'^\s+(.+?) \(compatibility version ', links, re.M):
            if link.startswith(("/usr/lib/", "/System/Library/")):
                continue
            if re.search(r'(?:^|/)(?:lib)?(?:qt[0-9]|qml|lvrs)', link, re.I):
                raise AssertionError(f"Forbidden legacy dependency: {link}")
            if link.startswith("@rpath/"):
                candidates = [Path(path) / link[len("@rpath/"):] for path in rpaths]
            else:
                candidates = [Path(expand(link))]
            target = next((path.resolve() for path in candidates if path.exists()), None)
            if target is None or not target.is_relative_to(app):
                raise AssertionError(f"Unresolved or external dependency: {link} in {binary}")
            if target != binary:
                pending.append((target, rpaths))
    names = {item.name for item in seen}
    if not any(name.startswith("libiiSDKCore.") for name in names):
        raise AssertionError("The installed app does not contain iiSDKCore")
    if not any(name.startswith("libMoltenVK") for name in names):
        raise AssertionError("The installed app does not contain its Vulkan fallback")
    return seen


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--cmake", required=True)
    args = parser.parse_args()
    workspace = checked_workspace(args.workspace)
    build = args.build.resolve()
    source = Path(__file__).resolve().parents[1]
    cache_text = (build / "CMakeCache.txt").read_text()
    cache = dict(re.findall(r'^([^/#][^:\n]*):[^=\n]*=(.*)$', cache_text, re.M))
    paths = install_paths(cache_text)
    env = os.environ.copy()
    for key in list(env):
        if key in {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH",
                   "CMAKE_PREFIX_PATH", "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH",
                   "CMAKE_INSTALL_MODE", "DESTDIR"} or key.startswith(
                       ("LVRS_", "QT_", "QML", "DYLD_", "LD_")):
            env.pop(key, None)
    with tempfile.TemporaryDirectory(prefix="native-deployment-", dir=build) as temporary:
        root = Path(temporary)
        sdk = root / "sdk"
        script = (build / "cmake_install.cmake").read_text()
        original = f'{build.as_posix()}/${{CMAKE_INSTALL_MANIFEST}}'
        if original not in script:
            raise RuntimeError("Unable to isolate the install manifest")
        isolated = workspace / "cmake_install.cmake"
        isolated.write_text(script.replace(original, f'{workspace.as_posix()}/${{CMAKE_INSTALL_MANIFEST}}'))
        run([args.cmake, f"-DCMAKE_INSTALL_PREFIX={sdk}", "-P", str(isolated)], env)
        package = sdk / paths["lib"] / "cmake/LVUI"
        options = [f"-DLVUI_DIR={package}", f"-DLVUI_FIXTURE_DIR={source / 'tests/fixtures'}",
                   "-DCMAKE_DISABLE_FIND_PACKAGE_LVRS=ON", "-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON",
                   "-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON"]
        for name in ("iiSDKCore_DIR", "iiGUI_DIR", "iiColorManager_DIR", "Vulkan_INCLUDE_DIR",
                     "Vulkan_LIBRARY", "CMAKE_CXX_COMPILER", "CMAKE_OSX_DEPLOYMENT_TARGET"):
            if cache.get(name) and not cache[name].endswith("-NOTFOUND"):
                options.append(f"-D{name}={cache[name]}")
        project = source / "tests/native_deployment_consumer"
        invalid = run([args.cmake, "-S", str(project), "-B", str(root / "invalid/build"),
                       "-G", "Ninja", "-DBAD_INSTALL_DESTINATION=ON", *options], env, check=False)
        if invalid.returncode == 0 or "relative" not in invalid.stdout.lower():
            raise AssertionError("An escaping installation destination was not rejected")
        target = root / "consumer/build"
        prefix = root / "installation"
        run([args.cmake, "-S", str(project), "-B", str(target), "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={prefix}", *options], env)
        run([args.cmake, "--build", str(target), "--target", "Deployable", "--parallel", "2"], env)
        source_binary = target / "Deployable.app/Contents/MacOS/Deployable"
        core_lib = Path(cache["iiSDKCore_DIR"]).parents[1] / "libiiSDKCore.0.dylib"
        observation_lib = core_lib.parent / "libiiSDKCoreObservation.0.dylib"
        originals = [source_binary, core_lib, observation_lib, Path(cache["Vulkan_LIBRARY"])]
        digest = lambda path: hashlib.sha256(path.read_bytes()).digest()
        before = [digest(path) for path in originals]
        run([args.cmake, "--install", str(target)], env)
        original_app = prefix / "Installed Apps - 도구/Deployable.app"
        relocated = root / "relocated/Deployable.app"
        relocated.parent.mkdir()
        shutil.move(original_app, relocated)
        binaries = macho_closure(relocated, env)
        run(["codesign", "--verify", "--deep", "--strict", str(relocated)], env)
        if [digest(path) for path in originals] != before:
            raise AssertionError("Installation modified the build target or a source SDK library")

        destdir_env = env.copy()
        destdir_env["DESTDIR"] = str(root / "destdir")
        run([args.cmake, "--install", str(target), "--prefix", "/lvui-test"], destdir_env)
        destdir_app = root / "destdir/lvui-test/Installed Apps - 도구/Deployable.app"
        macho_closure(destdir_app, env)
        run(["codesign", "--verify", "--deep", "--strict", str(destdir_app)], env)

        link_env = env.copy()
        link_env["CMAKE_INSTALL_MODE"] = "ABS_SYMLINK"
        linked = run([args.cmake, "--install", str(target), "--prefix", str(root / "linked")],
                     link_env, check=False)
        if linked.returncode == 0 or "requires copy mode" not in linked.stdout:
            raise AssertionError("A symlink-based install mode was not rejected")

        outside = root / "external-library.dylib"
        outside.write_bytes(b"must stay unchanged")
        framework_dir = target / "Deployable.app/Contents/Frameworks"
        framework_dir.mkdir(exist_ok=True)
        external_link = framework_dir / "external.dylib"
        external_link.symlink_to(outside)
        escaped = run([args.cmake, "--install", str(target), "--prefix", str(root / "escaped")],
                      env, check=False)
        if escaped.returncode == 0 or "symbolic link outside the bundle" not in escaped.stdout:
            raise AssertionError("An external bundle link was not rejected")
        if outside.read_bytes() != b"must stay unchanged":
            raise AssertionError("The external link target was modified")
        external_link.unlink()
        existing = root / "existing/Installed Apps - 도구/Deployable.app/Contents/Frameworks"
        existing.mkdir(parents=True)
        (existing / "external.dylib").symlink_to(outside)
        escaped_existing = run([args.cmake, "--install", str(target), "--prefix", str(root / "existing")],
                               env, check=False)
        if escaped_existing.returncode == 0 or "symbolic link outside the bundle" not in escaped_existing.stdout:
            raise AssertionError("An external link in an existing installation was not rejected")
        if outside.read_bytes() != b"must stay unchanged":
            raise AssertionError("The existing installation modified the external link target")
        overlap = root / "overlap"
        overlap.mkdir()
        (overlap / "Installed Apps - 도구").symlink_to(target, target_is_directory=True)
        overlapping = run([args.cmake, "--install", str(target), "--prefix", str(overlap)], env, check=False)
        if overlapping.returncode == 0 or "overlaps its build bundle" not in overlapping.stdout:
            raise AssertionError("An installation overlapping the build bundle was not rejected")
        if digest(source_binary) != before[0]:
            raise AssertionError("The overlapping installation modified its build target")

        fake_target = root / "fake-qt/build"
        run([args.cmake, "-S", str(project), "-B", str(fake_target), "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Release", "-DWITH_FAKE_QT=ON", *options], env)
        run([args.cmake, "--build", str(fake_target), "--target", "Deployable", "--parallel", "2"], env)
        fake_library = fake_target / "libQt6Pretend.dylib"
        backup = root / "fake-library-backup"
        fake_library.rename(backup)
        missing = run([args.cmake, "--install", str(fake_target), "--prefix", str(root / "missing")],
                      env, check=False)
        if missing.returncode == 0 or "unresolved" not in missing.stdout.lower():
            raise AssertionError("An unresolved runtime dependency was not rejected")
        if (root / "missing/Installed Apps - 도구/Deployable.app").exists():
            raise AssertionError("An application with a missing dependency was copied")
        backup.rename(fake_library)
        forbidden = run([args.cmake, "--install", str(fake_target), "--prefix", str(root / "forbidden")],
                        env, check=False)
        if forbidden.returncode == 0 or "Qt, QML or LVRS dependency" not in forbidden.stdout:
            raise AssertionError("A Qt-named runtime dependency was not rejected")
        if (root / "forbidden/Installed Apps - 도구/Deployable.app").exists():
            raise AssertionError("A forbidden application was copied before dependency validation")

        collision_target = root / "colliding/build"
        run([args.cmake, "-S", str(project), "-B", str(collision_target), "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Release", "-DWITH_COLLIDING_DEPENDENCIES=ON", *options], env)
        run([args.cmake, "--build", str(collision_target), "--target", "Deployable", "--parallel", "2"], env)
        collision = run([args.cmake, "--install", str(collision_target), "--prefix", str(root / "collision")],
                        env, check=False)
        if collision.returncode == 0 or "ambiguous bundled dependency names" not in collision.stdout:
            raise AssertionError("Conflicting embedded dependency keys were not rejected")
        if (root / "collision/Installed Apps - 도구/Deployable.app").exists():
            raise AssertionError("An application with conflicting dependencies was copied")
        if [digest(path) for path in originals] != before:
            raise AssertionError("An install attempt modified the build target or a source SDK library")
        print(f"Relocated app: {len(binaries)} bundled Mach-O files; GUI not executed")


if __name__ == "__main__":
    main()

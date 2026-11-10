"""Verify target builds do not install target binaries as host compiler tools."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    env = os.environ.copy()
    for key in list(env):
        if key in {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH"} or key.startswith(("QT_", "QML", "LVRS_", "DYLD_", "LD_")):
            env.pop(key, None)
    sdk = subprocess.check_output(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], text=True, env=env).strip()
    with tempfile.TemporaryDirectory(prefix=".ios-cross-input-", dir=root / "resources") as resource, \
         tempfile.TemporaryDirectory(prefix="ios-cross-build-", dir=args.build) as output:
        resource, output = Path(resource), Path(output)
        core, color = resource / "core", resource / "color"
        header = core / "include/iiSDKCore/Core/Property/Observable.h"
        header.parent.mkdir(parents=True)
        header.write_text('''namespace ii::core {
struct ObservableRead {};
struct ObservableReadScope { template<class F,class G> static auto collect(F, G g) { return g(); } };
template<class T> struct Observable { T value; T read_dependency() const { return value; } T get() const { return value; } };
}\n''')
        (core / "iiSDKCoreConfig.cmake").write_text(f'''add_library(iiSDKCore::Contracts INTERFACE IMPORTED)
set_target_properties(iiSDKCore::Contracts PROPERTIES INTERFACE_INCLUDE_DIRECTORIES [==[{(core / 'include').as_posix()}]==])
add_library(iiSDKCore::Observation INTERFACE IMPORTED)
set_target_properties(iiSDKCore::Observation PROPERTIES INTERFACE_LINK_LIBRARIES iiSDKCore::Contracts)
set(iiSDKCore_Contracts_FOUND TRUE)
set(iiSDKCore_Observation_FOUND TRUE)
''')
        color.mkdir()
        (color / "iiColorManagerConfig.cmake").write_text('add_library(iiColorManager::Values INTERFACE IMPORTED)\nset(iiColorManager_Values_FOUND TRUE)\n')
        version = 'set(PACKAGE_VERSION "9.0.0")\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n'
        (core / "iiSDKCoreConfigVersion.cmake").write_text(version)
        (color / "iiColorManagerConfigVersion.cmake").write_text(version)
        for name, tools, testing in (("runtime", False, False), ("default-tests", False, True), ("target-tools", True, False)):
            build = output / name / "build"
            command = ["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja", "-DCMAKE_SYSTEM_NAME=iOS",
                       f"-DCMAKE_OSX_SYSROOT={sdk}", "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=18.0",
                       f"-DiiSDKCore_DIR={core}", f"-DiiColorManager_DIR={color}", f"-DBUILD_TESTING={'ON' if testing else 'OFF'}"]
            if tools:
                command.append("-DLVUI_BUILD_COMPILER_TOOLS=ON")
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
            if result.returncode:
                raise RuntimeError(f"{name} cross configuration failed:\n" + result.stdout)
            ninja = (build / "build.ninja").read_text()
            has_compiler = bool(re.search(r"^build lvui-compile(?:\.app/[^: ]+)?\s*:", ninja, re.M))
            if has_compiler != tools:
                raise RuntimeError(f"{name} exposed an unexpected target compiler executable")
            if not re.search(r"^build (?:libLVUINative\.a|LVUINative)\s*:", ninja, re.M):
                raise RuntimeError(f"{name} did not configure the native LVUI runtime")
        for generator in ("lvui_generate", "lvui_generate_styles"):
            probe = resource / f"{generator}.cmake"
            probe.write_text(f'include([==[{(root / "cmake/LVUIAot.cmake").as_posix()}]==])\n'
                             f'{generator}(NAME Example SOURCE example.lvui OUTPUT_VARIABLE generated)\n')
            result = subprocess.run(["cmake", "-P", str(probe)], text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
            if result.returncode == 0 or "LVUI_HOST_COMPILER" not in result.stdout:
                raise RuntimeError(f"{generator} did not diagnose a missing host compiler:\n" + result.stdout)
    print("iOS runtime and explicit target-tool configurations verified; no app executed")


if __name__ == "__main__":
    main()

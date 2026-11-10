"""Build iOS bundle metadata without executing a GUI or modifying the SDK install."""
from pathlib import Path
import argparse
import os
import plistlib
import shutil
import subprocess
import tempfile


def command(args, *, env):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
    if result.returncode:
        raise RuntimeError("Command failed:\n" + result.stdout)


def project(source, helper, custom):
    source.mkdir(parents=True)
    (source / "Main.cpp").write_text("int main() { return 0; }\n")
    (source / "App.lvui").write_text('Application { Window { Label(text: "Test") } }\n')
    (source / "Custom.plist.in").write_text(
        '<?xml version="1.0" encoding="UTF-8"?><plist version="1.0"><dict>'
        '<key>CFBundleExecutable</key><string>${MACOSX_BUNDLE_EXECUTABLE_NAME}</string>'
        '<key>CFBundleIdentifier</key><string>${MACOSX_BUNDLE_GUI_IDENTIFIER}</string>'
        '<key>CFBundlePackageType</key><string>APPL</string>'
        '<key>CustomMarker</key><string>preserved</string></dict></plist>')
    extra = ('lvui_add_application(Preconfigured SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/App.lvui")\n'
             'lvui_add_application(Custom SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/App.lvui")\n'
             'set_target_properties(Custom PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Custom.plist.in")\n') if custom else ""
    (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.31)
project(LVUIBundleProbe VERSION 1.0 LANGUAGES CXX)
add_library(LVUI::Native INTERFACE IMPORTED)
add_library(iiGUI::iiGUI INTERFACE IMPORTED)
add_library(iiSDKCore::Runtime INTERFACE IMPORTED)
set(LVUI_HOST_COMPILER /usr/bin/true)
function(iisdkcore_add_application target)
    add_executable(${{target}} MACOSX_BUNDLE "${{CMAKE_CURRENT_SOURCE_DIR}}/Main.cpp")
    set_target_properties(${{target}} PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "com.iisacc.test.${{target}}"
        MACOSX_BUNDLE_BUNDLE_NAME "${{target}}" MACOSX_BUNDLE_SHORT_VERSION_STRING 1.0 MACOSX_BUNDLE_BUNDLE_VERSION 1)
    if(target STREQUAL "Preconfigured")
        set_target_properties(${{target}} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${{CMAKE_CURRENT_SOURCE_DIR}}/Custom.plist.in")
    endif()
endfunction()
include([==[{helper.as_posix()}]==])
lvui_add_application(Default SOURCE "${{CMAKE_CURRENT_SOURCE_DIR}}/App.lvui")
{extra}''')


def read_bundle(build, name):
    candidates = (build / f"{name}.app/Info.plist", build / f"{name}.app/Contents/Info.plist")
    path = next((candidate for candidate in candidates if candidate.is_file()), None)
    if not path:
        raise RuntimeError(f"{name} iOS bundle has no Info.plist")
    with path.open("rb") as stream:
        return plistlib.load(stream)


def validate_default(value):
    manifest = value.get("UIApplicationSceneManifest", {})
    configurations = manifest.get("UISceneConfigurations", {}).get("UIWindowSceneSessionRoleApplication", [])
    if (manifest.get("UIApplicationSupportsMultipleScenes") is not True or
            not any(item.get("UISceneDelegateClassName") == "IIGUIIOSScene" for item in configurations) or
            "com.iisacc.iigui.scene" not in value.get("NSUserActivityTypes", [])):
        raise RuntimeError("Default iOS LVUI bundle is missing its independent UIKit scene configuration")


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
    with tempfile.TemporaryDirectory(prefix=".ios-bundle-input-", dir=root / "resources") as resource, \
         tempfile.TemporaryDirectory(prefix="ios-bundle-build-", dir=args.build) as output:
        resource, output = Path(resource), Path(output)
        for name, helper, custom in (("source", root / "cmake/LVUIAot.cmake", True),):
            source, build = resource / name, output / name / "build"
            project(source, helper, custom)
            command(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja", "-DCMAKE_SYSTEM_NAME=iOS",
                     f"-DCMAKE_OSX_SYSROOT={sdk}", "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=18.0"], env=env)
            command(["cmake", "--build", str(build), "--parallel", "2"], env=env)
            validate_default(read_bundle(build, "Default"))
            if read_bundle(build, "Custom").get("CustomMarker") != "preserved":
                raise RuntimeError("The application could not override its iOS bundle metadata")
            if read_bundle(build, "Preconfigured").get("CustomMarker") != "preserved":
                raise RuntimeError("The application helper overwrote preconfigured iOS bundle metadata")

        package = resource / "relocated/lib/cmake/LVUI"
        (package / "ios").mkdir(parents=True)
        shutil.copy2(root / "cmake/LVUIAot.cmake", package / "LVUIAot.cmake")
        shutil.copy2(root / "resources/platform/ios/LVUIInfo.plist.in", package / "ios/LVUIInfo.plist.in")
        source, build = resource / "consumer", output / "consumer/build"
        project(source, package / "LVUIAot.cmake", False)
        command(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja", "-DCMAKE_SYSTEM_NAME=iOS",
                 f"-DCMAKE_OSX_SYSROOT={sdk}", "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=18.0"], env=env)
        command(["cmake", "--build", str(build), "--parallel", "2"], env=env)
        validate_default(read_bundle(build, "Default"))
    print("Source and relocated LVUI iOS bundle metadata built; custom override preserved; no app executed")


if __name__ == "__main__":
    main()

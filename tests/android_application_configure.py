"""Check the declarative NativeActivity target without executing Android code."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import sys
import tempfile


def configure(root, output, helper, ndk, available):
    source = root / ("available" if available else "missing")
    source.mkdir(parents=True)
    (source / "App.lvui").write_text('Application { Window { Label(text: "Hello") } }\n')
    (source / "Extra.cpp").write_text("void application_extra() {}\n")
    entry = 'add_library(iiGUI::AndroidEntry INTERFACE IMPORTED)\n' if available else ""
    (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.31)
project(AndroidEntryProbe LANGUAGES CXX)
add_library(LVUI::Native INTERFACE IMPORTED)
add_library(iiSDKCore::Runtime INTERFACE IMPORTED)
add_library(iiGUI::iiGUI INTERFACE IMPORTED)
add_library(application_extra INTERFACE)
{entry}set(LVUI_HOST_COMPILER [==[{Path(sys.executable).as_posix()}]==])
function(iisdkcore_add_application target)
    message(FATAL_ERROR "Android used the desktop executable entry")
endfunction()
include([==[{helper.as_posix()}]==])
lvui_add_application(App SOURCE "${{CMAKE_CURRENT_SOURCE_DIR}}/App.lvui"
    SOURCES "${{CMAKE_CURRENT_SOURCE_DIR}}/Extra.cpp" LIBRARIES application_extra)
get_target_property(type App TYPE)
if(NOT type STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR "Android LVUI app is not a NativeActivity shared library")
endif()
get_target_property(libraries App LINK_LIBRARIES)
foreach(expected IN ITEMS iiGUI::AndroidEntry LVUI::Native application_extra)
    if(NOT expected IN_LIST libraries)
        message(FATAL_ERROR "Android LVUI app omitted ${{expected}}")
    endif()
endforeach()
get_target_property(sources App SOURCES)
if(NOT "${{CMAKE_CURRENT_SOURCE_DIR}}/Extra.cpp" IN_LIST sources)
    message(FATAL_ERROR "Android LVUI app omitted application sources")
endif()
''')
    env = os.environ.copy()
    for key in list(env):
        if key in {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH"} or key.startswith(("QT_", "QML", "LVRS_", "DYLD_", "LD_")):
            env.pop(key, None)
    command = ["cmake", "-S", str(source), "-B", str(output), "-G", "Ninja",
               f"-DCMAKE_TOOLCHAIN_FILE={ndk / 'build/cmake/android.toolchain.cmake'}",
               "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-24"]
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, env=env)
    if available and result.returncode:
        raise RuntimeError("Android application configuration failed:\n" + result.stdout)
    if not available and (result.returncode == 0 or "iiGUI::AndroidEntry" not in result.stdout):
        raise RuntimeError("Missing Android entry did not produce the expected diagnostic:\n" + result.stdout)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--ndk", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    with tempfile.TemporaryDirectory(prefix=".android-entry-input-", dir=root / "resources") as inputs, \
         tempfile.TemporaryDirectory(prefix="android-entry-config-", dir=args.build) as outputs:
        inputs, outputs = Path(inputs), Path(outputs)
        helper = root / "cmake/LVUIAot.cmake"
        configure(inputs / "source", outputs / "source", helper, args.ndk, True)
        relocated = inputs / "relocated/lib/cmake/LVUI/LVUIAot.cmake"
        relocated.parent.mkdir(parents=True)
        shutil.copy2(helper, relocated)
        configure(inputs / "relocated-test", outputs / "relocated", relocated, args.ndk, True)
        configure(inputs / "old-package", outputs / "missing", helper, args.ndk, False)
    print("Android LVUI app target and missing-entry diagnostic verified; no app executed")


if __name__ == "__main__":
    main()

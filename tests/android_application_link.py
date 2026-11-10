"""Link a declarative Android NativeActivity against explicitly staged SDKs."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import sys
import tempfile


def forbidden_framework_symbols(text):
    return re.search(
        r"\blvrs::|\bQt(?:Private)?::|\bQ(?:Object|CoreApplication|GuiApplication|Application|Widget|Window|"
        r"Quick\w*|Qml\w*|Variant|MetaObject|String|ByteArray|Json\w*|EventLoop)\b", text)


def run(command, env):
    result = subprocess.run([str(item) for item in command], text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
    if result.returncode:
        raise RuntimeError("Command failed: " + " ".join(map(str, command)) + "\n" + result.stdout)
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    for name in ("build", "ndk", "core", "color", "gui", "lvui", "host-compiler"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    ndk = args.ndk.resolve()
    notices = (
        (ndk / "sources/android/native_app_glue/NOTICE", args.gui / "share/licenses/iiGUI/AndroidNativeAppGlue-NOTICE"),
        (ndk / "NOTICE", args.gui / "share/licenses/iiGUI/AndroidNDK-NOTICE"),
    )
    for source_notice, installed_notice in notices:
        if not installed_notice.is_file() or installed_notice.read_bytes() != source_notice.read_bytes():
            raise RuntimeError("Android entry package omitted an original NDK notice: " + str(installed_notice))
    for name in ("AndroidImeBridge.java", "AndroidImeBridge.pro"):
        if not (args.gui / "share/iiGUI/android" / name).is_file():
            raise RuntimeError("The Android iiGUI package omitted its input-method resource: " + name)
    for fixture in ("android-entry", "android-text", "android-pointer", "android-global", "android-editor", "android-keyboard"):
        if (args.gui / "share/iiGUI/resources/fixtures" / fixture).exists():
            raise RuntimeError("The installed iiGUI package exposed its Android test doubles: " + fixture)
    host = "darwin-x86_64" if sys.platform == "darwin" else "windows-x86_64" if os.name == "nt" else "linux-x86_64"
    toolchain = ndk / "toolchains/llvm/prebuilt" / host
    sysroot = toolchain / "sysroot"
    triple = "aarch64-linux-android"
    env = os.environ.copy()
    for key in list(env):
        if key in {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH"} or key.startswith(("QT_", "QML", "LVRS_", "DYLD_", "LD_")):
            env.pop(key, None)
    with tempfile.TemporaryDirectory(prefix=".android-link-input-", dir=root / "resources") as inputs, \
         tempfile.TemporaryDirectory(prefix="android-link-build-", dir=args.build) as outputs:
        source, build = Path(inputs), Path(outputs) / "build"
        (source / "Main.lvui").write_text(
            'Application { Window { VStack { Label(text: "한글 — مرحبا 🙂")\n'
            'InputField(id: "editor", text: "abc אבג")\n'
            'CodeEditor(id: "code", text: "abc", width: 240, height: 120) } } }\n', encoding="utf-8")
        (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.31)
project(LVUIAndroidConsumer LANGUAGES CXX)
find_package(LVUI 0.1 CONFIG REQUIRED COMPONENTS Native)
lvui_add_application(LVUIAndroid SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/Main.lvui")
target_link_options(LVUIAndroid PRIVATE "LINKER:--no-undefined")
''')
        command = ["cmake", "-S", source, "-B", build, "-G", "Ninja",
                   f"-DCMAKE_TOOLCHAIN_FILE={ndk / 'build/cmake/android.toolchain.cmake'}",
                   "-DANDROID_ABI=arm64-v8a", "-DANDROID_PLATFORM=android-24",
                   "-DANDROID_STL=c++_shared", "-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=BOTH",
                   f"-DiiSDKCore_DIR={args.core / 'lib/cmake/iiSDKCore'}",
                   f"-DiiColorManager_DIR={args.color / 'lib/cmake/iiColorManager'}",
                   f"-DiiGUI_DIR={args.gui / 'lib/cmake/iiGUI'}",
                   f"-DLVUI_DIR={args.lvui / 'lib/cmake/LVUI'}",
                   f"-DLVUI_HOST_COMPILER={args.host_compiler.resolve()}",
                   f"-DVulkan_INCLUDE_DIR={sysroot / 'usr/include'}",
                   f"-DVulkan_LIBRARY={sysroot / 'usr/lib' / triple / '24/libvulkan.so'}"]
        run(command, env)
        run(["cmake", "--build", build, "--parallel", "4"], env)
        app = build / "libLVUIAndroid.so"
        suffix = ".exe" if os.name == "nt" else ""
        readelf, nm = toolchain / ("bin/llvm-readelf" + suffix), toolchain / ("bin/llvm-nm" + suffix)
        header = run([readelf, "-h", app], env)
        if "AArch64" not in header or not re.search(r"Type:\s+DYN", header):
            raise RuntimeError("The Android app is not an arm64 shared library")
        exported = run([nm, "-D", "--defined-only", app], env)
        for symbol in ("ANativeActivity_onCreate", "android_main"):
            if not re.search(r"\b" + symbol + r"$", exported, re.M):
                raise RuntimeError("Missing Android entry symbol: " + symbol)
        symbols = run([nm, "-C", "--defined-only", app], env)
        if forbidden_framework_symbols(symbols):
            raise RuntimeError("Qt, QML, or LVRS symbols were linked statically into the Android app")
        if "ii::runtime::configure_application(" not in symbols:
            raise RuntimeError("Missing generated application configuration")
        required = {"ii::runtime::Application::exec(", "ii::gui::Host::run(",
                    "ii::gui::detail::rasterize_text(", "ii::gui::detail::native_text_line(",
                    "ii::gui::TextLine::wrap("}
        missing = {symbol for symbol in required if symbol not in symbols}
        if re.search(r"lvui::(?:parse|parse_view|compile_native)\(", symbols):
            raise RuntimeError("The compiler or parser was linked into the Android app")

        system = {"libandroid.so", "libc.so", "libdl.so", "liblog.so", "libm.so",
                  "libvulkan.so", "libz.so", "libjnigraphics.so"}
        search = [args.core / "lib", args.color / "lib", args.gui / "lib", args.lvui / "lib",
                  sysroot / "usr/lib" / triple]
        pending, visited, system_dependencies = [app], set(), set()
        keyboard_bridge = False
        ime_bridge = False
        while pending:
            library = pending.pop()
            if library.name in visited:
                continue
            visited.add(library.name)
            if library == app or library.name.startswith("libiiGUI"):
                data = library.read_bytes()
                keyboard_bridge |= b"android/view/KeyCharacterMap" in data and b"getDeadChar" in data
                ime_bridge |= b"com.iisacc.iigui.AndroidImeBridge" in data
            if library != app and library.name.startswith(("libiiGUI", "libiiSDKCore", "libLVUI", "libiiColorManager")):
                defined = run([nm, "-C", "--defined-only", library], env)
                if forbidden_framework_symbols(defined):
                    raise RuntimeError("Qt, QML, or LVRS symbols were linked into " + library.name)
                missing = {symbol for symbol in missing if symbol not in defined}
            dynamic = run([readelf, "-d", library], env)
            for dependency in re.findall(r"Shared library: \[([^]]+)\]", dynamic):
                if re.search(r"(?:qt|qml|lvrs|python|jvm)", dependency, re.I):
                    raise RuntimeError("Unexpected framework or interpreter dependency: " + dependency)
                if dependency in system:
                    system_dependencies.add(dependency)
                    continue
                resolved = next((folder / dependency for folder in search if (folder / dependency).is_file()), None)
                if not resolved:
                    raise RuntimeError("Missing transitive Android dependency: " + dependency)
                pending.append(resolved)
        if missing:
            raise RuntimeError("Missing native application implementation: " + ", ".join(sorted(missing)))
        if not keyboard_bridge:
            raise RuntimeError("The Android hardware keyboard bridge is absent from the app dependency closure")
        if not ime_bridge:
            raise RuntimeError("The Android input-method bridge is absent from the app dependency closure")
        if "libjnigraphics.so" not in system_dependencies:
            raise RuntimeError("The Android native bitmap library is absent from the dependency closure")
        if "libiiSDKCoreObservation.so" not in visited or "libc++_shared.so" not in visited:
            raise RuntimeError("The Android shared dependency closure was not inspected")
        # Exercise the installed example itself, including its opt-in source
        # fixtures. This catches missing test sources and private SDK headers.
        example = args.gui / "share/iiGUI/examples/HelloAndroid"
        example_build = Path(outputs) / "example-build"
        example_command = list(command)
        example_command[2], example_command[4] = example, example_build
        example_command += ["-DIIGUI_ANDROID_OS_SMOKE=ON", "-DIIGUI_ANDROID_RENDER_SMOKE=ON"]
        run(example_command, env)
        run(["cmake", "--build", example_build, "--parallel", "4"], env)
        example_symbols = run([nm, "-C", "--defined-only", example_build / "libhello_lvui.so"], env)
        for symbol in ("verify_android_os(", "verify_android_retained_labels("):
            if symbol not in example_symbols:
                raise RuntimeError("Installed Android example omitted an optional fixture: " + symbol)
    print("Generated LVUI Android app and installed smoke example linked; dependency closure verified; no app executed")


if __name__ == "__main__":
    main()

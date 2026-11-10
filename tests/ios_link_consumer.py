"""Link a real generated LVUI iOS app against supplied iOS SDK stages, without running it."""
from pathlib import Path
import argparse
import os
import plistlib
import subprocess
import tempfile


def command(args, env):
    result = subprocess.run(args, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env)
    if result.returncode:
        raise RuntimeError("Command failed:\n" + result.stdout)
    return result.stdout


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--core", type=Path, required=True)
    parser.add_argument("--gui", type=Path, required=True)
    parser.add_argument("--lvui", type=Path, required=True)
    parser.add_argument("--color", type=Path, required=True)
    parser.add_argument("--vulkan-include", type=Path, required=True)
    parser.add_argument("--vulkan-library", type=Path, required=True)
    parser.add_argument("--host-compiler", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    env = os.environ.copy()
    for key in list(env):
        if key in {"CPATH", "CPLUS_INCLUDE_PATH", "C_INCLUDE_PATH", "LIBRARY_PATH", "CMAKE_PREFIX_PATH"} or key.startswith(("QT_", "QML", "LVRS_", "DYLD_", "LD_")):
            env.pop(key, None)
    sdk = command(["xcrun", "--sdk", "iphoneos", "--show-sdk-path"], env).strip()
    with tempfile.TemporaryDirectory(prefix=".ios-link-input-", dir=root / "resources") as resource, \
         tempfile.TemporaryDirectory(prefix="ios-link-build-", dir=args.build) as output:
        resource, output = Path(resource), Path(output) / "build"
        (resource / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.31)
project(LVUIIOSConsumer VERSION 1.0 LANGUAGES CXX)
find_package(iiSDKCore 0.20 CONFIG REQUIRED COMPONENTS Runtime Observation)
find_package(iiGUI 0.5.0 CONFIG REQUIRED)
find_package(LVUI 0.1.0 CONFIG REQUIRED COMPONENTS Native)
lvui_add_application(LVUIIOSConsumer SOURCE [==[{(root / 'tests/fixtures/NativeMultiWindow.lvui').as_posix()}]==]
    INCLUDES NativeApplicationBackend.h)
target_include_directories(LVUIIOSConsumer PRIVATE [==[{(root / 'tests/fixtures').as_posix()}]==])
lvui_add_application(LVUIIOSTextConsumer SOURCE [==[{(root / 'tests/fixtures/NativeUIKitTextApplication.lvui').as_posix()}]==]
    INCLUDES NativeOwnedCodeEditorFixture.h)
target_include_directories(LVUIIOSTextConsumer PRIVATE [==[{(root / 'tests/fixtures').as_posix()}]==])
set_target_properties(LVUIIOSConsumer PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER com.iisacc.lvui.ios-test
    MACOSX_BUNDLE_BUNDLE_NAME LVUIIOSConsumer MACOSX_BUNDLE_SHORT_VERSION_STRING 1.0 MACOSX_BUNDLE_BUNDLE_VERSION 1)
set_target_properties(LVUIIOSTextConsumer PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER com.iisacc.lvui.ios-text-test
    MACOSX_BUNDLE_BUNDLE_NAME LVUIIOSTextConsumer MACOSX_BUNDLE_SHORT_VERSION_STRING 1.0 MACOSX_BUNDLE_BUNDLE_VERSION 1)
''')
        command(["cmake", "-S", str(resource), "-B", str(output), "-G", "Ninja", "-DCMAKE_SYSTEM_NAME=iOS",
                 f"-DCMAKE_OSX_SYSROOT={sdk}", "-DCMAKE_OSX_ARCHITECTURES=arm64", "-DCMAKE_OSX_DEPLOYMENT_TARGET=18.0",
                 "-DCMAKE_BUILD_TYPE=Release", f"-DiiSDKCore_DIR={args.core}", f"-DiiGUI_DIR={args.gui}",
                 f"-DLVUI_DIR={args.lvui}", f"-DiiColorManager_DIR={args.color}",
                 f"-DVulkan_INCLUDE_DIR={args.vulkan_include}", f"-DVulkan_LIBRARY={args.vulkan_library}",
                 f"-DLVUI_HOST_COMPILER={args.host_compiler}", "-DCMAKE_DISABLE_FIND_PACKAGE_Qt5=ON",
                 "-DCMAKE_DISABLE_FIND_PACKAGE_Qt6=ON", "-DCMAKE_DISABLE_FIND_PACKAGE_LVRS=ON"], env)
        command(["cmake", "--build", str(output), "--parallel", "4"], env)
        bundle = output / "LVUIIOSConsumer.app"
        binary = bundle / "LVUIIOSConsumer"
        with (bundle / "Info.plist").open("rb") as stream:
            metadata = plistlib.load(stream)
        manifest = metadata.get("UIApplicationSceneManifest", {})
        configurations = manifest.get("UISceneConfigurations", {}).get("UIWindowSceneSessionRoleApplication", [])
        if (manifest.get("UIApplicationSupportsMultipleScenes") is not True or
                not any(value.get("UISceneDelegateClassName") == "IIGUIIOSScene" for value in configurations)):
            raise RuntimeError("Linked LVUI iOS app is missing its multi-scene manifest")
        dependencies = command(["xcrun", "otool", "-L", str(binary)], env)
        if any(word in dependencies.lower() for word in ("qtcore", "qtgui", "qml", "lvrs")):
            raise RuntimeError("Generated iOS app has an unexpected direct Qt or LVRS dependency")
        platform = command(["xcrun", "vtool", "-show-build", str(binary)], env)
        if "PLATFORM IOS" not in platform.upper():
            raise RuntimeError("Generated application was not linked for iPhoneOS: " + platform)
        symbols = command(["xcrun", "nm", str(binary)], env)
        for cls in ("IIGUIIOSScene", "IIGUIIOSApplication"):
            if "OBJC_CLASS_$_" + cls not in symbols:
                raise RuntimeError("Linked iOS application does not contain " + cls)
        text_binary = output / "LVUIIOSTextConsumer.app" / "LVUIIOSTextConsumer"
        text_symbols = command(["xcrun", "nm", str(text_binary)], env)
        for method in ("setMarkedText:selectedRange:", "setSelectedTextRange:", "selectionRectsForRange:",
                       "caretRectForPosition:", "deleteBackward"):
            if "IIGUIIOSSurface(UIKitTextInput) " + method not in text_symbols:
                raise RuntimeError("Linked iOS text consumer is missing UIKit method " + method)
        text_dependencies = command(["xcrun", "otool", "-L", str(text_binary)], env)
        if any(word in text_dependencies.lower() for word in ("qtcore", "qtgui", "qml", "lvrs")):
            raise RuntimeError("Generated iOS text app has an unexpected direct Qt or LVRS dependency")
    print("Generated LVUI multi-window and text iPhoneOS apps linked against staged iiSDK and MoltenVK; no app executed")


if __name__ == "__main__":
    main()

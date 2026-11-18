"""Exercise Web target configuration without executing an application."""
from pathlib import Path
import argparse
from html.parser import HTMLParser
import subprocess
import tempfile


def check_example_shell(root):
    class ShellParser(HTMLParser):
        def __init__(self):
            super().__init__()
            self.canvas = None
            self.viewport = False

        def handle_starttag(self, tag, attrs):
            values = dict(attrs)
            if tag == "canvas" and values.get("id") == "canvas":
                self.canvas = values
            if tag == "meta" and values.get("name") == "viewport":
                self.viewport = "width=device-width" in values.get("content", "")

    shell = ShellParser()
    shell.feed((root / "resources/platform/web/shell.html").read_text())
    if (not shell.viewport or shell.canvas is None or "data-iigui-respect-minimum" in shell.canvas
            or "data-iigui-accessibility" not in shell.canvas):
        raise AssertionError("Web example shell must use fluid sizing, expose accessibility and declare its viewport")


def configure(source, output, helper, *, web=True, duplicate=False, missing=False, twice=False, empty=False, bad_destination=False):
    source.mkdir(parents=True)
    (source / "main.cpp").write_text("int main() { return 0; }\n")
    (source / "shell file.html").write_text('<canvas id="canvas"></canvas>{{{ SCRIPT }}}')
    (source / "first").mkdir()
    (source / "second").mkdir()
    (source / "first/font file.ttf").write_text("font")
    (source / "second/font file.ttf").write_text("other font")
    resources = '' if empty else 'RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/first/font file.ttf"'
    if duplicate:
        resources += ' "${CMAKE_CURRENT_SOURCE_DIR}/second/font file.ttf"'
    if missing:
        resources += ' "${CMAKE_CURRENT_SOURCE_DIR}/missing.ttf"'
    again = 'lvui_configure_web(App SHELL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shell file.html")' if twice else ''
    destination = '../escape' if bad_destination else 'web'
    if empty:
        assertions = '''if(joined MATCHES "--preload-file")
    message(FATAL_ERROR "Unexpected preload for a resource-free app")
endif()'''
    else:
        assertions = '''if(NOT "${CMAKE_CURRENT_SOURCE_DIR}/first/font file.ttf" IN_LIST dependencies OR
   NOT joined MATCHES "--preload-file" OR NOT joined MATCHES "font file.ttf@/resources/font file.ttf")
    message(FATAL_ERROR "Web resource was omitted: ${joined}")
endif()'''
    (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.31)
project(WebTargetProbe LANGUAGES CXX)
set(EMSCRIPTEN {"ON" if web else "OFF"})
include([==[{helper.as_posix()}]==])
add_executable(App main.cpp)
set_target_properties(App PROPERTIES LVUI_APPLICATION TRUE)
lvui_configure_web(App SHELL_FILE "${{CMAKE_CURRENT_SOURCE_DIR}}/shell file.html" {resources})
{again}
lvui_install_web(App DESTINATION {destination})
get_target_property(suffix App SUFFIX)
if(NOT suffix STREQUAL ".html")
    message(FATAL_ERROR "HTML output missing")
endif()
get_target_property(options App LINK_OPTIONS)
get_target_property(dependencies App LINK_DEPENDS)
if(NOT "${{CMAKE_CURRENT_SOURCE_DIR}}/shell file.html" IN_LIST dependencies)
    message(FATAL_ERROR "Shell was omitted from link dependencies")
endif()
string(JOIN " " joined ${{options}})
if(NOT joined MATCHES "--shell-file")
    message(FATAL_ERROR "Web link options omitted the shell")
endif()
{assertions}
''')
    return subprocess.run(["cmake", "-S", str(source), "-B", str(output), "-G", "Ninja"],
                          text=True, capture_output=True, timeout=60)


def configure_examples(root, inputs, output):
    """Check that the Web project packages the existing view/model pairs."""
    packages = inputs / "packages"
    packages.mkdir()
    (inputs / "main.cpp").write_text("int main() { return 0; }\n")
    (packages / "LVUIConfig.cmake").write_text('''
function(lvui_add_application target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "NAME;SOURCE;CONFIGURE_SOURCE" "INCLUDES")
    if(NOT EXISTS "${ARG_SOURCE}" OR NOT EXISTS "${ARG_CONFIGURE_SOURCE}")
        message(FATAL_ERROR "Web example is missing an existing view or application entry")
    endif()
    add_executable("${target}" "${LVUI_TEST_MAIN_SOURCE}")
    file(APPEND "${CMAKE_BINARY_DIR}/web-targets.txt"
        "${target}|${ARG_NAME}|${ARG_SOURCE}|${ARG_INCLUDES}\\n")
endfunction()
function(lvui_configure_web target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "SHELL_FILE" "RESOURCES")
    if(NOT TARGET "${target}" OR NOT EXISTS "${ARG_SHELL_FILE}")
        message(FATAL_ERROR "Web example lacks a target or shell")
    endif()
endfunction()
function(lvui_install_web target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Web example install references a missing target")
    endif()
endfunction()
''')
    (packages / "iiGUIConfig.cmake").write_text('set(iiGUI_NATIVE_RESOURCE_FILES "")\n')
    for package, version in (("LVUI", "0.1"), ("iiGUI", "0.5")):
        (packages / f"{package}ConfigVersion.cmake").write_text(
            f'set(PACKAGE_VERSION "{version}")\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n')
    result = subprocess.run([
        "cmake", "-S", str(root / "example/Web"), "-B", str(output), "-G", "Ninja",
        "-DEMSCRIPTEN=ON", f"-DLVUI_DIR={packages}", f"-DiiGUI_DIR={packages}",
        f"-DLVUI_TEST_MAIN_SOURCE={inputs / 'main.cpp'}",
    ], text=True, capture_output=True, timeout=60)
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    entries = [line.split("|") for line in (output / "web-targets.txt").read_text().splitlines()]
    actual = {name: (view, Path(source).name, includes) for name, view, source, includes in entries}
    expected = {
        "LVUICounterWeb": ("CppViewModelCounter", "CppViewModelCounter.lvui", "CppViewModelCounter.h"),
        "LVUIReadingListWeb": ("ReadingList", "ReadingList.lvui", "ReadingListBackend.h"),
        "LVUIInventoryWeb": ("Inventory", "Inventory.lvui", "InventoryBackend.h"),
        "LVUIControlsFixtureWeb": ("NativeWindowAccentControls", "NativeWindowAccentControls.lvui", "NativeWindowAccentControlsFixture.h"),
        "LVUIRadioRowsFixtureWeb": ("NativeOwnedRadioRows", "NativeOwnedRadioRows.lvui", "NativeOwnedRadioButtonFixture.h"),
        "LVUIToggleFixtureWeb": ("NativeOwnedToggleSwitch", "NativeOwnedToggleSwitch.lvui", "NativeOwnedToggleSwitchFixture.h"),
        "LVUISliderFixtureWeb": ("NativeOwnedSlider", "NativeOwnedSlider.lvui", "NativeOwnedSliderFixture.h"),
    }
    if actual != expected:
        raise AssertionError(f"Web examples do not cover the expected existing views/models: {actual}")


def configure_authoring(source, output, helper, *, web=True, custom=False, no_fonts=False,
                        missing_fonts=False, duplicate=False):
    """The single-call API must preserve the AOT, iiGUI asset and install contracts."""
    source.mkdir(parents=True)
    (source / "App.lvui").write_text("Application { Window {} }\n")
    (source / "model.h").write_text("#pragma once\n")
    (source / "extra.cpp").write_text("int extra = 0;\n")
    (source / "bootstrap.cpp").write_text("int bootstrap = 0;\n")
    (source / "font.ttf").write_text("font")
    (source / "shell.html").write_text('<canvas id="canvas"></canvas>{{{ SCRIPT }}}')
    (source / "asset.dat").write_text("asset")
    shell = 'SHELL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"' if custom else ''
    bootstrap = 'CONFIGURE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/bootstrap.cpp"' if custom else ''
    options = 'NO_DEFAULT_FONTS' if no_fonts else ''
    assets = '"${CMAKE_CURRENT_SOURCE_DIR}/asset.dat"'
    if duplicate:
        assets += ' "${CMAKE_CURRENT_SOURCE_DIR}/font.ttf"'
    fonts = '"${CMAKE_CURRENT_SOURCE_DIR}/missing.ttf"' if missing_fonts else '"${CMAKE_CURRENT_SOURCE_DIR}/font.ttf"'
    expected_shell = '${CMAKE_CURRENT_SOURCE_DIR}/shell.html' if custom else str(
        helper.parent.parent / "resources/platform/web/shell.html")
    font_assertion = '''if("${CMAKE_CURRENT_SOURCE_DIR}/font.ttf" IN_LIST dependencies)
    message(FATAL_ERROR "NO_DEFAULT_FONTS still preloaded iiGUI fonts")
endif()''' if no_fonts else '''if(NOT "${CMAKE_CURRENT_SOURCE_DIR}/font.ttf" IN_LIST dependencies)
    message(FATAL_ERROR "Default iiGUI resource missing")
endif()'''
    bootstrap_assertion = '''if(NOT "${CMAKE_CURRENT_SOURCE_DIR}/bootstrap.cpp" IN_LIST sources)
    message(FATAL_ERROR "Custom application bootstrap was not forwarded")
endif()''' if custom else ''
    (source / "CMakeLists.txt").write_text(f'''cmake_minimum_required(VERSION 3.31)
project(WebAuthoringProbe LANGUAGES CXX)
set(EMSCRIPTEN {"ON" if web else "OFF"})
set(LVUI_HOST_COMPILER "${{CMAKE_CURRENT_SOURCE_DIR}}/extra.cpp")
set(iiGUI_NATIVE_RESOURCES {fonts})
add_library(LVUI::Native INTERFACE IMPORTED)
add_library(iiGUI::iiGUI INTERFACE IMPORTED)
add_library(iiSDKCore::Runtime INTERFACE IMPORTED)
function(iisdkcore_add_application target)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "GUI" "" "SOURCES;LIBRARIES")
    add_executable("${{target}}" ${{ARG_SOURCES}})
    target_link_libraries("${{target}}" PRIVATE ${{ARG_LIBRARIES}})
endfunction()
include([==[{helper.as_posix()}]==])
lvui_add_web_application(App NAME Example SOURCE App.lvui INCLUDES model.h
    SOURCES extra.cpp OUTPUT_VARIABLE generated INSTALL_DESTINATION web {shell} {bootstrap} {options}
    RESOURCES {assets})
if(NOT generated STREQUAL "${{CMAKE_CURRENT_BINARY_DIR}}/Example.generated.h")
    message(FATAL_ERROR "Generated output was not returned: ${{generated}}")
endif()
get_target_property(suffix App SUFFIX)
get_target_property(dependencies App LINK_DEPENDS)
get_target_property(include_dirs App INCLUDE_DIRECTORIES)
get_target_property(sources App SOURCES)
if(NOT suffix STREQUAL ".html" OR
   NOT "{expected_shell}" IN_LIST dependencies OR
   NOT "${{CMAKE_CURRENT_SOURCE_DIR}}/asset.dat" IN_LIST dependencies OR
   NOT "${{CMAKE_CURRENT_SOURCE_DIR}}" IN_LIST include_dirs OR
   NOT "extra.cpp" IN_LIST sources)
    message(FATAL_ERROR "Web authoring inputs were not forwarded")
endif()
{font_assertion}
{bootstrap_assertion}
''')
    return subprocess.run(["cmake", "-S", str(source), "-B", str(output), "-G", "Ninja"],
                          text=True, capture_output=True, timeout=60)


def compile_declarative_example(root, compiler):
    """The shipped example must remain an application with a reactive C++ model."""
    example = root / "example/Web/Declarative"
    result = subprocess.run([str(compiler), "--name", "DeclarativeExample", "--include", "WebCounterModel.h",
                             str(example / "Main.lvui")],
                            text=True, capture_output=True, timeout=60)
    if result.returncode:
        raise AssertionError(result.stderr)
    for fragment in ('#include "WebCounterModel.h"', '::WebCounterModel backend_',
                     'configure_application(ii::runtime::Application&', '.increment(', '.reset('):
        if fragment not in result.stdout:
            raise AssertionError(f"Declarative Web example lost generated application or binding: {fragment}")


def compile_browser_demo(root, compiler):
    """The browser demo must generate a view with a live C++ counter model."""
    example = root / "example/Web/BrowserDemo"
    result = subprocess.run([str(compiler), "--name", "BrowserDemo", "--include", "BrowserDemoModel.h",
                             str(example / "Main.lvui")],
                            text=True, capture_output=True, timeout=60)
    if result.returncode:
        raise AssertionError(result.stderr)
    for fragment in ('#include "BrowserDemoModel.h"', '::BrowserDemoModel backend_',
                     'configure_application(ii::runtime::Application&', '.increment(', '.reset('):
        if fragment not in result.stdout:
            raise AssertionError(f"Browser demo lost its generated application or binding: {fragment}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--compiler", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    check_example_shell(root)
    with tempfile.TemporaryDirectory(prefix=".web-config-", dir=root / "resources") as inputs, \
         tempfile.TemporaryDirectory(prefix="web-config-", dir=args.build) as outputs:
        inputs, outputs = Path(inputs), Path(outputs)
        for name, kwargs, expected in [
            ("valid", {}, None),
            ("empty", {"empty": True}, None),
            ("nonweb", {"web": False}, "Emscripten"),
            ("duplicate", {"duplicate": True}, "duplicate"),
            ("missing", {"missing": True}, "resource"),
            ("twice", {"twice": True}, "already"),
            ("bad_destination", {"bad_destination": True}, "relative path"),
        ]:
            result = configure(inputs / name, outputs / name, root / "cmake/LVUIAot.cmake", **kwargs)
            log = result.stdout + result.stderr
            if expected is None:
                if result.returncode:
                    raise AssertionError(log)
                install = (outputs / name / "cmake_install.cmake").read_text()
                for artifact in ("App.html", "App.js", "App.wasm"):
                    if artifact not in install:
                        raise AssertionError(f"{artifact} omitted from install rules")
                if ("App.data" in install) == kwargs.get("empty", False):
                    raise AssertionError("Data archive installation does not match the resource list")
            elif result.returncode == 0 or expected.lower() not in log.lower():
                raise AssertionError(f"{name} did not reject invalid configuration: {log}")
        configure_examples(root, inputs, outputs / "examples")
        for name, kwargs, expected in [
            ("default", {}, None),
            ("custom", {"custom": True, "no_fonts": True}, None),
            ("nonweb", {"web": False}, "Emscripten"),
            ("missing_fonts", {"missing_fonts": True}, "resource"),
            ("duplicate", {"duplicate": True}, "duplicate"),
        ]:
            result = configure_authoring(inputs / ("author-" + name), outputs / ("author-" + name),
                                         root / "cmake/LVUIAot.cmake", **kwargs)
            log = result.stdout + result.stderr
            if expected is None:
                if result.returncode:
                    raise AssertionError(log)
                install = (outputs / ("author-" + name) / "cmake_install.cmake").read_text()
                for artifact in ("App.html", "App.js", "App.wasm", "App.data"):
                    if artifact not in install:
                        raise AssertionError(f"Web authoring install omitted {artifact}")
            elif result.returncode == 0 or expected.lower() not in log.lower():
                raise AssertionError(f"Authoring {name} did not reject invalid configuration: {log}")
        if args.compiler:
            compile_declarative_example(root, args.compiler.resolve())
            compile_browser_demo(root, args.compiler.resolve())
    print("Web target options, assets, installation and diagnostics verified; no application executed")


if __name__ == "__main__":
    main()
